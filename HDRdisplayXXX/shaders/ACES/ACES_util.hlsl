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

static const float TINY = 1e-10;
static const float M_PI = 3.1415927f;
static const float HALF_MAX = 65504.0f;

static const float3x3 AP0_2_XYZ_MAT = 
{
	0.95255238f, 0.00000000f, 0.00009368f,
	0.34396642f, 0.72816616f, -0.07213254f,
	-0.00000004f, 0.00000000f, 1.00882506f
};

static const float3x3 XYZ_2_AP0_MAT =
{
	1.04981101f, -0.00000000f, -0.00009748f,
	-0.49590296f, 1.37331295f, 0.09824003f,
	0.00000004f, -0.00000000f, 0.99125212f
};

static const float3x3 AP1_2_XYZ_MAT =
{
	0.66245413f, 0.13400421f, 0.15618768f,
	0.27222872f, 0.67408168f, 0.05368952f,
	-0.00557466f, 0.00406073f, 1.01033902f
};

static const float3x3 XYZ_2_AP1_MAT =
{
	1.64102352f, -0.32480335f, -0.23642471f,
	-0.66366309f, 1.61533189f, 0.01675635f,
	0.01172191f, -0.00828444f, 0.98839492f
};

static const float3x3 AP0_2_AP1_MAT = 
{
	1.45143950f, -0.23651081f, -0.21492855f,
	-0.07655388f, 1.17623007f, -0.09967594f,
	0.00831613f, -0.00603245f, 0.99771625f
};

static const float3x3 AP1_2_AP0_MAT = 
{
	0.69545215f, 0.14067869f, 0.16386905f,
	0.04479461f, 0.85967094f, 0.09553432f,
	-0.00552587f, 0.00402521f, 1.00150073f
};

// EHart - need to check this, might be a transpose issue with CTL
static const float3 AP1_RGB2Y = { 0.27222872f, 0.67408168f, 0.05368952f };


float max_f3(float3 In)
{
	return max(In.r, max(In.g, In.b));
}

float min_f3(float3 In)
{
	return min(In.r, min(In.g, In.b));
}

float rgb_2_saturation(float3 rgb)
{
	return (max(max_f3(rgb), TINY) - max(min_f3(rgb), TINY)) / max(max_f3(rgb), 1e-2);
}

/* ---- Conversion Functions ---- */
// Various transformations between color encodings and data representations
//

// Transformations between CIE XYZ tristimulus values and CIE x,y 
// chromaticity coordinates
float3 XYZ_2_xyY(float3 XYZ)
{
	float3 xyY;
	float divisor = (XYZ[0] + XYZ[1] + XYZ[2]);
	if (divisor == 0.) divisor = 1e-10;
	xyY[0] = XYZ[0] / divisor;
	xyY[1] = XYZ[1] / divisor;
	xyY[2] = XYZ[1];

	return xyY;
}

float3 xyY_2_XYZ( float3 xyY)
{
	float3 XYZ;
	XYZ[0] = xyY[0] * xyY[2] / max(xyY[1], 1e-10);
	XYZ[1] = xyY[2];
	XYZ[2] = (1.0 - xyY[0] - xyY[1]) * xyY[2] / max(xyY[1], 1e-10);

	return XYZ;
}


// Transformations from RGB to other color representations
float rgb_2_hue(float3 rgb)
{
	// Returns a geometric hue angle in degrees (0-360) based on RGB values.
	// For neutral colors, hue is undefined and the function will return a quiet NaN value.
	float hue;
	if (rgb[0] == rgb[1] && rgb[1] == rgb[2]) {
		// RGB triplets where RGB are equal have an undefined hue
		// EHart - reference code uses NaN, use 0 instead to prevent propagation of NaN
		hue = 0.0f; 
	}
	else {
		hue = (180. / M_PI) * atan2(sqrt(3)*(rgb[1] - rgb[2]), 2 * rgb[0] - rgb[1] - rgb[2]);
	}

	if (hue < 0.) hue = hue + 360.;

	return hue;
}

float rgb_2_yc(float3 rgb, float ycRadiusWeight = 1.75)
{
	// Converts RGB to a luminance proxy, here called YC
	// YC is ~ Y + K * Chroma
	// Constant YC is a cone-shaped surface in RGB space, with the tip on the 
	// neutral axis, towards white.
	// YC is normalized: RGB 1 1 1 maps to YC = 1
	//
	// ycRadiusWeight defaults to 1.75, although can be overridden in function 
	// call to rgb_2_yc
	// ycRadiusWeight = 1 -> YC for pure cyan, magenta, yellow == YC for neutral 
	// of same value
	// ycRadiusWeight = 2 -> YC for pure red, green, blue  == YC for  neutral of 
	// same value.

	float r = rgb[0];
	float g = rgb[1];
	float b = rgb[2];

	float chroma = sqrt(b*(b - g) + g*(g - r) + r*(r - b));

	return (b + g + r + ycRadiusWeight * chroma) / 3.;
}

/* ODT utility functions */
float Y_2_linCV(float Y, float Ymax, float Ymin)
{
	return (Y - Ymin) / (Ymax - Ymin);
}

float linCV_2_Y(float linCV, float Ymax, float Ymin)
{
	return linCV * (Ymax - Ymin) + Ymin;
}

// Gamma compensation factor
static const float DIM_SURROUND_GAMMA = 0.9811;

float3 darkSurround_to_dimSurround(float3 linearCV)
{
	float3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

	float3 xyY = XYZ_2_xyY(XYZ);
	//xyY[2] = clamp(xyY[2], 0., HALF_POS_INF);
	xyY[2] = max(xyY[2], 0.);
	xyY[2] = pow(xyY[2], DIM_SURROUND_GAMMA);
	XYZ = xyY_2_XYZ(xyY);

	return mul(XYZ_2_AP1_MAT, XYZ);
}

float3 dimSurround_to_darkSurround(float3 linearCV)
{
	float3 XYZ = mul(AP1_2_XYZ_MAT,linearCV);

	float3 xyY = XYZ_2_xyY(XYZ);
	//xyY[2] = clamp(xyY[2], 0., HALF_POS_INF);
	xyY[2] = max(xyY[2], 0.);
	xyY[2] = pow(xyY[2], 1. / DIM_SURROUND_GAMMA);
	XYZ = xyY_2_XYZ(xyY);

	return mul(XYZ_2_AP1_MAT,XYZ);
}

float3 alter_surround(float3 linearCV, float gamma)
{
	float3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

		float3 xyY = XYZ_2_xyY(XYZ);
		//xyY[2] = clamp(xyY[2], 0., HALF_POS_INF);
		xyY[2] = max(xyY[2], 0.);
	xyY[2] = pow(xyY[2], gamma);
	XYZ = xyY_2_XYZ(xyY);

	return mul(XYZ_2_AP1_MAT, XYZ);
}

float3x3 transpose_f33(float3x3 inM)
{
	float3x3 M;

	M[0][0] = inM[0][0];
	M[1][0] = inM[0][1];
	M[2][0] = inM[0][2];
			  
	M[0][1] = inM[1][0];
	M[1][1] = inM[1][1];
	M[2][1] = inM[1][2];
			  
	M[0][2] = inM[2][0];
	M[1][2] = inM[2][1];
	M[2][2] = inM[2][2];

	return M;
}

float3x3 calc_sat_adjust_matrix( float sat, float3 rgb2Y)
{
	//
	// This function determines the terms for a 3x3 saturation matrix that is
	// based on the luminance of the input.
	//
	float3x3 M;
	M[0][0] = (1.0 - sat) * rgb2Y[0] + sat;
	M[1][0] = (1.0 - sat) * rgb2Y[0];
	M[2][0] = (1.0 - sat) * rgb2Y[0];

	M[0][1] = (1.0 - sat) * rgb2Y[1];
	M[1][1] = (1.0 - sat) * rgb2Y[1] + sat;
	M[2][1] = (1.0 - sat) * rgb2Y[1];

	M[0][2] = (1.0 - sat) * rgb2Y[2];
	M[1][2] = (1.0 - sat) * rgb2Y[2];
	M[2][2] = (1.0 - sat) * rgb2Y[2] + sat;

	// EHart - removed transpose, as the indexing in CTL is transposed

	return M;
}

/* ---- Signal encode/decode functions ---- */

float moncurve_f(float x, float gamma, float offs)
{
	// Forward monitor curve
	float y;
	const float fs = ((gamma - 1.0) / offs) * pow(offs * gamma / ((gamma - 1.0) * (1.0 + offs)), gamma);
	const float xb = offs / (gamma - 1.0);
	if (x >= xb)
		y = pow((x + offs) / (1.0 + offs), gamma);
	else
		y = x * fs;
	return y;
}

float moncurve_r(float y, float gamma, float offs)
{
	// Reverse monitor curve
	float x;
	const float yb = pow(offs * gamma / ((gamma - 1.0) * (1.0 + offs)), gamma);
	const float rs = pow((gamma - 1.0) / offs, gamma - 1.0) * pow((1.0 + offs) / gamma, gamma);
	if (y >= yb)
		x = (1.0 + offs) * pow(y, 1.0 / gamma) - offs;
	else
		x = y * rs;
	return x;
}

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
float pq_f(float N)
{
	// Note that this does NOT handle any of the signal range
	// considerations from 2084 - this assumes full range (0 - 1)
	float Np = pow(N, 1.0 / pq_m2);
	float L = Np - pq_c1;
	if (L < 0.0)
		L = 0.0;
	L = L / (pq_c2 - pq_c3 * Np);
	L = pow(L, 1.0 / pq_m1);
	return L * pq_C; // returns cd/m^2
}

// Converts from linear cd/m^2 to the non-linear perceptually quantized space
// Note that this is in float, and assumes normalization from 0 - 1
// (0 - pq_C for linear) and does not handle the integer coding in the Annex 
// sections of SMPTE ST 2084-2014
float pq_r(float C)
{
	// Note that this does NOT handle any of the signal range
	// considerations from 2084 - this returns full range (0 - 1)
	float L = C / pq_C;
	float Lm = pow(L, pq_m1);
	float N = (pq_c1 + pq_c2 * Lm) / (1.0 + pq_c3 * Lm);
	N = pow(N, pq_m2);
	return N;
}

float3 pq_r_f3(float3 In)
{
	// converts from linear cd/m^2 to PQ code values

	float3 Out;
	Out[0] = pq_r(In[0]);
	Out[1] = pq_r(In[1]);
	Out[2] = pq_r(In[2]);

	return Out;
}

float3 pq_f_f3(float3 In)
{
	// converts from PQ code values to linear cd/m^2

	float3 Out;
	Out[0] = pq_f(In[0]);
	Out[1] = pq_f(In[1]);
	Out[2] = pq_f(In[2]);

	return Out;
}