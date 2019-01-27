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
* transform pass that performs optional operations on the input
*/
class XformPass : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;

	struct Constants
	{
		unsigned int filter;
		int zoom;
		float scale;
		float expansion;
		int size_data[4];
		unsigned int match_aspect;
		unsigned int tile;
		int readback_pos[2];
		unsigned int applyAutoExposure;
		unsigned int gradingFlags;
		float xlrContrastL;
		float xlrContrastC;
		float xlrScaleC;
		float xlrBiasA;
		float xlrBiasB;
		float pad;
		float rgbSaturation[4];
		float rgbContrast[4];
		float rgbGamma[4];
		float rgbGain[4];
		float rgbBias[4];
		float iptContrastL;
		float iptContrastC;
		float iptScaleC;
		float iptBiasC;
		float iptBiasA;
		float iptBiasB;
		float pad2[2];
	};

	Constants constants;

	bool gradeXLR;
	bool gradeRGB;
	bool gradeIPT;
	bool gradeSplitScreen;

public:

	XformPass(ID3D11Device * inDevice) : Tonemapper(inDevice)
	{
		shader = CompilePS("xform_input.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);

		constants.filter = 1;
		constants.zoom = 0;
		constants.scale = 0.5f;
		constants.expansion = 1.0f;
		constants.match_aspect = 1;
		constants.tile = 0;
		constants.applyAutoExposure = 0;

		gradeXLR = false;
		gradeRGB = false;
		gradeIPT = false;
		gradeSplitScreen = false;

		constants.xlrContrastL = 1.0f;
		constants.xlrContrastC = 1.0f;
		constants.xlrScaleC = 1.0f;
		constants.xlrBiasA = 0.0f;
		constants.xlrBiasB = 0.0f;

		constants.iptContrastL = 1.0f;
		constants.iptContrastC = 1.0f;
		constants.iptScaleC = 1.0f;
		constants.iptBiasC = 0.0f;
		constants.iptBiasA = 0.0f;
		constants.iptBiasB = 0.0f;

		for (int i = 0; i < 3; i++)
		{
			constants.rgbSaturation[i] = 1.0f;
			constants.rgbContrast[i] = 1.0f;
			constants.rgbGamma[i] = 1.0f;
			constants.rgbGain[i] = 1.0f;
			constants.rgbBias[i] = 0.0f;
		}
	}

	~XformPass()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
	}

	void SetDimensions(int inX, int inY, int outX, int outY) override
	{
		constants.size_data[0] = inX;
		constants.size_data[1] = inY;
		constants.size_data[2] = outX;
		constants.size_data[3] = outY;
	}

	void SetReadback(int x, int y) override
	{
		constants.readback_pos[0] = x;
		constants.readback_pos[1] = y;
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override
	{
		SetupTransformPass(ctx, srcData, nullptr);
	}

	void SetupTransformPass(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData, ID3D11ShaderResourceView* autoExposure)
	{
		constants.gradingFlags = gradeXLR ? 0x1 : 0x0;
		constants.gradingFlags |= gradeRGB ? 0x2 : 0x0;
		constants.gradingFlags |= gradeIPT ? 0x4 : 0x0;
		constants.gradingFlags |= gradeSplitScreen ? 0x8 : 0x0;

		D3D11_MAPPED_SUBRESOURCE mapObj;
		ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
		memcpy(mapObj.pData, &constants, sizeof(constants));
		ctx->Unmap(cb, 0);

		ctx->PSSetConstantBuffers(0, 1, &cb);
		ctx->PSSetShader(shader, nullptr, 0);
		ctx->PSSetShaderResources(0, 1, &srcData);
		ctx->PSSetShaderResources(1, 1, &autoExposure);
	}

#define SUPPORT_XLR_GRADE 0
#define SUPPORT_IPT_GRADE 1
#define SUPPORT_RGB_GRADE 1

	TwBar* InitUI() override
	{
		TwBar* settings_bar = TwNewBar("Input_Xform");
		TwDefine("Input_Xform " TONEMAPPER_UI_SETTINGS);
		TwDefine("Input_Xform iconified=true label='Pre-transform Color'");

		TwAddVarRW(settings_bar, "Filtered", TW_TYPE_BOOL32, &constants.filter, "");
		TwAddVarRW(settings_bar, "Match Aspect", TW_TYPE_BOOL32, &constants.match_aspect, "");
		TwAddVarRW(settings_bar, "Tile Image", TW_TYPE_BOOL32, &constants.tile, "");

		{
			TwEnumVal enumModeTypeEV[] = {
					{ -3, "1/8x" },
					{ -2, "1/4x" },
					{ -1, "1/2x" },
					{ 0, "1x" },
					{ 1, "2x" },
					{ 2, "4x" },
					{ 3, "8x" },
			};
			TwType enumModeType = TwDefineEnum("Zoom", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Zoom", enumModeType, &constants.zoom, "");
		}

		TwAddVarRW(settings_bar, "AutoExposure", TW_TYPE_BOOL32, &constants.applyAutoExposure, "");
		TwAddVarRW(settings_bar, "ExposureBias (stops)", TW_TYPE_FLOAT, &constants.scale, "min=-12.0  max=12.0 step=0.25  precision=2");

		TwAddVarRW(settings_bar, "ExpansionPower", TW_TYPE_FLOAT, &constants.expansion, "min=0.5  max=2.0 step=0.05  precision=2");

		TwAddVarRW(settings_bar, "Grade Split Screen", TW_TYPE_BOOLCPP, &gradeSplitScreen, "");

#if SUPPORT_XLR_GRADE
		TwAddVarRW(settings_bar, "Grade in XLR", TW_TYPE_BOOLCPP, &gradeXLR, "");
#endif
#if SUPPORT_RGB_GRADE
		TwAddVarRW(settings_bar, "Grade in RGB", TW_TYPE_BOOLCPP, &gradeRGB, "");
#endif
#if SUPPORT_IPT_GRADE 
		TwAddVarRW(settings_bar, "Grade in IPT", TW_TYPE_BOOLCPP, &gradeIPT, "");
#endif

#if SUPPORT_XLR_GRADE
		TwAddVarRW(settings_bar, "Contrast L", TW_TYPE_FLOAT, &constants.xlrContrastL, "precision=2 group=XLR_Grade");
		TwAddVarRW(settings_bar, "Contrast C", TW_TYPE_FLOAT, &constants.xlrContrastC, "precision=2 group=XLR_Grade");
		TwAddVarRW(settings_bar, "Scale C", TW_TYPE_FLOAT, &constants.xlrScaleC, "precision=2 group=XLR_Grade");
		TwAddVarRW(settings_bar, "Bias a", TW_TYPE_FLOAT, &constants.xlrBiasA, "precision=2 group=XLR_Grade");
		TwAddVarRW(settings_bar, "Bias b", TW_TYPE_FLOAT, &constants.xlrBiasB, "precision=2 group=XLR_Grade");
#endif

#if SUPPORT_RGB_GRADE
#define ADD_VEC3_SETTING( bar, name, type, var, def) { const char* suffix[] = { " R", " G", " B"}; for(int i = 0; i <3; i++) { std::string label(name); label = label + suffix[i]; TwAddVarRW( bar, label.c_str(), type, &(var[i]), def); } }
		ADD_VEC3_SETTING(settings_bar, "Saturation", TW_TYPE_FLOAT, constants.rgbSaturation, "precision=2 group=RGB_Grade min=0.0  max=1.5 step=0.1");
		ADD_VEC3_SETTING(settings_bar, "Contrast", TW_TYPE_FLOAT, constants.rgbContrast, "precision=2 group=RGB_Grade min=0.5  max=4.0 step=0.1");
		ADD_VEC3_SETTING(settings_bar, "Gamma", TW_TYPE_FLOAT, constants.rgbGamma, "precision=2 group=RGB_Grade min=0.2  max=4.0 step=0.1");
		ADD_VEC3_SETTING(settings_bar, "Gain", TW_TYPE_FLOAT, constants.rgbGain, "precision=2 group=RGB_Grade min=-4.0  max=4.0 step=0.1");
		ADD_VEC3_SETTING(settings_bar, "Bias", TW_TYPE_FLOAT, constants.rgbBias, "precision=2 group=RGB_Grade min=-4.0  max=4.0 step=0.1");
#undef ADD_VEC3_SETTING
#endif

#if SUPPORT_IPT_GRADE 
		TwAddVarRW(settings_bar, "Intensity Contrast", TW_TYPE_FLOAT, &constants.iptContrastL, "precision=2 group=IPT_Grade min=0.5 max=2.0 step=0.05");
		TwAddVarRW(settings_bar, "Colorfulness Contrast", TW_TYPE_FLOAT, &constants.iptContrastC, "precision=2 group=IPT_Grade min=0.5  max=2.0 step=0.05");
		TwAddVarRW(settings_bar, "Colorfulness Scale", TW_TYPE_FLOAT, &constants.iptScaleC, "precision=2 group=IPT_Grade min=0.0  max=4.0 step=0.1");
		TwAddVarRW(settings_bar, "Colorfulness Bias", TW_TYPE_FLOAT, &constants.iptBiasC, "precision=2 group=IPT_Grade min=-0.5  max=1.0 step=0.1");
		TwAddVarRW(settings_bar, "Red/Green Bias", TW_TYPE_FLOAT, &constants.iptBiasA, "precision=2 group=IPT_Grade min=-1.0  max=1.0 step=0.05");
		TwAddVarRW(settings_bar, "Blue/Yellow Bias", TW_TYPE_FLOAT, &constants.iptBiasB, "precision=2 group=IPT_Grade min=-1.0  max=1.0 step=0.05");
#endif

		TwDefine(" Input_Xform/XLR_Grade opened=false ");
		TwDefine(" Input_Xform/RGB_Grade opened=false ");
		TwDefine(" Input_Xform/IPT_Grade opened=false ");
		return settings_bar;
	}

};