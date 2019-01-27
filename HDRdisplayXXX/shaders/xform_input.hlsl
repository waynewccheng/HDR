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

/*
LAB Colors

Border
0.9658, 0.4963, 0.5024 
*/

////////////////////////////////////////////////////////////////////////////////
// Resources

Texture2D texInput : register(t0);
SamplerState sampLinearWrap : register(s0);
Buffer<float> autoExposure : register(t1);

RWBuffer<float> uav : register(u1);

////////////////////////////////////////////////////////////////////////////////
// IO Structures

struct VS_OUTPUT
{
	float4 P  : SV_POSITION;
	float2 TC : TEXCOORD0;
};


cbuffer cbObject : register(b0)
{
	uint filter;
	int zoom;
	float scale_stops;
	float expansion;
	int2 inDim;
	int2 outDim;
	int match_aspect;
	int tile;
	int2 readback_pos;
	uint applyAutoExposure;
	uint gradingFlags;
	float xlrContrastL;
	float  xlrContrastC;
	float xlrScaleC;
	float xlrBiasA;
	float xlrBiasB;
	float pad;
	float3 rgbSaturation;
	float3 rgbContrast;
	float3 rgbGamma;
	float3 rgbGain;
	float3 rgbBias;
	float pad2; // start on new vec 4
	float iptContrastL;
	float iptContrastC;
	float iptScaleC;
	float iptBiasC;
	float iptBiasA;
	float iptBiasB;
};


////////////////////////////////////////////////////////////////////////////////

// This is unnormalized hunt
static const float3x3 XYZ_2_LMS_MAT =
{
	0.38971f, 0.68898f, -0.07868f,
	-.22981f, 1.1834f, 0.04641f,
	0.0f, 0.0f, 1.0f
};

static const float3x3 LMS_2_XYZ_MAT =
{
	1.9102, -1.1121, 0.2019,
	0.371, 0.6291, -0,
	0, 0, 1
};

static const float3x3 MCAT02 =
{
	0.7328, 0.4296, -0.1624,
	-0.7036, 1.6975, 0.0061,
	0.0030, 0.0136, 0.9834
};

static const float3x3 MCAT02i =
{
	1.096124, -0.278869, 0.182745,
	0.454369, 0.473533, 0.072098,
	-0.009628, -0.005698, 1.015326
};

static const float3x3 Aab_2_LMS_MAT =
{
	1.0000, 0.3215, 0.2053,
	1.0000, -0.6351, -0.1860,
	1.0000, -0.1568, -4.4904
};

static const float3x3 sRGB_2_XYZ_MAT =
{
	0.41239089f, 0.35758430f, 0.18048084f,
	0.21263906f, 0.71516860f, 0.07219233f,
	0.01933082f, 0.11919472f, 0.95053232f
};

static const float3x3 XYZ_2_sRGB_MAT =
{
	3.24096942f, -1.53738296f, -0.49861076f,
	-0.96924388f, 1.87596786f, 0.04155510f,
	0.05563002f, -0.20397684f, 1.05697131f
};

float3 XLR_CAM_Forward(float3 LinearColor, float3 inWhite, float La, float DisplayFactor, bool JCh, bool CAT)
{

	// map to ZYZ
	float3 XYZ;
	XYZ = mul(sRGB_2_XYZ_MAT, LinearColor);

	float3 XYZw = inWhite;

	if (CAT)
	{
		float3 RGB = mul(MCAT02, XYZ);
		float3 RGBw = mul(MCAT02, XYZw);

		float Lw = XYZw.y;

		RGB = RGB / (RGBw / Lw);
		RGBw = RGBw / (RGBw / Lw);

		XYZ = mul(MCAT02i, RGB);
		XYZw = mul(MCAT02i, RGBw);
	}

	//Map to LMS
	float3 LMS = mul(XYZ_2_LMS_MAT, XYZ);


	float3 LMSw = mul(XYZ_2_LMS_MAT, XYZw);

	//Compute cone response
	LMS = pow(LMS, 0.57f) / (pow(LMS, 0.57f) + pow(La, 0.57f));
	LMSw = pow(LMSw, 0.57f) / (pow(LMSw, 0.57f) + pow(La, 0.57f));

	//Compute achromatic response
	float A = (40.0f * LMS.x + 20.0 * LMS.y + LMS.z) / 61.0f;
	float Aw = (40.0f * LMSw.x + 20.0 * LMSw.y + LMSw.z) / 61.0f;

	//Compute lightness
	float J;
	{
		float x = A / Aw;
		const float aj = 0.89f;
		const float bj = 0.24f;
		const float oj = 0.65f;
		const float nj = 3.65f;
		J = 100.0f * pow((-(x - bj) * pow(oj, nj)) / (x - bj - aj), 1.0f / nj);

		if (x <= bj) J = 1.0f;
		if (x >= (bj + aj)) J = 100.0f;

		J /= 100.0f;
		//depends on display device
		const float E = DisplayFactor;
		J = 100.0f * (E * (J - 1.0f) + 1.0f);
		J = max(J, 0.0f);
		J = min(J, 100.0f);
	}

	//Compute Brightness (magic number is luminance of reference white)
	float Q = J * pow(100.0f, 0.1308f);

	//Compute Colorfulness
	float a = (11.0f*LMS.x - 12.0f * LMS.y + LMS.z) / 11.0f;
	float b = (LMS.x + LMS.y - 2.0f * LMS.z) / 9.0f;
	float C = 456.5f * pow(sqrt(a*a + b*b), 0.62f);
	float M = C * (0.11f * log10(100.0f) + 0.61f);

	//Compute saturation
	float saturation = 100.0f * sqrt(M / Q);

	//compute hues
	float hue = 180.0f / 3.14159f * atan2(b, a);

	hue += hue < 0.0f ? 360.0 : 0.0f;

	// Return lightness, Color, hue, color can either be encoded as Chromacity or colorfulness
	float color = JCh ? C : M;
	return float3(J, color, hue);

}

float3 XLR_CAM_Inverse(float3 Color, float3 outWhite, float La, float DisplayFactor, bool JCh, bool CAT)
{

	float J = Color.x;
	float C = Color.y;
	float hue = Color.z;

	if (!JCh)
	{
		// Color.y represents colorfulness (M) instead of Chroma (C)
		const float Ma = 0.11;
		const float Mb = 0.61;
		C = Color.y / (Ma*log10(outWhite.y) + Mb);
	}

	//compute a white based on present exposure
	float3 XYZw = outWhite;


	if (CAT)
	{
		float3 RGBw = mul(MCAT02, XYZw);

			float Lw = XYZw.y;

		RGBw = RGBw / (RGBw / Lw);

		XYZw = mul(MCAT02i, RGBw);
		}
		else
		{

			float3 RGBw = mul(MCAT02, XYZw);

				float Lw = XYZw.y;

			RGBw = RGBw / (RGBw.y / Lw);

			XYZw = mul(MCAT02i, RGBw);
		}

	//Map to LMS

	float3 LMSw = mul(XYZ_2_LMS_MAT, XYZw);

	//Compute cone response
	LMSw = pow(LMSw, 0.57f) / (pow(LMSw, 0.57f) + pow(La, 0.57f));

	//Compute achromatic response
	float Aw = (40.0f * LMSw.x + 20.0 * LMSw.y + LMSw.z) / 61.0f;


	// compute rgb through the inverse function

	const float mD = DisplayFactor;
	float Jp = (1.0f / mD)*(J / 100.0f - 1.0f) + 1.0f;

	const float Aa = 0.89;
	const float Ab = 0.24;
	const float As = 0.65;
	const float An = 3.65;
	float A = Aw*(Aa*pow(Jp, An) / (pow(Jp, An) + pow(As, An)) + Ab);

	const float Ca = 465.5;
	const float Cn = 0.62;
	float CC = pow((C / Ca), (1.0 / Cn));
	float r = (3.14159f / 180.0f);
	float a = cos(r*hue)*CC;
	float b = sin(r*hue)*CC;
	float3 LMSp;
	LMSp = mul(Aab_2_LMS_MAT, float3(A, a, b));
	const float Hn = 0.57;
	LMSp = max(LMSp, 0.0f);
	float3 LMS = pow((-pow(La, Hn)*LMSp / (LMSp - 1.0)), (1.0f / Hn));

		// original code seems to clamp S to max( L, M) for the entire image ???
		/*
		lmax = max(LMS(:, 1));
		mmax = max(LMS(:, 2));
		sclamp = max([lmax, mmax]);
		lms_s = LMS(:, 3);
		lms_s(lms_s > sclamp) = sclamp;
		LMS(:, 3) = lms_s;
		*/
#if 0
		if (HDR_extra.y < 0.0f)
		{
		float sclamp = max(LMS.x, LMS.y);
		LMS.z = min(LMS.z, sclamp);
		}
#endif

	float3 XYZp = mul(LMS_2_XYZ_MAT, LMS);

	if (CAT)
	{
		float3 XYZw = outWhite;

		//XYZw.y *= HDR_scale.x;

		float3 RGBw = mul(MCAT02, XYZw);
		RGBw /= RGBw.y;
		XYZp = mul(MCAT02, XYZp);
		XYZp *= RGBw;
		XYZp = mul(MCAT02i, XYZp);
	}

	float3 RGB = mul(XYZ_2_sRGB_MAT, XYZp);

	return RGB;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader

static const float3x3 XYZ_2_DISPLAY_PRI_MAT =
{
	3.24096942f, -1.53738296f, -0.49861076f,
	-0.96924388f, 1.87596786f, 0.04155510f,
	0.05563002f, -0.20397684f, 1.05697131f
};

float4 main(VS_OUTPUT input) : SV_Target0
{
	float4 texLookup = 0;

	if (filter)
	{
		float aspectIn = float(inDim.x) / float(inDim.y);
		float aspectOut = float(outDim.x) / float(outDim.y);
		float2 aspectScale = 1;
		if (match_aspect)
		{
			aspectScale.x = aspectIn < aspectOut ? aspectOut / aspectIn : 1.0f;
			aspectScale.y = aspectIn > aspectOut ? aspectIn / aspectOut : 1.0f;
		}
		float2 uv = (input.TC - 0.5) * pow(2.0f, float(-zoom)) *aspectScale + 0.5;
		texLookup = texInput.SampleLevel(sampLinearWrap, uv, 0);
		if (tile == 0 && (any(uv < 0.0) || any(uv > 1.0)))
			texLookup = 0.0;
	}
	else
	{
		float scale = pow(2.0f, float(-zoom));
		int2 offset = (inDim/scale - outDim) / 2;
		int2 uv = (input.P.xy + offset) * scale;
		texLookup = texInput.Load(int3(uv, 0));
	}

	// Filter any input NaNs (convoluted logic handles HLSL compiler transforms)
	texLookup.rgb = max(-65505.0f, texLookup.rgb);
	texLookup.rgb = (texLookup.rgb == -65505.0f) ? 0.0f : texLookup.rgb;

	if (int(input.P.x) == readback_pos.x && int(input.P.y) == readback_pos.y)
	{
		uav[0] = texLookup.r;
		uav[1] = texLookup.g;
		uav[2] = texLookup.b;
	}

	float3 rgb = texLookup.rgb;

	if (applyAutoExposure)
	{
		float exposure = autoExposure.Load(0);

		rgb *= 0.18f / exp2(exposure);
	}

	// adjust exposure
	if (scale_stops != 0.0f)
	{
		rgb *= exp2(scale_stops);
	}

	// expand / contract color range
	if (expansion != 1.0f)
	{
		float3 trgb = pow( abs(rgb) / 0.18, expansion) * 0.18;
		rgb = rgb < 0.0f ? -trgb : trgb;
	}

	bool grade = (gradingFlags & 0x8) ? input.TC.x >= 0.5f : true;

	// grade in xlrCAM space
	if (gradingFlags & 0x1 && grade)
	{
		const float3 white = mul(sRGB_2_XYZ_MAT, float3(1, 1, 1)) / 80.0f * 10000.0f; // white is 10,000 nit
		float3 Jch = XLR_CAM_Forward(rgb, white, 0.001f, 1.0f, true, false);

		// adjust contrast
		Jch.x = 100.0f * pow(Jch.x / 100.0f, xlrContrastL);

		// adjust colorfulness
		Jch.y = 456.5f * pow(Jch.y / 456.5f, xlrContrastC) * xlrScaleC;

		const float Ca = 465.5;
		const float Cn = 0.62;
		float CC = pow((Jch.y / Ca), (1.0 / Cn));
		float r = (3.14159f / 180.0f);
		float a = cos(r*Jch.z);
		float b = sin(r*Jch.z);

		// how to get the scale appropriate?
		a = max( min( a + xlrBiasA, 1.0f), -1.0f);
		b = max(min(b + xlrBiasB, 1.0f), -1.0f);

		a *= CC;
		b *= CC;

		Jch.y = 456.5f * pow(sqrt(a*a + b*b), 0.62f);
		Jch.y = max(456.5f, Jch.y);

		//compute hues
		Jch.z = 180.0f / 3.14159f * atan2(b, a);

		Jch.z += Jch.z < 0.0f ? 360.0 : 0.0f;

		rgb = XLR_CAM_Inverse(Jch, white, 0.001f, 1.0f, true, false);
	}

	// RGB grading
	if (gradingFlags & 0x2 && grade)
	{
		// use ACES space to grade
		static const float3x3 XYZ_2_AP1_MAT =
		{
			1.64102352f, -0.32480335f, -0.23642471f,
			-0.66366309f, 1.61533189f, 0.01675635f,
			0.01172191f, -0.00828444f, 0.98839492f
		};

		static const float3x3 AP1_2_XYZ_MAT =
		{
			0.66245413f, 0.13400421f, 0.15618768f,
			0.27222872f, 0.67408168f, 0.05368952f,
			-0.00557466f, 0.00406073f, 1.01033902f
		};

		static const float3 AP1_RGB2Y = { 0.27222872f, 0.67408168f, 0.05368952f };

		rgb = mul(XYZ_2_AP1_MAT, mul(sRGB_2_XYZ_MAT, rgb));

		// saturation
		float l = dot(rgb, AP1_RGB2Y);
		rgb = lerp(l, rgb, rgbSaturation);

		// contrast
		rgb = pow(max(rgb / 0.18, 0.0), rgbContrast) *0.18;
		
		// gamma
		rgb = pow(rgb, rgbGamma);

		// gain + bias
		rgb = rgb*rgbGain + rgbBias;

		rgb = mul(XYZ_2_sRGB_MAT, mul(AP1_2_XYZ_MAT, rgb));
	}

	// IPT grading
	if (gradingFlags & 0x4 && grade)
	{
		/*
		// CIECAM02 based LMS transforms
		static const float3x3 XYZ_2_LMS_MAT =
		{
			0.4002f, 0.7075f, -0.0807f,
			-0.2280f, 1.1500f, 0.0612f,
			0.0000f, 0.0000f, 0.9184f
		};

		static const float3x3 LMS_2_XYZ_MAT =
		{
			1.0000, 0.0976, 0.2052,
			1.0000, -1.1139, 0.1332,
			1.0000, 0.0326, -0.6769
		};
		*/

		static const float3x3 LMS_2_IPT_MAT =
		{
			0.4000, 0.4000, 0.2000,
			4.4550, -4.8510, 0.3960,
			0.8056, 0.3572, -1.1628
		};
#if 0
		static const float3x3 IPT_2_LMS_MAT =
		{
			1.8502, -1.1383, 0.2384,
			0.3668, 0.6439, -0.0107,
			0.0000, 0.0000, 1.0889
		};
#else
		static const float3x3 IPT_2_LMS_MAT =
		{
			1.00000000, 0.09756894, 0.20522645,
			1.00000000, -0.11387650, 0.13321717,
			1.00000012, 0.03261511, -0.67688727
		};
#endif
		float3 midGray = 0.18;
		midGray = mul(XYZ_2_LMS_MAT, mul(sRGB_2_XYZ_MAT, midGray));
		midGray = midGray < 0.0 ? -pow(-midGray, 0.43) : pow(midGray, 0.43);
		midGray = mul(LMS_2_IPT_MAT, midGray);

		float3 lms = mul(XYZ_2_LMS_MAT, mul(sRGB_2_XYZ_MAT, rgb));

		lms = lms < 0.0 ? -pow(-lms, 0.43) : pow(lms, 0.43);

		float3 ipt = mul(LMS_2_IPT_MAT, lms);

		// chrominance / colorfullness adjustment
		float chroma = length(ipt.yz);
		float2 ab = ipt.yz / chroma;
		chroma = (pow(chroma / ipt.x, 1.0 / iptContrastC) * iptScaleC + iptBiasC) * ipt.x;

		ab.x += iptBiasA;
		ab.y += iptBiasB;
		ipt.yz = ab * chroma;

		// luminance contrast
		ipt.x = pow(ipt.x / midGray.x, iptContrastL) * midGray.x;
		

		lms = mul(IPT_2_LMS_MAT, ipt);

		lms = lms < 0.0 ? -pow(-lms, 1.0 / 0.43) : pow(lms, 1.0 / 0.43);

		rgb = mul(XYZ_2_sRGB_MAT, mul(LMS_2_XYZ_MAT, lms));
	}

	return float4(rgb, 1);
}


