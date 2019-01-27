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

#pragma once

#include "tonemapper.h"

//
// Simple compositor pass used to visualize different versions side by side
// 

class Compositor : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;

	struct Constants
	{
		unsigned int mode;
		unsigned int texA;
		unsigned int texB;
		float fade;
		float scroll[2];
		float pad[2];
	};

	Constants constants;


public:

	Compositor(ID3D11Device * inDevice) : Tonemapper(inDevice)
	{
		shader = CompilePS("composite.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);

		constants.mode = 0;
		constants.texA = 0;
		constants.texB = 1;
		constants.fade = 0.0f;
	}

	~Compositor()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
	}

	void tick( int mode, int texA, int texB, float fade)
	{
		constants.mode = mode;
		constants.texA = texA;
		constants.texB = texB;
		constants.fade = fade;
	}

	void setScroll(float x, float y)
	{
		constants.scroll[0] = x;
		constants.scroll[1] = y;
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

	void SetupShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView** srcData) 
	{

		D3D11_MAPPED_SUBRESOURCE mapObj;
		ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
		memcpy(mapObj.pData, &constants, sizeof(constants));
		ctx->Unmap(cb, 0);

		ctx->PSSetConstantBuffers(0, 1, &cb);
		ctx->PSSetShader(shader, nullptr, 0);
		ctx->PSSetShaderResources(0, 3, srcData);
	}



	TwBar* InitUI() override
	{
		TwBar* settings_bar = TwNewBar("Composite");
		TwDefine("Composite " TONEMAPPER_UI_SETTINGS);
		TwDefine("Composite iconified=true ");

		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "Fullscreen" },
					{ 1, "Splitscreen" },
					{ 2, "Tiled" },
					{ 3, "Scrolling" }
			};
			TwType enumModeType = TwDefineEnum("DisplayMode", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Mode", enumModeType, &constants.mode, "");
		}

		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "HDR" },
					{ 1, "LDR" },
					{ 2, "Original" }
			};
			TwType enumModeType = TwDefineEnum("ImageType", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Image A", enumModeType, &constants.texA, "");
			TwAddVarRW(settings_bar, "Image B", enumModeType, &constants.texB, "");
		}

		TwAddVarRW(settings_bar, "Fade", TW_TYPE_FLOAT, &constants.fade, "min=0.0  max=1.0 step=0.05 precision=2");

		return settings_bar;
	}

};