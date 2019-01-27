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

#include <algorithm>
#include <functional>
#include "acesTonemapper.h"

#include "ACES.h"

void TW_CALL AcesSettings::Apply1000nitHDR(void *data)
{
	AcesSettings *This = (AcesSettings*)data;

	This->selectedCurve = ODT_1000Nit_Adj;
	This->maxStops = -12.0f;
	This->maxStops = 10.0f;
	This->midGrayScale = 1.0f;
	This->adjustWP = true;
	This->desaturate = false;
	This->selectedColorMatrix = 2;
	This->outputMode = 2; // scRGB
}

void TW_CALL AcesSettings::Apply1000nitHDRSharpened(void *data)
{
	AcesSettings *This = (AcesSettings*)data;

	This->selectedCurve = ODT_1000Nit_Adj;
	This->minStops = -8.0f;
	This->maxStops = 8.0f;
	This->midGrayScale = 1.0f;
	This->adjustWP = true;
	This->desaturate = false;
	This->selectedColorMatrix = 2;
	This->outputMode = 2; // scRGB
}

void TW_CALL AcesSettings::ApplySDR(void *data)
{
	AcesSettings *This = (AcesSettings*)data;

	This->selectedCurve = ODT_LDR_Adj;
	This->minStops = -6.5f;
	This->maxStops = 6.5f;
	This->midGrayScale = 1.0f;
	This->adjustWP = true;
	This->desaturate = true;
	This->selectedColorMatrix = 0;
	This->outputMode = 0; // sRGB
}

void TW_CALL AcesSettings::ApplyEDRExtreme(void *data)
{
	AcesSettings *This = (AcesSettings*)data;

	This->selectedCurve = ODT_1000Nit_Adj;
	This->minStops = -12.0f;
	This->maxStops = 9.0f;
	This->midGrayScale = 1.0f;
	This->adjustWP = true;
	This->desaturate = false;
	This->selectedColorMatrix = 0;
	This->outputMode = 0; // sRGB
}

void TW_CALL AcesSettings::ApplyEDR(void *data)
{
	AcesSettings *This = (AcesSettings*)data;

	This->selectedCurve = ODT_1000Nit_Adj;
	This->minStops = -8.0f;
	This->maxStops = 8.0f;
	This->midGrayScale = 3.0f;
	This->adjustWP = true;
	This->desaturate = false;
	This->selectedColorMatrix = 0;
	This->outputMode = 0; // sRGB
}


void ParameterizedACES::SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData)
{
	// setup the color matrix
	memcpy(constants.colorMat, &ColorMatrices[settings.selectedColorMatrix * 12], sizeof(float) * 12);
	memcpy(constants.colorMatInv, &ColorMatricesInv[settings.selectedColorMatrix * 12], sizeof(float) * 12);

	// setup the aces data
	SegmentedSplineParams_c9 aces = GetAcesODTData(ODTCurve(settings.selectedCurve), settings.minStops, settings.maxStops, settings.maxLevel, settings.midGrayScale);

	memcpy(constants.ACES_coefs, aces.coefs, sizeof(float) * 40);
	memcpy(constants.ACES_max, &aces.maxPoint, sizeof(float) * 2);
	memcpy(constants.ACES_mid, &aces.midPoint, sizeof(float) * 2);
	memcpy(constants.ACES_min, &aces.minPoint, sizeof(float) * 2);
	memcpy(constants.ACES_slope, &aces.slope, sizeof(float) * 2);

	constants.CinemaLimits[0] = aces.minPoint.Y;
	constants.CinemaLimits[1] = aces.maxPoint.Y;

	constants.flags = settings.adjustWP ? 0x4 : 0x0;
	constants.flags |= settings.desaturate ? 0x2 : 0x0;
	constants.flags |= settings.dimSurround ? 0x1 : 0x0;
	constants.flags |= settings.luminanceOnly ? 0x8 : 0x0;

	constants.OutputMode = settings.outputMode;
	constants.saturation = settings.toneCurveSaturation;
	constants.surroundGamma = settings.surroundGamma;
	constants.gamma = settings.outputGamma;


	D3D11_MAPPED_SUBRESOURCE mapObj;
	ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
	memcpy(mapObj.pData, &constants, sizeof(constants));
	ctx->Unmap(cb, 0);

	ctx->PSSetConstantBuffers(1, 1, &cb);
	ctx->PSSetShader(shader, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &srcData);
}


void TW_CALL ParameterizedACES::Apply1000nitHDR(void *data)
{
	ParameterizedACES *This = (ParameterizedACES*)data;

	AcesSettings::Apply1000nitHDR( &(This->settings));
}

void TW_CALL ParameterizedACES::Apply1000nitHDRSharpened(void *data)
{
	ParameterizedACES *This = (ParameterizedACES*)data;

	AcesSettings::Apply1000nitHDRSharpened(&(This->settings));
}

void TW_CALL ParameterizedACES::ApplySDR(void *data)
{
	ParameterizedACES *This = (ParameterizedACES*)data;

	AcesSettings::ApplySDR(&(This->settings));
}

void LutACES::SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData)
{

	if (memcmp(&current, &active, sizeof(active)) != 0)
	{
		UpdateLUT();
	}


	D3D11_MAPPED_SUBRESOURCE mapObj;
	ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
	memcpy(mapObj.pData, &constants, sizeof(constants));
	ctx->Unmap(cb, 0);


	ctx->PSSetConstantBuffers(0, 1, &cb);
	ctx->PSSetShader(shader, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &srcData);
	ctx->PSSetShaderResources(1, 1, &LUTsrv);
	ctx->PSSetSamplers(1, 1, &LUTsamp);

}

const float linearGray = 0.18f;

// Base functions from SMPTE ST 2084-2014

// Constants from SMPTE ST 2084-2014
static const float pq_m1 = 0.1593017578125; // ( 2610.0 / 4096.0 ) / 4.0;
static const float pq_m2 = 78.84375; // ( 2523.0 / 4096.0 ) * 128.0;
static const float pq_c1 = 0.8359375; // 3424.0 / 4096.0 or pq_c3 - pq_c2 + 1.0;
static const float pq_c2 = 18.8515625; // ( 2413.0 / 4096.0 ) * 32.0;
static const float pq_c3 = 18.6875; // ( 2392.0 / 4096.0 ) * 32.0;

static const float pq_C = 10000.0;

// Converts from the non-linear perceptually quantized space to linear cd/m^2
// Note that this is in float, and assumes normalization from 0 - 1
// (0 - pq_C for linear) and does not handle the integer coding in the Annex 
// sections of SMPTE ST 2084-2014
static float pq_f(float N)
{
	// Note that this does NOT handle any of the signal range
	// considerations from 2084 - this assumes full range (0 - 1)
	float Np = powf(N, 1.0f / pq_m2);
	float L = Np - pq_c1;
	if (L < 0.0)
		L = 0.0;
	L = L / (pq_c2 - pq_c3 * Np);
	L = powf(L, 1.0f / pq_m1);
	return L * pq_C; // returns cd/m^2
}

// Converts from linear cd/m^2 to the non-linear perceptually quantized space
// Note that this is in float, and assumes normalization from 0 - 1
// (0 - pq_C for linear) and does not handle the integer coding in the Annex 
// sections of SMPTE ST 2084-2014
static float pq_r(float C)
{
	// Note that this does NOT handle any of the signal range
	// considerations from 2084 - this returns full range (0 - 1)
	float L = C / pq_C;
	float Lm = pow(L, pq_m1);
	float N = (pq_c1 + pq_c2 * Lm) / (1.0f + pq_c3 * Lm);
	N = pow(N, pq_m2);
	return N;
}

// whether to use fp32 for the LUT, primarily debugging
#define USE_FLOAT 0

void LutACES::UpdateLUT()
{
	SAFE_RELEASE(LUTtex);
	SAFE_RELEASE(LUTsrv);
	SAFE_RELEASE(LUTsamp);

	active = current;

	constants.scRGB_output = current.aces.outputMode == 2;
	constants.shaper = current.shaper;

	ACESparams params;
	params.C = GetAcesODTData(ODTCurve(current.aces.selectedCurve), current.aces.minStops, current.aces.maxStops, current.aces.maxLevel, current.aces.midGrayScale);

	params.applyCAT = current.aces.adjustWP;
	params.surroundAdjust = current.aces.dimSurround;
	params.CinemaLimits.X = params.C.minPoint.Y;
	params.CinemaLimits.Y = params.C.maxPoint.Y;
	params.desaturate = current.aces.desaturate;
	params.OutputMode = current.aces.outputMode;
	params.surroundGamma = current.aces.surroundGamma;
	params.tonemapLuminance = current.aces.luminanceOnly;
	params.saturationLevel = current.aces.toneCurveSaturation;
	memcpy(&params.XYZ_2_DISPLAY_PRI_MAT, &ColorMatrices[current.aces.selectedColorMatrix * 12], sizeof(float) * 3);
	memcpy(&(params.XYZ_2_DISPLAY_PRI_MAT.m[3]), &ColorMatrices[current.aces.selectedColorMatrix * 12 + 4], sizeof(float) * 3);
	memcpy(&(params.XYZ_2_DISPLAY_PRI_MAT.m[6]), &ColorMatrices[current.aces.selectedColorMatrix * 12 + 8], sizeof(float) * 3);

	memcpy(&params.DISPLAY_PRI_MAT_2_XYZ, &ColorMatricesInv[current.aces.selectedColorMatrix * 12], sizeof(float) * 3);
	memcpy(&(params.DISPLAY_PRI_MAT_2_XYZ.m[3]), &ColorMatricesInv[current.aces.selectedColorMatrix * 12 + 4], sizeof(float) * 3);
	memcpy(&(params.DISPLAY_PRI_MAT_2_XYZ.m[6]), &ColorMatricesInv[current.aces.selectedColorMatrix * 12 + 8], sizeof(float) * 3);

	std::function<float(float)> shaper_func;

	// scale and bias constants are a function of shaper and range
	if (current.shaper == 0) // log2
	{
		float log_min = log2(std::max(params.C.limits.X, 0.0000001f));
		float log_max = log2(params.C.limits.Y);
		constants.scale = 1.0f / (log_max - log_min);
		constants.bias = -(constants.scale * log_min);

		shaper_func = [=](float t) -> float { return pow(2.0f, ((t - constants.bias) / constants.scale)); };
	}
	else if (current.shaper == 1) //pq
	{

		// need to plumb in real min/max values
		float pq_min = params.C.limits.X;
		float pq_max = params.C.limits.Y;
		constants.scale = 10000.0f / (pq_max - pq_min); // scale to make the data fit 0-10000
		constants.bias = -(constants.scale * pq_min);
		shaper_func = [=](float t) -> float { return (pq_f(t) -constants.bias)  / constants.scale; };
	}
	else
	{
		//shouldn't happen
		shaper_func = [](float t) -> float { return t; };
		constants.scale = 1.0f;
		constants.bias = 0.0f;
	}


#if !USE_FLOAT
	unsigned short *data = new unsigned short[current.LUTdimx * current.LUTdimy * current.LUTdimz * 4];

	unsigned short *walk = data;

	for (int i = 0; i < current.LUTdimz; i++)
	{
		for (int j = 0; j < current.LUTdimy; j++)
		{
			for (int k = 0; k < current.LUTdimx; k++)
			{
				float x = (k + 0.5f) / float(current.LUTdimx);
				float y = (j + 0.5f) / float(current.LUTdimy);
				float z = (i + 0.5f) / float(current.LUTdimz);
				x = shaper_func(x);
				y = shaper_func(y);
				z = shaper_func(z);
				Float3 color = { x, y, z };
				Float3 temp = EvalACES(color, params);
				walk[0] = float2half(temp.X);
				walk[1] = float2half(temp.Y);
				walk[2] = float2half(temp.Z);
				walk[3] = 0x3b00; // 1.0 half

				walk += 4;
			}
		}
	}
#else
	float *data = new float[current.LUTdimx * current.LUTdimy * current.LUTdimz * 4];

	float *walk = data;

	for (int i = 0; i < current.LUTdimz; i++)
	{
		for (int j = 0; j < current.LUTdimy; j++)
		{
			for (int k = 0; k < current.LUTdimx; k++)
			{
				float x = (k + 0.5f) / float(current.LUTdimx);
				float y = (j + 0.5f) / float(current.LUTdimy);
				float z = (i + 0.5f) / float(current.LUTdimz);
				x = shaper_func(x);
				y = shaper_func(y);
				z = shaper_func(z);
				Float3 color = { x, y, z };
				Float3 temp = EvalACES(color, params);
				walk[0] = temp.X;
				walk[1] = temp.Y;
				walk[2] = temp.Z;
				walk[3] = 1.0f;

				walk += 4;
			}
		}
	}
#endif
	DXGI_FORMAT format = USE_FLOAT ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT;
	unsigned int stride = USE_FLOAT ? 16 : 8;

	D3D11_TEXTURE3D_DESC tDesc;

	tDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tDesc.CPUAccessFlags = 0;
	tDesc.Width = current.LUTdimx;
	tDesc.Height = current.LUTdimy;
	tDesc.Depth = current.LUTdimz;
	tDesc.Format = format;
	tDesc.MipLevels = 1;
	tDesc.MiscFlags = 0;
	tDesc.Usage = D3D11_USAGE_IMMUTABLE;

	D3D11_SUBRESOURCE_DATA srData;

	srData.pSysMem = data;
	srData.SysMemPitch = current.LUTdimx * stride;
	srData.SysMemSlicePitch = current.LUTdimx * current.LUTdimy * stride;

	device->CreateTexture3D(&tDesc, &srData, &LUTtex);

	delete[]data;

	D3D11_SHADER_RESOURCE_VIEW_DESC sDesc;
	sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	sDesc.Format = format;
	sDesc.Texture3D.MipLevels = 1;
	sDesc.Texture3D.MostDetailedMip = 0;

	device->CreateShaderResourceView(LUTtex, &sDesc, &LUTsrv);


	D3D11_SAMPLER_DESC sampDesc;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	sampDesc.BorderColor[0] = 0.0f;
	sampDesc.BorderColor[1] = 0.0f;
	sampDesc.BorderColor[2] = 0.0f;
	sampDesc.BorderColor[3] = 0.0f;

	sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sampDesc.MaxAnisotropy = 1;
	sampDesc.MipLODBias = 0.0f;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = 0;

	device->CreateSamplerState(&sampDesc, &LUTsamp);
}


void LDR_ss::SetupTonemapShader(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srcData)
{
	// setup the color matrix
	memcpy(constants.colorMat, &ColorMatrices[settings.selectedColorMatrix * 12], sizeof(float) * 12);
	memcpy(constants.colorMatInv, &ColorMatricesInv[settings.selectedColorMatrix * 12], sizeof(float) * 12);

	// setup the aces data
	SegmentedSplineParams_c9 aces = GetAcesODTData(ODTCurve(settings.selectedCurve), settings.minStops, settings.maxStops, settings.maxLevel, settings.midGrayScale);

	memcpy(constants.ACES_coefs, aces.coefs, sizeof(float) * 40);
	memcpy(constants.ACES_max, &aces.maxPoint, sizeof(float) * 2);
	memcpy(constants.ACES_mid, &aces.midPoint, sizeof(float) * 2);
	memcpy(constants.ACES_min, &aces.minPoint, sizeof(float) * 2);
	memcpy(constants.ACES_slope, &aces.slope, sizeof(float) * 2);

	constants.CinemaLimits[0] = aces.minPoint.Y;
	constants.CinemaLimits[1] = aces.maxPoint.Y;

	constants.flags = settings.adjustWP ? 0x4 : 0x0;
	constants.flags |= settings.desaturate ? 0x2 : 0x0;
	constants.flags |= settings.dimSurround ? 0x1 : 0x0;
	constants.flags |= settings.luminanceOnly ? 0x8 : 0x0;

	constants.OutputMode = settings.outputMode;
	constants.saturation = settings.toneCurveSaturation;
	constants.surroundGamma = settings.surroundGamma;
	constants.gamma = settings.outputGamma;


	D3D11_MAPPED_SUBRESOURCE mapObj;
	ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapObj);
	memcpy(mapObj.pData, &constants, sizeof(constants));
	ctx->Unmap(cb, 0);

	ctx->PSSetConstantBuffers(1, 1, &cb);
	ctx->PSSetShader(shader, nullptr, 0);
	ctx->PSSetShaderResources(0, 1, &srcData);
}