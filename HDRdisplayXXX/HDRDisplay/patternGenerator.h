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

/*
* Simple pattern generator to create test data
*/
class PatternGen : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;

	struct Constants
	{
		float brighness[2];
		unsigned int cIndex[2];
		unsigned int mode;
		float scale;
		float pad[2];
	};

	Constants current;
	Constants active;

public:

	PatternGen(ID3D11Device * inDevice) : Tonemapper(inDevice)
	{
		shader = CompilePS("test_generator.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);

		current.mode = 0;
		active.mode = 0xffffffff;
		current.brighness[0] = 1.0f;
		current.brighness[1] = 1.0f;
		current.cIndex[0] = 0;
		current.cIndex[1] = 2;
		current.scale = 4.0f;
	}

	~PatternGen()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
	}

	bool Dirty()
	{
		return memcmp(&active, &current, sizeof(Constants)) != 0;
	}

	int Width()
	{
		return 900;
	}

	int Height()
	{
		return 600;
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override
	{
		memcpy(&active, &current, sizeof(Constants));
		D3D11_MAPPED_SUBRESOURCE mapObj;
		ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
		memcpy(mapObj.pData, &active, sizeof(Constants));
		ctx->Unmap(cb, 0);

		ctx->PSSetConstantBuffers(0, 1, &cb);
		ctx->PSSetShader(shader, nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srcData);
	}

	TwBar* InitUI() override
	{
		TwBar* settings_bar = TwNewBar("TestPatternParameters");
		TwDefine("TestPatternParameters " TONEMAPPER_UI_SETTINGS);
		TwDefine("TestPatternParameters iconified=true");


		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "Full Color Checker" },
					{ 1, "Simple Color Checker" },
					{ 2, "Single Color Square" },
					{ 3, "Gradient" },
					{ 4, "HDR check" },
			};
			TwType enumModeType = TwDefineEnum("Pattern", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Pattern", enumModeType, &current.mode, "");
		}

		TwAddVarRW(settings_bar, "Brightness 1", TW_TYPE_FLOAT, &current.brighness[0], " step=0.1 precision=1");
		TwAddVarRW(settings_bar, "Brightness 2", TW_TYPE_FLOAT, &current.brighness[1], " step=0.1 precision=1");
		TwAddVarRW(settings_bar, "Color index 1", TW_TYPE_INT32, &current.cIndex[0], "min=0 max=139");
		TwAddVarRW(settings_bar, "Color index 2", TW_TYPE_INT32, &current.cIndex[1], "min=0 max=139");

		TwAddVarRW(settings_bar, "Scale (1/screen)", TW_TYPE_FLOAT, &current.scale, "min=1.0 step=0.5 precision=1");

		return settings_bar;
	}
};