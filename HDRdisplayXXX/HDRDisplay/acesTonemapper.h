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

#include "ACES.h"

// TODO : refactor to have one common settings block

struct AcesSettings
{
	int selectedColorMatrix;
	int selectedCurve;
	int outputMode;
	float minStops;
	float maxStops;
	float maxLevel;
	float midGrayScale;
	float surroundGamma;
	float toneCurveSaturation;
	float outputGamma;
	bool adjustWP;
	bool desaturate;
	bool dimSurround;
	bool luminanceOnly;

	static void TW_CALL Apply1000nitHDR(void *data);
	static void TW_CALL Apply1000nitHDRSharpened(void *data);
	static void TW_CALL ApplySDR(void *data);
	static void TW_CALL ApplyEDR(void *data);
	static void TW_CALL ApplyEDRExtreme(void *data);

	AcesSettings()
	{
		outputMode = 2; // scRGB
		surroundGamma = 0.9811f;
		toneCurveSaturation = 1.0f;
		selectedColorMatrix = 0;
		selectedCurve = ODT_LDR_Ref;
		outputGamma = 2.2f;
		adjustWP = true;
		desaturate = true;
		dimSurround = true;
		luminanceOnly = false;
		maxStops = 8.0f;
		maxLevel = -1.0f;
		midGrayScale = 1.0f;
	}

	void InitPresets(TwBar* settings_bar)
	{
		TwAddButton(settings_bar, "HDR1000", Apply1000nitHDR, this, "label='Apply HDR 1000 nit preset'");
		TwAddButton(settings_bar, "HDR1000Sharp", Apply1000nitHDRSharpened, this, "label='Apply sharpened HDR 1000 nit preset'");
		TwAddButton(settings_bar, "SDR", ApplySDR, this, "label='Apply SDR preset'");
		TwAddButton(settings_bar, "EDR", ApplyEDR, this, "label='Apply EDR preset'");
		TwAddButton(settings_bar, "EDRExtreme", ApplyEDRExtreme, this, "label='Apply extreme EDR preset'");
	}

	void InitUI(TwBar* settings_bar)
	{
		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "sRGB/rec709" },
					{ 1, "DCI-P3" },
					{ 2, "BT2020" },
			};
			TwType enumModeType = TwDefineEnum("ColorSpace", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Color Space", enumModeType, &selectedColorMatrix, "group=Settings");
		}
		{
			TwEnumVal enumModeTypeEV[] = {
					{ ODT_LDR_Adj, "LDR adjustable" },
					{ ODT_1000Nit_Adj, "1000 nit adjustable" },
					{ ODT_2000Nit_Adj, "2000 nit adjustable" },
					{ ODT_4000Nit_Adj, "4000 nit adjustable" },
					{ ODT_LDR_Ref, "LDR" },
					{ ODT_1000Nit_Ref, "1000 nit reference" },
					{ ODT_2000Nit_Ref, "2000 nit reference" },
					{ ODT_4000Nit_Ref, "4000 nit reference" },

			};
			TwType enumModeType = TwDefineEnum("ToneCurve", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Tone Curve", enumModeType, &selectedCurve, "group=Curve");
		}
		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "sRGB" },
					{ 1, "PQ" },
					{ 2, "scRGB" },
					{ 3, "Gamma" },
			};
			TwType enumModeType = TwDefineEnum("EOTF", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "EOTF Mode/Output mode", enumModeType, &outputMode, "group=Settings");
		}

		TwAddVarRW(settings_bar, "EOTF Mode/Gamma value", TW_TYPE_FLOAT, &outputGamma, "min=0.2 max=4.0 step=0.1 precision=2 group=Settings");

		TwAddSeparator(settings_bar, nullptr, " group=Settings ");
		TwAddVarRW(settings_bar, "Tonemap Luminance", TW_TYPE_BOOLCPP, &luminanceOnly, "group=Settings");
		TwAddVarRW(settings_bar, "Saturation level", TW_TYPE_FLOAT, &toneCurveSaturation, "min=0.0 max=1.1 step=0.05 precision=2 group=Settings");

		TwAddSeparator(settings_bar, nullptr, " group=Settings ");
		TwAddVarRW(settings_bar, "CAT D60 to D65", TW_TYPE_BOOLCPP, &adjustWP, "group=Settings");
		TwAddVarRW(settings_bar, "Desaturate", TW_TYPE_BOOLCPP, &desaturate, "group=Settings");

		TwAddSeparator(settings_bar, nullptr, " group=Settings ");
		TwAddVarRW(settings_bar, "Alter Surround", TW_TYPE_BOOLCPP, &dimSurround, "group=Settings");
		TwAddVarRW(settings_bar, "Surround Gamma", TW_TYPE_FLOAT, &surroundGamma, "min=0.6 max=1.2 step=0.005 precision=3 group=Settings");

		TwAddVarRW(settings_bar, "Min Stops", TW_TYPE_FLOAT, &minStops, "min=-14.0  max=0.0 step=0.25 precision=2 group=Curve");
		TwAddVarRW(settings_bar, "Max Stops", TW_TYPE_FLOAT, &maxStops, "min=0.0  max=14.0 step=0.25 precision=2 group=Curve");
		TwAddVarRW(settings_bar, "Max Level (nits -1=default)", TW_TYPE_FLOAT, &maxLevel, "min=-1.0  max=4000.0 group=Curve");
		TwAddVarRW(settings_bar, "Middle Gray Scale", TW_TYPE_FLOAT, &midGrayScale, "min=0.01 max=100.0 step=0.1 precision=3 group=Curve");

		std::string barName(TwGetBarName(settings_bar));
		std::string arg = barName + "/Curve opened=false group=Settings ";
		TwDefine( arg.c_str());
		arg = barName + "/Settings opened=false ";
		TwDefine(arg.c_str());
	}
};
/*
* ACES tonemapper with components exposed as parameters
*/
class ParameterizedACES : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;

	struct Constants
	{
		float ACES_min[2];
		float ACES_mid[2];
		float ACES_max[2];
		float ACES_slope[2];
		float ACES_coefs[10 * 4];
		float colorMat[12];
		float colorMatInv[12];
		float CinemaLimits[2];
		int OutputMode;
		unsigned int flags;
		float surroundGamma;
		float saturation;
		float postScale;
		float gamma;
	};

	Constants constants;


	AcesSettings settings;

public:

	ParameterizedACES(ID3D11Device * inDevice) : Tonemapper(inDevice)
	{
		shader = CompilePS("ACES/ACES_parameterized.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);
		constants.postScale = 1.0f;

	}

	~ParameterizedACES()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override;

	static void TW_CALL ApplyEDR1(void* data);
	static void TW_CALL ApplyEDR2(void* data);
	static void TW_CALL Apply1000nitHDR(void *data);
	static void TW_CALL Apply1000nitHDRSharpened(void *data);
	static void TW_CALL ApplySDR(void *data);
	static void TW_CALL ApplyEDRExtreme(void* data);

	TwBar* InitUI() override
	{
		Apply1000nitHDR(this);

		TwBar* settings_bar = TwNewBar("Parameterized_ACES");
		TwDefine("Parameterized_ACES " TONEMAPPER_UI_SETTINGS);
		TwDefine("Parameterized_ACES size='500 350'");

		settings.InitPresets(settings_bar);
		settings.InitUI(settings_bar);

		return settings_bar;
	}
};

/*
* ACES tonemapper with components exposed as parameters
*/
class LutACES : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;
	ID3D11Texture3D *LUTtex;
	ID3D11ShaderResourceView *LUTsrv;
	ID3D11SamplerState *LUTsamp;

	struct Constants
	{
		float scale;
		float bias;
		unsigned int scRGB_output;
		unsigned int shaper;
	};

	Constants constants;

	struct Settings
	{
		int LUTdimx;
		int LUTdimy;
		int LUTdimz;
		unsigned int shaper;
		AcesSettings aces;
	};

	Settings active;
	Settings current;

	void UpdateLUT();
public:

	LutACES(ID3D11Device * inDevice) : Tonemapper(inDevice), LUTtex(nullptr), LUTsrv(nullptr), LUTsamp(nullptr)
	{
		shader = CompilePS("ACES/ACES_lut.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);

		current.LUTdimx = 32;
		current.LUTdimy = 32;
		current.LUTdimz = 32;
		current.shaper = 0;

		// just need to set one thing in the "active" set to ensure it isn't matching the "current" one on startup
		active.LUTdimx = 0;
		active.LUTdimy = 0;
		active.LUTdimz = 0;
	}

	~LutACES()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
		SAFE_RELEASE(LUTsrv);
		SAFE_RELEASE(LUTtex);
		SAFE_RELEASE(LUTsamp);
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override;



	TwBar* InitUI() override
	{
		TwBar* settings_bar = TwNewBar("LUT_ACES");
		TwDefine("LUT_ACES " TONEMAPPER_UI_SETTINGS);
		TwDefine("LUT_ACES size='500 300'");

		current.aces.InitPresets(settings_bar);

		TwAddVarRW(settings_bar, "LUT size R", TW_TYPE_INT32, &current.LUTdimx, "min=2 max=128 group=LUT");
		TwAddVarRW(settings_bar, "LUT size G", TW_TYPE_INT32, &current.LUTdimy, "min=2 max=128 group=LUT");
		TwAddVarRW(settings_bar, "LUT size B", TW_TYPE_INT32, &current.LUTdimz, "min=2 max=128 group=LUT");

		{
			TwEnumVal enumModeTypeEV[] = {
					{ 0, "log" },
					{ 1, "PQ" },
			};
			TwType enumModeType = TwDefineEnum("ShaperFunction", enumModeTypeEV, sizeof(enumModeTypeEV) / sizeof(enumModeTypeEV[0]));
			TwAddVarRW(settings_bar, "Shaper Function", enumModeType, &current.shaper, "group=LUT");
		}

		current.aces.InitUI(settings_bar);

		return settings_bar;
	}
};

/*
* ACES LDR tonemapper for split screen, derived from parameterized
*/
class LDR_ss : public Tonemapper
{
protected:
	ID3D11PixelShader *shader;
	ID3D11Buffer *cb;

	struct Constants
	{
		float ACES_min[2];
		float ACES_mid[2];
		float ACES_max[2];
		float ACES_slope[2];
		float ACES_coefs[10 * 4];
		float colorMat[12];
		float colorMatInv[12];
		float CinemaLimits[2];
		int OutputMode;
		unsigned int flags;
		float surroundGamma;
		float saturation;
		float post_scale;
		float gamma;
	};

	Constants constants;

	AcesSettings settings;
public:

	LDR_ss(ID3D11Device * inDevice) : Tonemapper(inDevice)
	{
		shader = CompilePS("ACES/ACES_parameterized.hlsl", "main");

		D3D11_BUFFER_DESC desc;
		ZeroMemory(&desc, sizeof(desc));

		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = sizeof(Constants);
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		desc.Usage = D3D11_USAGE_DYNAMIC;

		device->CreateBuffer(&desc, nullptr, &cb);

		constants.OutputMode = 2; // scRGB
		constants.surroundGamma = 0.9811f;
		constants.saturation = 1.0f;
		constants.post_scale = 1.0f;
	}

	~LDR_ss()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(cb);
	}

	void SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData) override;


	TwBar* InitUI() override
	{
		AcesSettings::ApplySDR(&settings);

		TwBar* settings_bar = TwNewBar("SplitScreen");
		TwDefine("SplitScreen " TONEMAPPER_UI_SETTINGS);
		TwDefine("SplitScreen iconified=true");

		settings.InitPresets(settings_bar);
		settings.InitUI(settings_bar);

		TwAddVarRW(settings_bar, "Post Scale", TW_TYPE_FLOAT, &constants.post_scale, "min=0.0f max=1250f step=0.1 precision=2");

		return settings_bar;
	}

};

