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

#include "ACES_rrt.hlsl"
#include "../utilities.hlsl"


////////////////////////////////////////////////////////////////////////////////
// Resources

Texture2D texInput : register(t0);
SamplerState sampLinearWrap : register(s0);

////////////////////////////////////////////////////////////////////////////////
// IO Structures

struct VS_OUTPUT
{
	float4 P  : SV_POSITION;
	float2 TC : TEXCOORD0;
};

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader

static const float DISPGAMMA = 2.4;
static const float OFFSET = 0.055;


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
	0.05563002f, -0.20397684f, 1.05697131f,
};


//
// Parameterized ACES ODT
// Uses uniforms to setup the curve parameters to prevent excessive
// constant folding and runtime branching in the HLSL.
//

cbuffer Constants : register(b1)
{
	float2 ACES_min;
	float2 ACES_mid;
	float2 ACES_max;
	float2 ACES_slope;
	float4 ACES_coefs[10];
	row_major float3x3 XYZ_2_DISPLAY_PRI_MAT;
	row_major float3x3 DISPLAY_PRI_MAT_2_XYZ;
	float2 CinemaLimits;
	int OutputMode;
	uint Flags;
	float surroundGamma;
	float saturation;
	float postScale;
	float gamma;
};

float uniform_segmented_spline_c9_fwd(float x)
{
	const int N_KNOTS_LOW = 8;
	const int N_KNOTS_HIGH = 8;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to OCESMIN.
	float xCheck = x <= 0 ? 1e-4 : x;

	float logx = log10(xCheck);
	float logy;

	if (logx <= log10(ACES_min.x))
	{
		logy = logx * ACES_slope.x + (log10(ACES_min.y) - ACES_slope.x * log10(ACES_min.x));
	}
	else if ((logx > log10(ACES_min.x)) && (logx < log10(ACES_mid.x)))
	{
		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(ACES_min.x)) / (log10(ACES_mid.x) - log10(ACES_min.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = { ACES_coefs[j].x, ACES_coefs[j + 1].x, ACES_coefs[j + 2].x };

			float3 monomials = { t * t, t, 1 };
			logy = dot(monomials, mul(cf, M));
	}
	else if ((logx >= log10(ACES_mid.x)) && (logx < log10(ACES_max.x)))
	{
		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(ACES_mid.x)) / (log10(ACES_max.x) - log10(ACES_mid.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = { ACES_coefs[j].y, ACES_coefs[j + 1].y, ACES_coefs[j + 2].y };

			float3 monomials = { t * t, t, 1 };
			logy = dot(monomials, mul(cf, M));
	}
	else//if ( logIn >= log10(ACES_max.x) )
	{
		logy = logx * ACES_slope.y + (log10(ACES_max.y) - ACES_slope.y * log10(ACES_max.x));
	}

	return pow(10, logy);
}

static const float3x3 D65_2_D60_CAT =
{
	1.01303, 0.00610531, -0.014971,
	0.00769823, 0.998165, -0.00503203,
	-0.00284131, 0.00468516, 0.924507,
};
/*
static const float3 AP1_RGB2Y =
{
	0.2722287168, //AP1_2_XYZ_MAT[0][1],
	0.6740817658, //AP1_2_XYZ_MAT[1][1],
	0.0536895174, //AP1_2_XYZ_MAT[2][1]
};
*/

//Derived from rec709 ODT
float4 main(VS_OUTPUT input) : SV_Target0
{
	float4 texLookup = texInput.SampleLevel(sampLinearWrap, input.TC, 0);

	float3 aces = mul(XYZ_2_AP0_MAT, mul(D65_2_D60_CAT,	mul(sRGB_2_XYZ_MAT, texLookup.rgb)));

	float3 oces = rrt(aces);

	// OCES to RGB rendering space
	float3 rgbPre = mul(AP0_2_AP1_MAT, oces);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	if (Flags & 0x8)
	{
		// luminance only path, for content that has been mastered for an expectation of an oversaturated tonemap operator
		float y = dot(rgbPre, AP1_RGB2Y);
		float scale = uniform_segmented_spline_c9_fwd(y) / y;

		// compute the more desaturated per-channel version
		rgbPost[0] = uniform_segmented_spline_c9_fwd(rgbPre[0]);
		rgbPost[1] = uniform_segmented_spline_c9_fwd(rgbPre[1]);
		rgbPost[2] = uniform_segmented_spline_c9_fwd(rgbPre[2]);

		// lerp between values
		rgbPost = max(lerp(rgbPost, rgbPre * scale, saturation), CinemaLimits.x); // clamp to min to prevent the genration of negative values
	}
	else
	{
		rgbPost[0] = uniform_segmented_spline_c9_fwd(rgbPre[0]);
		rgbPost[1] = uniform_segmented_spline_c9_fwd(rgbPre[1]);
		rgbPost[2] = uniform_segmented_spline_c9_fwd(rgbPre[2]);
	}

	// Scale luminance to linear code value
	float3 linearCV;
	linearCV[0] = Y_2_linCV(rgbPost[0], CinemaLimits.y, CinemaLimits.x);
	linearCV[1] = Y_2_linCV(rgbPost[1], CinemaLimits.y, CinemaLimits.x);
	linearCV[2] = Y_2_linCV(rgbPost[2], CinemaLimits.y, CinemaLimits.x);

	if (Flags & 0x1)
	{
		// Apply gamma adjustment to compensate for surround
		linearCV = alter_surround(linearCV, surroundGamma);
	}

	if (Flags & 0x2)
	{
		// Apply desaturation to compensate for luminance difference
		// Saturation compensation factor
		const float ODT_SAT_FACTOR = 0.93;
		const float3x3 ODT_SAT_MAT = calc_sat_adjust_matrix(ODT_SAT_FACTOR, AP1_RGB2Y);
		linearCV = mul(ODT_SAT_MAT, linearCV);
	}

	// Convert to display primary encoding
	// Rendering space RGB to XYZ
	float3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

	if (Flags & 0x4)
	{
		// Apply CAT from ACES white point to assumed observer adapted white point
		// EHart - should recompute this matrix
		const float3x3 D60_2_D65_CAT =
		{
			0.987224, -0.00611327, 0.0159533,
			-0.00759836, 1.00186, 0.00533002,
			0.00307257, -0.00509595, 1.08168,
		};
		XYZ = mul(D60_2_D65_CAT, XYZ);
	}

	// CIE XYZ to display primaries
	linearCV = mul(XYZ_2_DISPLAY_PRI_MAT, XYZ);

	// Encode linear code values with transfer function
	float3 outputCV = linearCV;

	if (OutputMode == 0)
	{
		// LDR mode, clamp 0/1 and encode 
		linearCV = clamp(linearCV, 0., 1.);

		outputCV[0] = moncurve_r(linearCV[0], DISPGAMMA, OFFSET);
		outputCV[1] = moncurve_r(linearCV[1], DISPGAMMA, OFFSET);
		outputCV[2] = moncurve_r(linearCV[2], DISPGAMMA, OFFSET);
	}
	else if (OutputMode == 1)
	{
		//scale to bring the ACES data back to the proper range
		linearCV[0] = linCV_2_Y(linearCV[0], CinemaLimits.y, CinemaLimits.x);
		linearCV[1] = linCV_2_Y(linearCV[1], CinemaLimits.y, CinemaLimits.x);
		linearCV[2] = linCV_2_Y(linearCV[2], CinemaLimits.y, CinemaLimits.x);

		// Handle out-of-gamut values
		// Clip values < 0 (i.e. projecting outside the display primaries)
		//rgb = clamp(rgb, 0., HALF_POS_INF);
		linearCV = max(linearCV, 0.);

		// Encode with PQ transfer function
		outputCV = pq_r_f3(linearCV);
	}
	else if (OutputMode == 2)
	{
		// output in scRGB

		//scale to bring the ACES data back to the proper range
		linearCV[0] = linCV_2_Y(linearCV[0], CinemaLimits.y, CinemaLimits.x);
		linearCV[1] = linCV_2_Y(linearCV[1], CinemaLimits.y, CinemaLimits.x);
		linearCV[2] = linCV_2_Y(linearCV[2], CinemaLimits.y, CinemaLimits.x);

		// Handle out-of-gamut values
		// Clip values < 0 (i.e. projecting outside the display primaries)
		linearCV = max(linearCV, 0.);

		// convert from eported display primaries to sRGB primaries
		linearCV = mul( DISPLAY_PRI_MAT_2_XYZ, linearCV);
		linearCV = mul( XYZ_2_sRGB_MAT, linearCV);

		// map 1.0 to 80 nits (or max nit level if it is lower)
		//outputCV = (linearCV / min(80.0f, CinemaLimits.y) ) * postScale;
		float3 ccSpace = (linearCV / min(80.0f, CinemaLimits.y));
		// quantize
		if (0 &&postScale != 1.0)
		{
			const float bits = 6.0;
			ccSpace = round(ccSpace * (pow(2.0, bits) - 1.0)) / (pow(2.0, bits) - 1.0);
		}
		outputCV = ccSpace * postScale;
	}
	else if (OutputMode == 3)
	{
		// LDR mode, clamp 0/1 and encode 
		linearCV = clamp(linearCV, 0., 1.);

		outputCV = outputCV > 0.0f ? pow(linearCV, 1.0f / gamma) : 0.0f;

	}
	

	if (OutputMode != 2)
	{
		//encode it with sRGB, to counteract a Windows display driver transform
		outputCV = sRGB_2_Linear(outputCV);
	}

	return float4(outputCV, 1);
}


