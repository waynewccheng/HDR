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

// Simple Tonemapper interface for abstracting differences

#pragma once

#define NOMINMAX

#include <d3d11.h>
#include "shaderCompile.h"

#include "common_util.h"

#include "AntTweakBar.h"
#include <string>

// Macro for standard settings
#define TONEMAPPER_UI_SETTINGS "color='19 25 19' alpha=255 text=light size='500 250' iconified=false valueswidth=200 position='10 220'"

class Tonemapper
{
protected:
	ID3D11Device *device;

	static const float ColorMatrices[12 * 3];
	static const float ColorMatricesInv[12 * 3];
public:

	Tonemapper(ID3D11Device *inDevice) : device(inDevice)
	{}

	virtual ~Tonemapper() {}

	virtual void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) = 0;

	virtual TwBar* InitUI() { return nullptr; }

	virtual void SetDimensions(int inX, int inY, int outX, int outY) {}
	virtual void SetReadback(int x, int y) {}


	ID3D11PixelShader* CompilePS(const char* shaderFile, const char* entry)
	{
		ID3D11PixelShader *shader = nullptr;
		HRESULT hr = S_OK;
		ID3DBlob* pErrorBlob = nullptr;
		ID3DBlob* pShaderBuffer = nullptr;

		hr = CompileShaderFromFile(shaderFile, NULL, entry, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, &pShaderBuffer, &pErrorBlob);
		if (FAILED(hr))
		{
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
			SAFE_RELEASE(pErrorBlob);
			return nullptr;
		}
		SAFE_RELEASE(pErrorBlob);

		device->CreatePixelShader(pShaderBuffer->GetBufferPointer(), pShaderBuffer->GetBufferSize(), nullptr, &shader);

		return shader;
	}
};


// simple tonemapper that has a shader file that is stand-alone and needs no extra support
class SimpleTonemapper : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
public:

	SimpleTonemapper(ID3D11Device * inDevice, const char* shaderFile, const char* entry = "main") : Tonemapper(inDevice)
	{
		shader = CompilePS(shaderFile, entry);
	}

	~SimpleTonemapper()
	{
		SAFE_RELEASE(shader);
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override
	{
		ctx->PSSetShader(shader, nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srcData);
	}

};

/*
 * simple linear tonemapper + OETF & colorspace
 */
class Linear : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;

	struct Constants
	{
		float scale;
		float gamma;
		int mode;
		float pad;
		float matrix[12];
	};

	Constants constants;

	int selectedColorMatrix;
public:

	Linear(ID3D11Device * inDevice) : Tonemapper(inDevice)
	{
		shader = CompilePS("linear.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);

		constants.scale = 1.0f;
		constants.gamma = 1.0f;
		constants.mode = 1;
		selectedColorMatrix = 0;
	}

	~Linear()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override
	{
		// setup the color matrix
		memcpy(constants.matrix, &ColorMatrices[selectedColorMatrix * 12], sizeof(float) * 12);

		D3D11_MAPPED_SUBRESOURCE mapObj;
		ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
		memcpy(mapObj.pData, &constants, sizeof(constants));
		ctx->Unmap(cb, 0);

		ctx->PSSetConstantBuffers(0, 1, &cb);
		ctx->PSSetShader(shader, nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srcData);
	}

	TwBar* InitUI() override
	{
		TwBar* settings_bar = TwNewBar("LinearTonemapParameters");
		TwDefine("LinearTonemapParameters " TONEMAPPER_UI_SETTINGS);

		TwAddVarRW(settings_bar, "Scale", TW_TYPE_FLOAT, &constants.scale, "min=0.000001  max=10000.0 step=0.25  precision=2");

		TwAddVarRW(settings_bar, "Gamma", TW_TYPE_FLOAT, &constants.gamma, "min=0.1  max=10.0 step=0.05  precision=2");

		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "sRGB/rec709" },
					{ 1, "DCI-P3" },
					{ 2, "BT2020" },
			};
			TwType enumModeType = TwDefineEnum("ColorSpace", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Color Space", enumModeType, &selectedColorMatrix, "");
		}
		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "sRGB" },
					{ 1, "Gamma" },
					{ 2, "PQ" },
					{ 3, "scRGB/raw" },
			};
			TwType enumModeType = TwDefineEnum("EOTF", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "EOTF Mode", enumModeType, &constants.mode, "");
		}

		return settings_bar;
	}
};


/*
* simple Reinhard tonemapper
*/
class Reinhard : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;

	struct Constants
	{
		float maxOutput;
		float gamma;
		int mode;
		float pad;
	};

	Constants constants;

public:

	Reinhard(ID3D11Device * inDevice) : Tonemapper(inDevice)
	{
		shader = CompilePS("reinhard.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);

		constants.maxOutput = 1000.0f;
		constants.gamma = 1.0f;
		constants.mode = 0;
	}

	~Reinhard()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override
	{

		D3D11_MAPPED_SUBRESOURCE mapObj;
		ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
		memcpy(mapObj.pData, &constants, sizeof(constants));
		ctx->Unmap(cb, 0);

		ctx->PSSetConstantBuffers(0, 1, &cb);
		ctx->PSSetShader(shader, nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srcData);
	}

	TwBar* InitUI() override
	{
		TwBar* settings_bar = TwNewBar("ReinhardTonemapParameters");
		TwDefine("ReinhardTonemapParameters " TONEMAPPER_UI_SETTINGS);

		TwAddVarRW(settings_bar, "Max display luminance (nits)", TW_TYPE_FLOAT, &constants.maxOutput, "min=0.000001  max=10000.0 step=10.0  precision=2");

		TwAddVarRW(settings_bar, "Gamma", TW_TYPE_FLOAT, &constants.gamma, "min=0.1  max=10.0 step=0.05  precision=2");

		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "sRGB" },
					{ 1, "Gamma" },
					{ 2, "PQ" },
					{ 3, "scRGB/raw" },
			};
			TwType enumModeType = TwDefineEnum("EOTF", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "EOTF Mode", enumModeType, &constants.mode, "");
		}

		return settings_bar;
	}
};

