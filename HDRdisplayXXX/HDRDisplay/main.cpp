// Copyright(c) 2016, NVIDIA CORPORATION.All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met :
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and / or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "common_util.h"

#include <vector>
#include "PerfTracker.h"

#include "AntTweakBar.h"
#include "DeviceManager.h"

#include "tonemapper.h"
#include "patternGenerator.h"
#include "inputTransform.h"
#include "acesTonemapper.h"
#include "compositor.h"
#include "Exposure.h"

#include "rgbe.h"

#include <d3dcommon.h>
#include <dxgi.h>
#include <d3d11.h>
#include "shaderCompile.h"
#include <ctime>
#include <map>

#include <ImfRgbaFile.h>
#include <ImfStringAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfArray.h>

#include "uhdDisplay.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////////////////////////

DeviceManager* g_device_manager = NULL;

bool g_bRenderHUD = true;


typedef enum eViewMode 
{
	VIEW_MODE_ACES_LDR = 0,
	VIEW_MODE_RANGE = 1,
	VIEW_MODE_LINEAR = 2,
	VIEW_MODE_ACES_PARAM = 6,

	VIEW_MODE_LUT = 7,

	VIEW_MODE_REINHARD = 8,

	VIEW_MODE_INVALID = 0xffffffff
};

#define SIMPLE_TONEMAP_DECL( ClassName, Shader) \
class ClassName : public SimpleTonemapper  \
{ \
	public: \
	ClassName(ID3D11Device *device) : SimpleTonemapper(device, Shader) {} \
	~##ClassName(){} \
};

SIMPLE_TONEMAP_DECL(VisRange_class, "vis_range.hlsl")

struct TonemapCreator
{
	const char* Label;
	eViewMode Mode;

	virtual Tonemapper* Create(ID3D11Device* device) = 0;

	TonemapCreator(eViewMode inMode, const char* inLabel) :
		Label(inLabel), Mode(inMode)
	{}
};


template <typename T>
struct TemplateTonemapCreator : public TonemapCreator
{
	virtual Tonemapper* Create(ID3D11Device* device) override
	{
		return new T(device);
	}

	TemplateTonemapCreator(eViewMode inMode, const char* inLabel) :
		TonemapCreator(inMode, inLabel)
	{}
};

TemplateTonemapCreator<VisRange_class> VisRange( VIEW_MODE_RANGE, "Display Range" );
TemplateTonemapCreator<Linear> LinearTonemap( VIEW_MODE_LINEAR, "Linear" );
TemplateTonemapCreator<ParameterizedACES> AcesParameterized( VIEW_MODE_ACES_PARAM, "ACES Parameterized" );
TemplateTonemapCreator<LutACES> AcesLut( VIEW_MODE_LUT, "ACES LUT" );
TemplateTonemapCreator<Reinhard> ReinhardTM(VIEW_MODE_REINHARD, "Reinhard");

TonemapCreator* TonemapperList[] = 
{
	&VisRange,
	&LinearTonemap,
	&AcesParameterized,
	&AcesLut,
	&ReinhardTM
};

typedef enum eDisplayPrimaries
{
	PRIM_REC709 = 0,
	PRIM_DCIP3 = 1,
	PRIM_BT2020 = 2,
	PRIM_P3D60 = 3,
	PRIM_ADOBE98 = 4,

	PRIM_INVALID = 0xffffffff
};


// WCC
// eViewMode g_view_mode = VIEW_MODE_ACES_PARAM;
eViewMode g_view_mode = VIEW_MODE_LINEAR;

int g_Width = 0;
int g_Height = 0;
bool g_HDRon = false;

float g_Red = 0.0f, g_Green = 0.0f, g_Blue = 0.0f;

int g_MouseX = 0, g_MouseY = 0;

std::vector<std::string> g_Textures;

unsigned int g_tex_index = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Chromacities for setting up UHD monitor metadata
////////////////////////////////////////////////////////////////////////////////////////////////////

// must match ordering of eDisplayPrimaries
const Chromaticities chromacityList[] =
{
	{ 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // rec709
	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31400f, 0.35100f }, // DCI-P3
	{ 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // bt2020
	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.32168f, 0.33767f }, // D60 DCI
	{ 0.64000f, 0.33000f, 0.21000f, 0.71000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Adobe98
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Scene Controller
////////////////////////////////////////////////////////////////////////////////////////////////////

class SceneController : public IVisualController
{
private:

	struct HDRTexture
	{
		ID3D11Texture2D *texPtr;
		ID3D11ShaderResourceView *srvPtr;
		int width, height;
	};

	ID3D11Buffer*				quad_verts;
	ID3D11InputLayout*			quad_layout;
	ID3D11VertexShader*			quad_vs;

	std::map<eViewMode,Tonemapper*>    tonemappers;

	//xform pre-pass
	XformPass*					xform;

	// LDR split screen tonemapper
	Tonemapper*					ldr;

	// Pattern generator shader
	PatternGen*					patternGen;

	// compositor
	Compositor*					compositor;

	ID3D11RasterizerState*		rs_state;

	ID3D11SamplerState*			samp_linear_wrap;

	ID3D11DepthStencilState*	ds_state;
	ID3D11DepthStencilState*	ds_state_disabled;
	
	ID3D11BlendState*			blend_state_disabled;

	// intermediate buffers to hold results of transformed texture
	static const int intermediateCount = 4;
	ID3D11Texture2D*			intermediateTex[intermediateCount];
	ID3D11ShaderResourceView*	intermediateSRV[intermediateCount];
	ID3D11RenderTargetView*		intermediateRTV[intermediateCount];

	// buffers to read back data
	ID3D11Buffer*				feedbackBuf;
	ID3D11UnorderedAccessView*  feedbackUAV;
	ID3D11Buffer*				readbackBuf;

	// Texture for test patterns
	ID3D11Texture2D*			patternTex;
	ID3D11ShaderResourceView*	patternSRV;
	ID3D11RenderTargetView*		patternRTV;

	// Auto-exposure controls
	ExposureReduction*			exposurePass;
	ID3D11Buffer*				exposureBuffer;
	ID3D11ShaderResourceView*	exposureSRV;
	ID3D11UnorderedAccessView*	exposureUAV;

	std::vector<HDRTexture>     textures;

	DXGI_SURFACE_DESC			surface_desc;

	TwBar*                      tonemapperSettings;
	eViewMode                   activeTonemapper;

	float						internalTime;

	bool CreateImmutableTexture(ID3D11Device* device, DXGI_FORMAT format, int width, int height, const D3D11_SUBRESOURCE_DATA *data, ID3D11Texture2D **tex, ID3D11ShaderResourceView **srv)
	{
		D3D11_TEXTURE2D_DESC tex_desc;
		ZeroMemory(&tex_desc, sizeof(tex_desc));
		tex_desc.ArraySize = 1;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		tex_desc.Format = format;
		tex_desc.Width = width;
		tex_desc.Height = height;
		tex_desc.MipLevels = 1;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.Usage = D3D11_USAGE_IMMUTABLE;

		HRESULT hr = device->CreateTexture2D(&tex_desc, data, tex);

		if (FAILED(hr))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;

		ZeroMemory(&srv_desc, sizeof(srv_desc));
		srv_desc.Format = format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = 0;

		hr = device->CreateShaderResourceView(*tex, &srv_desc, srv);

		if (FAILED(hr))
		{
			(*tex)->Release();
			return false;
		}
		return true;
	}

	bool CreateEXRTexture(ID3D11Device* device, const std::string& texName, HDRTexture& texStruct)
	{
		try
		{
			Imf_2_2::Array2D<Imf_2_2::Rgba> pixels;
			int width;
			int height;

			// Read exr file using simple OpenEXR path
			Imf_2_2::RgbaInputFile file(texName.c_str());
			Imath_2_2::Box2i dw = file.dataWindow();

			width = dw.max.x - dw.min.x + 1;
			height = dw.max.y - dw.min.y + 1;
			pixels.resizeErase(height, width);

			file.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * width, 1, width);
			file.readPixels(dw.min.y, dw.max.y);

			ID3D11Texture2D* new_texture = nullptr;
			ID3D11ShaderResourceView *srv = nullptr;

			D3D11_SUBRESOURCE_DATA data;
			ZeroMemory(&data, sizeof(data));
			data.pSysMem = pixels[0];
			data.SysMemPitch = width * 8;

			if (!CreateImmutableTexture(device, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, &data, &new_texture, &srv))
				return false;

			texStruct.texPtr = new_texture;
			texStruct.srvPtr = srv;
			texStruct.width = width;
			texStruct.height = height;
		}
		catch (...)
		{
			return false;
		}
		return true;
	}

	bool CreateHDRTexture(ID3D11Device *device, const std::string &texName, HDRTexture &texStruct)
	{
		int width, height;
		FILE *fp = fopen(texName.c_str(), "rb");

		if (fp)
		{
			rgbe_header_info header;

			if (!RGBE_ReadHeader(fp, &width, &height, &header))
			{

				float* data = new float[width*height * 3];

				if (!RGBE_ReadPixels_RLE(fp, (float*)data, width, height)) {
					float* rgba = new float[width*height * 4];

					//convert to rgba
					for (int i = 0; i < (width*height); i++)
					{
						rgba[i * 4 + 0] = data[i * 3 + 0];
						rgba[i * 4 + 1] = data[i * 3 + 1];
						rgba[i * 4 + 2] = data[i * 3 + 2];
						rgba[i * 4 + 3] = 1.0f;
					}

					ID3D11Texture2D* new_texture = nullptr;
					ID3D11ShaderResourceView *srv = nullptr;

					D3D11_SUBRESOURCE_DATA data;
					ZeroMemory(&data, sizeof(data));
					data.pSysMem = rgba;
					data.SysMemPitch = width * 16;

					if (!CreateImmutableTexture(device, DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, &data, &new_texture, &srv))
						return false;

					texStruct.texPtr = new_texture;
					texStruct.srvPtr = srv;
					texStruct.width = width;
					texStruct.height = height;

					delete[] rgba;

				}
				delete[] data;

			}
			else
			{
				fclose(fp);
				return false;
			}
			fclose(fp);
			return true;
		}
		return false;
	}

public:
	SceneController() :
		tonemapperSettings(nullptr),
		activeTonemapper(VIEW_MODE_INVALID),
		internalTime(0.0f)
	{
		memset(intermediateTex, 0, sizeof(intermediateTex));
		memset(intermediateSRV, 0, sizeof(intermediateSRV));
		memset(intermediateRTV, 0, sizeof(intermediateRTV));
	}


	HRESULT CompileShaderFromFile(LPCSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
	{
		HRESULT hr = S_OK;
		ID3DBlob* pErrorBlob;

		hr = ::CompileShaderFromFile(szFileName, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, ppBlobOut, &pErrorBlob);

		if(FAILED(hr))
		{
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			SAFE_RELEASE(pErrorBlob);
			return hr;
		}
		SAFE_RELEASE(pErrorBlob);

		return S_OK;
	}

	//create the intermediate surface
	void CreateIntermediate( ID3D11Device* device)
	{
		for (int i = 0; i < intermediateCount; i++)
		{
			SAFE_RELEASE(intermediateTex[i]);
			SAFE_RELEASE(intermediateSRV[i]);
			SAFE_RELEASE(intermediateRTV[i]);
		}

		D3D11_TEXTURE2D_DESC tDesc;
		ZeroMemory(&tDesc, sizeof(tDesc));
		tDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		tDesc.Width = g_Width;
		tDesc.Height = g_Height;
		tDesc.ArraySize = 1;
		tDesc.MipLevels = 1;
		tDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		tDesc.SampleDesc.Count = 1;
		tDesc.SampleDesc.Quality = 0;
		tDesc.Usage = D3D11_USAGE_DEFAULT;


		D3D11_SHADER_RESOURCE_VIEW_DESC sDesc;
		ZeroMemory(&sDesc, sizeof(sDesc));
		sDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sDesc.Texture2D.MipLevels = 1;
		sDesc.Texture2D.MostDetailedMip = 0;


		D3D11_RENDER_TARGET_VIEW_DESC rDesc;
		ZeroMemory(&rDesc, sizeof(rDesc));
		rDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		rDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rDesc.Texture2D.MipSlice = 0;

		printf("Creating intermediates at %d x %d\n", g_Width, g_Height);

		for (int i = 0; i < intermediateCount; i++)
		{
			SAFE_RELEASE(intermediateTex[i]);
			SAFE_RELEASE(intermediateRTV[i]);
			SAFE_RELEASE(intermediateSRV[i]);

			device->CreateTexture2D(&tDesc, nullptr, &intermediateTex[i]);

			device->CreateShaderResourceView(intermediateTex[i], &sDesc, &intermediateSRV[i]);

			device->CreateRenderTargetView(intermediateTex[i], &rDesc, &intermediateRTV[i]);
		}

	}

	virtual HRESULT DeviceCreated(ID3D11Device* device)
	{
		HRESULT hr;
		(void)hr;


		{
			// 6 2-element vertices
			float verts[12] =
			{
				-1.f, -1.f,
				-1.f, 1.f,
				1.f, 1.f,
				-1.f, -1.f,
				1.f, 1.f,
				1.f, -1.f
			};
			D3D11_BUFFER_DESC desc = { 12 * sizeof(float), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0 };
			D3D11_SUBRESOURCE_DATA data = { (void *)verts, sizeof(float)*2, 12 * sizeof(float) };
			hr = device->CreateBuffer(&desc, &data, &this->quad_verts);
			_ASSERT(!FAILED(hr));
		}

		ID3DBlob* pShaderBuffer = NULL;
		void* ShaderBufferData;
		SIZE_T  ShaderBufferSize;
		{
			CompileShaderFromFile("vs_quad.hlsl", "main", "vs_5_0", &pShaderBuffer);
			ShaderBufferData = pShaderBuffer->GetBufferPointer();
			ShaderBufferSize = pShaderBuffer->GetBufferSize();
			device->CreateVertexShader(ShaderBufferData, ShaderBufferSize, nullptr, &this->quad_vs);

			D3D11_INPUT_ELEMENT_DESC quad_layout_desc[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			hr = device->CreateInputLayout(quad_layout_desc, sizeof(quad_layout_desc)/sizeof(D3D11_INPUT_ELEMENT_DESC), ShaderBufferData, ShaderBufferSize, &this->quad_layout);

			// create the list of tonemappers from the creator list
			for (auto creator : TonemapperList)
			{
				tonemappers[creator->Mode] = creator->Create(device);
			}

			xform = new XformPass(device);
			xform->InitUI();

			patternGen = new PatternGen(device);
			patternGen->InitUI();

			ldr = new LDR_ss(device);
			ldr->InitUI();

			compositor = new Compositor(device);
			compositor->InitUI();

		}
		{
			D3D11_RASTERIZER_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.CullMode = D3D11_CULL_NONE;
			desc.FillMode = D3D11_FILL_SOLID;
			desc.AntialiasedLineEnable = FALSE;
			desc.DepthBias = 0;
			desc.DepthBiasClamp = 0;
			desc.DepthClipEnable = TRUE;
			desc.FrontCounterClockwise = FALSE;
			desc.MultisampleEnable = TRUE;
			desc.ScissorEnable = TRUE;
			desc.SlopeScaledDepthBias = 0;
			device->CreateRasterizerState(&desc, &this->rs_state);
		}
		{
			D3D11_SAMPLER_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			desc.MipLODBias = 0.0f;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			desc.BorderColor[0] = 0.f; desc.BorderColor[0] = 0.f; desc.BorderColor[0] = 0.f; desc.BorderColor[0] = 0.f;
			desc.MinLOD = 0;
			desc.MaxLOD = D3D11_FLOAT32_MAX;
			device->CreateSamplerState(&desc, &this->samp_linear_wrap);
		}
		{
			D3D11_DEPTH_STENCIL_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.StencilEnable = FALSE;
			desc.DepthEnable = TRUE;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			device->CreateDepthStencilState(&desc, &this->ds_state);
		}
		{
			D3D11_DEPTH_STENCIL_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.StencilEnable = FALSE;
			desc.DepthEnable = FALSE;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			device->CreateDepthStencilState(&desc, &this->ds_state_disabled);
		}
		{
			D3D11_BLEND_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.RenderTarget[0].BlendEnable = FALSE;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			device->CreateBlendState(&desc, &this->blend_state_disabled);
		}
		{
			D3D11_BUFFER_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.ByteWidth = 256;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			device->CreateBuffer(&desc, nullptr, &feedbackBuf);
		}

		{
			D3D11_BUFFER_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.ByteWidth = 256;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; //actually cheating and reading
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			device->CreateBuffer(&desc, nullptr, &readbackBuf);
		}

		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			desc.Buffer.FirstElement = 0;
			desc.Buffer.NumElements = 8;
			desc.Buffer.Flags = 0;

			device->CreateUnorderedAccessView(feedbackBuf, &desc, &feedbackUAV);
		}

		// Data used for auto-exposure
		{
			D3D11_BUFFER_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.ByteWidth = 256;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.StructureByteStride = 0;

			float blob[64] = { 0.0f };
			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = blob;
			data.SysMemPitch = 256;
			data.SysMemSlicePitch = 0;

			device->CreateBuffer(&desc, &data, &exposureBuffer);
		}

		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			desc.Buffer.FirstElement = 0;
			desc.Buffer.NumElements = 8;
			desc.Buffer.Flags = 0;

			device->CreateUnorderedAccessView(exposureBuffer, &desc, &exposureUAV);
		}

		{
			D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			desc.Buffer.FirstElement = 0;
			desc.Buffer.NumElements = 8;

			device->CreateShaderResourceView(exposureBuffer, &desc, &exposureSRV);
		}

		exposurePass = new ExposureReduction(device);

		//create the intermediate surface
		CreateIntermediate(device);

		// Load the exr files from the commandline
		{
			for (auto it = g_Textures.begin(); it < g_Textures.end(); it++)
			{
				std::string &filepath = *it;
				HDRTexture texStruct;

				if (filepath.rfind(".exr") != std::string::npos)
				{
					if (!CreateEXRTexture(device, filepath, texStruct))
					{
						continue;
					}

				}
				else if (filepath.rfind(".hdr") != std::string::npos)
				{

					if (!CreateHDRTexture(device, filepath, texStruct))
					{
						continue;
					}
				}
				else
				{
					//unknown format
					continue;
				}


				textures.push_back(texStruct);
			}
		}

		//pattern texture
		{
			patternTex = nullptr;

			D3D11_TEXTURE2D_DESC tex_desc;
			ZeroMemory(&tex_desc, sizeof(tex_desc));
			tex_desc.ArraySize = 1;
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			tex_desc.Width = patternGen->Width();
			tex_desc.Height = patternGen->Height();
			tex_desc.MipLevels = 1;
			tex_desc.SampleDesc.Count = 1;
			tex_desc.SampleDesc.Quality = 0;
			tex_desc.Usage = D3D11_USAGE_DEFAULT;


			device->CreateTexture2D(&tex_desc, nullptr, &patternTex);

			patternSRV = nullptr;
			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;

			ZeroMemory(&srv_desc, sizeof(srv_desc));
			srv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
			srv_desc.Texture2D.MostDetailedMip = 0;

			hr = device->CreateShaderResourceView(patternTex, &srv_desc, &patternSRV);

			HDRTexture texStruct;

			texStruct.texPtr = patternTex;
			texStruct.srvPtr = patternSRV;
			texStruct.width = patternGen->Width();
			texStruct.height = patternGen->Height();

			textures.push_back(texStruct);
			
			patternRTV = nullptr;
			D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
			rtv_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			rtv_desc.Texture2D.MipSlice = 0;

			hr = device->CreateRenderTargetView(patternTex, &rtv_desc, &patternRTV);
		}

		return S_OK;
	}

	virtual void DeviceDestroyed()
	{
		SAFE_RELEASE(this->quad_verts);
		SAFE_RELEASE(this->quad_layout);
		SAFE_RELEASE(this->quad_vs);
		SAFE_RELEASE(this->rs_state);
		SAFE_RELEASE(this->samp_linear_wrap);
		SAFE_RELEASE(this->ds_state);
		SAFE_RELEASE(this->ds_state_disabled);
		SAFE_RELEASE(this->blend_state_disabled);
		for (int i = 0; i < intermediateCount; i++)
		{
			SAFE_RELEASE(intermediateTex[i]);
			SAFE_RELEASE(intermediateSRV[i]);
			SAFE_RELEASE(intermediateRTV[i]);
		}

		SAFE_RELEASE(feedbackBuf);
		SAFE_RELEASE(readbackBuf);
		SAFE_RELEASE(feedbackUAV);

		SAFE_RELEASE(exposureSRV);
		SAFE_RELEASE(exposureUAV);
		SAFE_RELEASE(exposureBuffer);

		delete exposurePass;
		exposurePass = nullptr;

		// Tex handle and srv taken care of since they are in the texture array
		SAFE_RELEASE(patternRTV);

		for (auto it = textures.begin(); it < textures.end(); it++)
		{
			SAFE_RELEASE(it->texPtr);
			SAFE_RELEASE(it->srvPtr);
		}

		textures.resize(0);

		for (auto it = tonemappers.begin(); it != tonemappers.end(); it++)
		{
			delete it->second;
		}
		tonemappers.clear();

		delete xform;
		xform = nullptr;

		delete ldr;
		ldr = nullptr;

		delete patternGen;
		patternGen = nullptr;
	}

	virtual void BackBufferResized(ID3D11Device* device, const DXGI_SURFACE_DESC* surface_desc)
	{
		this->surface_desc = *surface_desc;

		g_Width = surface_desc->Width;
		g_Height = surface_desc->Height;

		CreateIntermediate(device);
	}

	virtual void Render(ID3D11Device* device, ID3D11DeviceContext* ctx, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV)
	{
		if (activeTonemapper != g_view_mode)
		{
			if (tonemapperSettings)
			{
				TwDeleteBar(tonemapperSettings);
			}
			activeTonemapper = g_view_mode;

			tonemapperSettings = tonemappers[activeTonemapper]->InitUI();

		}


		//readback the last color values
		{
			D3D11_MAPPED_SUBRESOURCE map;
			ctx->Map( readbackBuf, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &map);

			float* data = (float*)map.pData;

			//reading  white mapped NOOVERWRITE is wrong, but it works well enough here
			//it alos avoids extraneous code to manage queued feedback
			g_Red = data[0];
			g_Green = data[1];
			g_Blue = data[2];

			ctx->Unmap(readbackBuf,0);
		}

		{
			{

				float clear_color_scene[4]    = { 1.00f, 1.00f, 1.00f, 0.0f };
				ctx->ClearRenderTargetView(pRTV, clear_color_scene);
				ctx->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

				Tonemapper *tonemapper = tonemappers[activeTonemapper];

				ID3D11ShaderResourceView *srv = nullptr;
				int tWidth = 0, tHeight = 0;
				if (textures.size())
				{
					const HDRTexture &ref = textures[g_tex_index % unsigned int(textures.size())];
					srv = ref.srvPtr;
					tWidth = ref.width;
					tHeight = ref.height;
				}

				// Compute auto-exposure from the image
				exposurePass->Process(ctx, srv, exposureUAV, tWidth, tHeight);

				// Common sampler setup for all shaders
				ctx->PSSetSamplers(0, 1, &samp_linear_wrap);

				// Full screen quad to fill the background
				UINT quad_strides = sizeof(float) * 2;
				UINT quad_offsets = 0;
				ctx->VSSetShader(this->quad_vs, nullptr, 0);
				ctx->IASetInputLayout(this->quad_layout);
				ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				ctx->IASetVertexBuffers(0, 1, &this->quad_verts, &quad_strides, &quad_offsets);
				ctx->OMSetDepthStencilState(this->ds_state_disabled, 0xFF);

				ctx->OMSetBlendState(this->blend_state_disabled, nullptr, 0xFFFFFFFF);
				ctx->RSSetState(this->rs_state);

				D3D11_RECT default_scissor =
				{
					0,
					0,
					g_Width,
					g_Height
				};
				ctx->RSSetScissorRects(1, &default_scissor);

				D3D11_VIEWPORT viewport =
				{
					0.0f, 0.0f, // top left coords
					float(g_Width), float(g_Height), // width height
					0.0f, 1.0f // min/max depth
				};

				// render the test pattern if necessary
				if (patternGen->Dirty())
				{
					viewport.Width = float(patternGen->Width());
					viewport.Height = float(patternGen->Height());
					ctx->OMSetRenderTargets(1, &patternRTV, nullptr);
					ctx->RSSetViewports( 1, &viewport);

					patternGen->SetupTonemapShader(ctx, nullptr);
					ctx->Draw(6, 0);
				}

				viewport.Width = float(g_Width);
				viewport.Height = float(g_Height);
				ctx->RSSetViewports(1, &viewport);

				//Run a pre transform on the scene to allow exposure adjustment and grading tweaks
				ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &intermediateRTV[0], nullptr, 1, 1, &feedbackUAV, nullptr);
				
				xform->SetDimensions( tWidth, tHeight, g_Width, g_Height);
				xform->SetReadback(g_MouseX, g_MouseY);
				
				//have the tonemapper setup its parameters
				xform->SetupTransformPass(ctx, srv, exposureSRV);

				ctx->Draw(6, 0);

				ctx->CopyResource( readbackBuf, feedbackBuf);

				//render the real scene
				ctx->OMSetRenderTargets(1, &pRTV, pDSV);

				//render hdr tonemap
				ctx->OMSetRenderTargets(1, &intermediateRTV[1], pDSV);

				//have the tonemapper setup its parameters
				tonemapper->SetupTonemapShader(ctx, intermediateSRV[0]);

				ctx->Draw(6, 0);

				//render ldr tonemap
				ctx->OMSetRenderTargets(1, &intermediateRTV[2], pDSV);

				ldr->SetupTonemapShader(ctx, intermediateSRV[0]);
				ctx->Draw(6, 0);

				//render composite
				ctx->OMSetRenderTargets(1, &pRTV, pDSV);

				compositor->setScroll(internalTime / 15.0f, 0.0f);

				compositor->SetupShader(ctx, intermediateSRV + 1);

				ctx->Draw(6, 0);

				ctx->RSSetScissorRects(1, &default_scissor);

			}

			// Reset RT and SRV state
			ID3D11ShaderResourceView* nullAttach[16] = {nullptr};
			ctx->PSSetShaderResources(0, 16, nullAttach);
			ctx->OMSetRenderTargets(0, nullptr, nullptr);
		}
	}

	virtual void Animate(double fElapsedTimeSeconds) override
	{
		internalTime = float(fmod(internalTime + fElapsedTimeSeconds, 60.0));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// UI Controller
////////////////////////////////////////////////////////////////////////////////////////////////////

class UIController : public IVisualController
{
private:
	TwBar* settings_bar;
	TwBar* hdr_bar;
	float ui_update_time;
	struct HDRprops
	{
		float maxMasterLum;
		float minMasterLum;
		float maxCLL;
		float maxFALL;
		eDisplayPrimaries DisplayPrimaries;
		bool hdrEnabled;

		HDRprops() :
			maxMasterLum(1000.0f),
			minMasterLum(1.0f),
			maxCLL(1000.0f),
			maxFALL(100.0f),
			DisplayPrimaries(PRIM_REC709),
			hdrEnabled(false)
		{}

		bool operator!=(const HDRprops &lhs)
		{
			//evil check, everything is equal if hdr is off on both
			if ( !hdrEnabled)
			{
				return hdrEnabled != lhs.hdrEnabled;
			}

			return hdrEnabled != lhs.hdrEnabled
				|| DisplayPrimaries != lhs.DisplayPrimaries
				|| maxMasterLum != lhs.maxMasterLum
				|| minMasterLum != lhs.minMasterLum
				|| maxCLL != lhs.maxCLL
				|| maxFALL != lhs.maxFALL;
		}

	};
	HDRprops uiHdrProps;
	HDRprops appliedHdrProps;

public:
	UIController()
	{
		uiHdrProps.hdrEnabled = g_HDRon;
	}

	~UIController()
	{
		// turn off HDR on shutdown
		if (appliedHdrProps.hdrEnabled)
		{
			SetHdrMonitorMode(false, &chromacityList[appliedHdrProps.DisplayPrimaries], appliedHdrProps.maxMasterLum, appliedHdrProps.minMasterLum, appliedHdrProps.maxCLL, appliedHdrProps.maxFALL);
		}
	}

	virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
	{
		(void) hWnd;
		(void) uMsg;
		(void) wParam;
		(void) lParam;

		if (uMsg == WM_MOUSEMOVE)
		{
			// track mouse (not quite right on multimon systems)
			g_MouseX = LOWORD(lParam);
			g_MouseY = HIWORD(lParam);

		}

		if(TwEventWin(hWnd, uMsg, wParam, lParam))
		{
			return 0; // Event has been handled by AntTweakBar
		}

		if(uMsg == WM_KEYDOWN)
		{        
			int iKeyPressed = static_cast<int>(wParam);
			switch( iKeyPressed )
			{
			case VK_TAB:
				g_bRenderHUD = !g_bRenderHUD;
				return 0;
				break;

			case VK_F1:
				PerfTracker::ui_toggle_visibility();
				break;

			case VK_ESCAPE:
				PostQuitMessage(0);
				break;
 

			default:
				break;
			}
		}
		return 1; 
	}

	virtual void Animate(double fElapsedTimeSeconds)
	{

		this->ui_update_time -= (float) fElapsedTimeSeconds;
		if (this->ui_update_time <= 0)
		{
			this->ui_update_time = 1.0f;
			std::vector<PerfTracker::FrameMeasurements> perf_measurements;
			PerfTracker::get_results(perf_measurements);
			PerfTracker::ui_update(perf_measurements);
		}

		if (uiHdrProps != appliedHdrProps)
		{
			appliedHdrProps = uiHdrProps;
			SetHdrMonitorMode(appliedHdrProps.hdrEnabled, &chromacityList[appliedHdrProps.DisplayPrimaries], appliedHdrProps.maxMasterLum, appliedHdrProps.minMasterLum, appliedHdrProps.maxCLL, appliedHdrProps.maxFALL);
		}
	}

	virtual void Render(ID3D11Device*, ID3D11DeviceContext*, ID3D11RenderTargetView*, ID3D11DepthStencilView*) 
	{ 
		if (g_bRenderHUD)
		{
			TwBeginText(2, 0, 0, 0);
			char msg[256];
			double averageTime = g_device_manager->GetAverageFrameTime();
			double fps = (averageTime > 0) ? 1.0 / averageTime : 0.0;
			sprintf_s(msg, "%.1f FPS", fps);
			TwAddTextLine(msg, 0xFF9BD839, 0xFF000000);
			TwEndText();

			TwBeginText(g_Width - 100, 0, 0, 0);
			sprintf_s(msg, "Red   %0.4f", g_Red);
			TwAddTextLine(msg, 0xFFFF0000, 0xFF000000);
			TwEndText();
			TwBeginText(g_Width - 100, 20, 0, 0);
			sprintf_s(msg, "Green %0.4f", g_Green);
			TwAddTextLine(msg, 0xFF00FF00, 0xFF000000);
			TwEndText();
			TwBeginText(g_Width - 100, 40, 0, 0);
			sprintf_s(msg, "Blue  %0.4f", g_Blue);
			TwAddTextLine(msg, 0xFF0000FF, 0xFF000000);
			TwEndText();

			TwDraw();
		}
	}

	virtual HRESULT DeviceCreated(ID3D11Device* pDevice) 
	{ 
		TwInit(TW_DIRECT3D11, pDevice);        
		TwDefine("GLOBAL fontstyle=fixed contained=true");
		InitDialogs();

		return S_OK;
	}

	virtual void DeviceDestroyed() 
	{ 
		TwTerminate();
	}

	virtual void BackBufferResized(ID3D11Device* pDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc) 
	{
		(void)pDevice;
		TwWindowSize(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);


		if (settings_bar) {
		}
	}

	void InitDialogs()
	{
		settings_bar = TwNewBar("Settings");
		TwDefine("Settings color='19 25 19' alpha=255 text=light size='500 150' iconified=false valueswidth=200 position='10 15'");

		{
			std::vector<TwEnumVal> texEnums;
			if (g_Textures.size())
			{
				for (size_t i = 0; i < g_Textures.size(); i++)
				{
					TwEnumVal e;
					e.Value = int(i);
					e.Label = g_Textures[i].c_str();
					texEnums.push_back(e);
				}
			}
			// add the pattern generator
			{
				TwEnumVal e;
				e.Value = int(g_Textures.size());
				e.Label = "Test Pattern";
				texEnums.push_back(e);
			}
			TwType enumModeType = TwDefineEnum("Texture_index", &texEnums[0], unsigned int(texEnums.size()));
			TwAddVarRW(settings_bar, "Texture", enumModeType, &g_tex_index, "keyIncr=v keyDecr=V");
		}
		{

			std::vector<TwEnumVal> enumModeTypeEV;
			for (auto tonemapper : TonemapperList)
			{
				TwEnumVal e = { tonemapper->Mode, tonemapper->Label };
				enumModeTypeEV.push_back(e);
			}

			TwType enumModeType = TwDefineEnum("TonemapMode", &enumModeTypeEV[0], unsigned int(enumModeTypeEV.size()));
			TwAddVarRW(settings_bar, "Tonemap Mode", enumModeType, &g_view_mode, "");
			TwAddButton(settings_bar, "comment", nullptr, nullptr, "label='For split-screen comparison, see composite panel'");
		}

		hdr_bar = TwNewBar("HdrSettings");
		TwDefine("HdrSettings color='19 25 19' alpha=255 text=light size='500 250' iconified=true valueswidth=200");

		TwAddVarRW(hdr_bar, "EnableHDR", TW_TYPE_BOOLCPP, &(uiHdrProps.hdrEnabled), "");
		{
			TwEnumVal enumModeTypeEV[] = {
					{ PRIM_REC709, "Rec709/sRGB" },
					{ PRIM_DCIP3, "DCI-P3" },
					{ PRIM_BT2020, "BT2020" },
					{ PRIM_P3D60, "DCI-D60" },
					{ PRIM_ADOBE98, "Adobe98" },
			};
			TwType enumModeType = TwDefineEnum("DisplayPrimaries", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(hdr_bar, "DisplayPrimaries", enumModeType, &(uiHdrProps.DisplayPrimaries), "");
		}

		TwAddVarRW(hdr_bar, "Max Master", TW_TYPE_FLOAT, &(uiHdrProps.maxMasterLum), "min=1.0  max=10000.0  precision=1");
		TwAddVarRW(hdr_bar, "Min Master", TW_TYPE_FLOAT, &(uiHdrProps.minMasterLum), "min=1.0  max=10000.0  precision=1");
		TwAddVarRW(hdr_bar, "Max CLL", TW_TYPE_FLOAT, &(uiHdrProps.maxCLL), "min=1.0  max=10000.0  precision=1");
		TwAddVarRW(hdr_bar, "Max FALL", TW_TYPE_FLOAT, &(uiHdrProps.maxFALL), "min=1.0  max=10000.0  precision=1");
	}
};


bool g_Fullscreen = false;
bool g_LogErrors = true;
bool g_sRGB = false;
int g_Display = 0;

void ParseCommandline()
{
	for (int i = 1; i < __argc; i++)
	{
		if (!wcscmp(L"-w", __wargv[i]))
		{
			i += 1;
			if (i < __argc)
			{
				g_Width = _wtoi(__wargv[i]);
			}
		}
		else if (!wcscmp(L"-h", __wargv[i]))
		{
			i += 1;
			if (i < __argc)
			{
				g_Height = _wtoi(__wargv[i]);
			}
		}
		else if (!wcscmp(L"-fullscreen", __wargv[i]))
		{
			g_Fullscreen = true;
		}
		else if (!wcscmp(L"-log", __wargv[i]))
		{
			g_LogErrors = true;
		}
		else if (!wcscmp(L"-display", __wargv[i]))
		{
			i += 1;
			if (i < __argc)
			{
				g_Display = _wtoi(__wargv[i]);
			}
		}
		else if (!wcscmp(L"-hdr", __wargv[i]))
		{
			g_HDRon = true;
		}
		else if (!wcscmp(L"-sRGB", __wargv[i]))
		{
			g_sRGB = true;
		}
		else if (wcsncmp(L"-", __wargv[i], 1))
		{
			char mbcs[256];
			// assuming that the first non-dash argument signifies the begining of a list of image files
			while (i < __argc)
			{
				// convert to ascii (not really unicode safe)
				wcstombs( mbcs, __wargv[i], 256);

				std::string filepath(mbcs);
				
				if (filepath.rfind(".exr") != std::string::npos)
				{
					try
					{
						Imf_2_2::RgbaInputFile file(mbcs);
						g_Textures.push_back(std::string(mbcs));
					}
					catch (...) { /*file load failure means we skip it*/ }
				}
				else if (filepath.rfind(".hdr") != std::string::npos)
				{
					FILE* fp = fopen(mbcs, "rb");
					if (fp)
					{
						g_Textures.push_back(std::string(mbcs));
						fclose(fp);
					}
				}
				i++;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
////////////////////////////////////////////////////////////////////////////////////////////////////

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	//NvAPI_Initialize();

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	const bool isDebug = true;
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#else
	const bool isDebug = false;
#endif

	ParseCommandline();

	if (g_LogErrors || isDebug)
	{
		if (!AttachConsole(ATTACH_PARENT_PROCESS))
		{
			AllocConsole();
		}
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);

	}

	g_device_manager = new DeviceManager();

	auto scene_controller = SceneController();
	g_device_manager->AddControllerToFront(&scene_controller);
	
	auto ui_controller = UIController();
	g_device_manager->AddControllerToFront(&ui_controller);

	if (g_Fullscreen)
	{
		// default to giant in fullscreen, where the window setup will pare it down
		g_Width = g_Width ? g_Width : 8196;
		g_Height = g_Height ? g_Height : 8196;
	}
	else
	{
		g_Width = g_Width ? g_Width : 1280;
		g_Height = g_Height ? g_Height : 720;
	}

	DeviceCreationParameters deviceParams;
	deviceParams.swapChainFormat = g_sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R16G16B16A16_FLOAT;
	deviceParams.swapChainSampleCount = 1;
	deviceParams.startFullscreen = g_Fullscreen;
	deviceParams.backBufferWidth = g_Width;
	deviceParams.backBufferHeight = g_Height;
	deviceParams.displayIndex = g_Display;
#if defined(DEBUG)
	deviceParams.createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif

	if(FAILED(g_device_manager->CreateWindowDeviceAndSwapChain(deviceParams, L"HDRDisplay image viewer")))
	{
		MessageBox(nullptr, L"Cannot initialize the D3D11 device with the requested parameters", L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}
	g_device_manager->SetVsyncEnabled(true);
	PerfTracker::initialize();
	PerfTracker::EventDesc perf_events[] = {
		PERF_EVENT_DESC("Render Scene"),
		PERF_EVENT_DESC("Render > Main"),
	};
	PerfTracker::ui_setup(perf_events, sizeof(perf_events)/sizeof(PerfTracker::EventDesc), nullptr);

	g_device_manager->MessageLoop();
	g_device_manager->Shutdown();

	PerfTracker::shutdown();
	delete g_device_manager;

	return 0;
}
