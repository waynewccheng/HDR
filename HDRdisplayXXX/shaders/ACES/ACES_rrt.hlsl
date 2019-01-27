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

#include "ACES_util.hlsl"
#include "ACES_tonescales.hlsl"

float glow_fwd(float ycIn, float glowGainIn, float glowMid)
{
	float glowGainOut;

	if (ycIn <= 2. / 3. * glowMid) {
		glowGainOut = glowGainIn;
	}
	else if (ycIn >= 2. * glowMid) {
		glowGainOut = 0.;
	}
	else {
		glowGainOut = glowGainIn * (glowMid / ycIn - 1. / 2.);
	}

	return glowGainOut;
}

float cubic_basis_shaper( float x, float w   /* full base width of the shaper function (in degrees)*/)
{
	const float M[4][4] =
	{
		{ -1. / 6, 3. / 6, -3. / 6, 1. / 6 },
		{ 3. / 6, -6. / 6, 3. / 6, 0. / 6 },
		{ -3. / 6, 0. / 6, 3. / 6, 0. / 6 },
		{ 1. / 6, 4. / 6, 1. / 6, 0. / 6 }
	};

	float knots[5] =
	{
		-w / 2.,
		-w / 4.,
		0.,
		w / 4.,
		w / 2.
	};

	// EHart - init y, because CTL does by default
	float y = 0;
	if ((x > knots[0]) && (x < knots[4])) {
		float knot_coord = (x - knots[0]) * 4. / w;
		int j = knot_coord;
		float t = knot_coord - j;

		float monomials[4] = { t*t*t, t*t, t, 1. };

		// (if/else structure required for compatibility with CTL < v1.5.)
		if (j == 3) {
			y = monomials[0] * M[0][0] + monomials[1] * M[1][0] +
				monomials[2] * M[2][0] + monomials[3] * M[3][0];
		}
		else if (j == 2) {
			y = monomials[0] * M[0][1] + monomials[1] * M[1][1] +
				monomials[2] * M[2][1] + monomials[3] * M[3][1];
		}
		else if (j == 1) {
			y = monomials[0] * M[0][2] + monomials[1] * M[1][2] +
				monomials[2] * M[2][2] + monomials[3] * M[3][2];
		}
		else if (j == 0) {
			y = monomials[0] * M[0][3] + monomials[1] * M[1][3] +
				monomials[2] * M[2][3] + monomials[3] * M[3][3];
		}
		else {
			y = 0.0;
		}
	}

	return y * 3 / 2.;
}


float sigmoid_shaper(float x)
{
	// Sigmoid function in the range 0 to 1 spanning -2 to +2.

	float t = max(1. - abs(x / 2.), 0.);
	float y = 1. + sign(x) * (1. - t * t);

	return y / 2.;
}

float center_hue(float hue, float centerH)
{
	float hueCentered = hue - centerH;
	if (hueCentered < -180.) hueCentered = hueCentered + 360.;
	else if (hueCentered > 180.) hueCentered = hueCentered - 360.;
	return hueCentered;
}

float3 rrt( float3 rgbIn)
{
	
	// "Glow" module constants
	const float RRT_GLOW_GAIN = 0.05;
	const float RRT_GLOW_MID = 0.08;
	// --- Glow module --- //
	float saturation = rgb_2_saturation(rgbIn);
	float ycIn = rgb_2_yc(rgbIn);
	float s = sigmoid_shaper((saturation - 0.4) / 0.2);
	float addedGlow = 1. + glow_fwd(ycIn, RRT_GLOW_GAIN * s, RRT_GLOW_MID);

	float3 aces = addedGlow * rgbIn;


	// Red modifier constants
	const float RRT_RED_SCALE = 0.82;
	const float RRT_RED_PIVOT = 0.03;
	const float RRT_RED_HUE = 0.;
	const float RRT_RED_WIDTH = 135.;
	// --- Red modifier --- //
	float hue = rgb_2_hue(aces);
	float centeredHue = center_hue(hue, RRT_RED_HUE);
	float hueWeight = cubic_basis_shaper(centeredHue, RRT_RED_WIDTH);

	aces[0] = aces[0] + hueWeight * saturation *(RRT_RED_PIVOT - aces[0]) * (1. - RRT_RED_SCALE);


	// --- ACES to RGB rendering space --- //
	aces = max(aces, 0.0f);  // avoids saturated negative colors from becoming positive in the matrix

	

	float3 rgbPre = mul(AP0_2_AP1_MAT, aces);

	rgbPre = clamp(rgbPre, 0., HALF_MAX);

	// Desaturation contants
	const float RRT_SAT_FACTOR = 0.96;
	const float3x3 RRT_SAT_MAT = calc_sat_adjust_matrix(RRT_SAT_FACTOR, AP1_RGB2Y);
	// --- Global desaturation --- //
	rgbPre = mul(RRT_SAT_MAT, rgbPre);


	// --- Apply the tonescale independently in rendering-space RGB --- //
	float3 rgbPost;
	rgbPost[0] = segmented_spline_c5_fwd(rgbPre[0]);
	rgbPost[1] = segmented_spline_c5_fwd(rgbPre[1]);
	rgbPost[2] = segmented_spline_c5_fwd(rgbPre[2]);

	// --- RGB rendering space to OCES --- //
	float3 rgbOces = mul(AP1_2_AP0_MAT, rgbPost);

	return rgbOces;
}