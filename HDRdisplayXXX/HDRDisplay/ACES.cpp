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

#define NOMINMAX
#include "ACES.h"

#include <algorithm>
using std::max;
using std::min;



/////////////////////////////////////////////////////////////////////////////////////////
//
//  ACES code
//
////////////////////////////////////////////////////////////////////////////////////////




/*
 * Struct with RRT spline parameters
 */
struct SegmentedSplineParams_c5
{
	float coefsLow[6];    // coefs for B-spline between minPoint and midPoint (units of log luminance)
	float coefsHigh[6];   // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	Float2 minPoint; // {luminance, luminance} linear extension below this
	Float2 midPoint; // {luminance, luminance} 
	Float2 maxPoint; // {luminance, luminance} linear extension above this
	float slopeLow;       // log-log slope of low linear extension
	float slopeHigh;      // log-log slope of high linear extension
};

/*
* Struct with ODT spline parameters
*/
struct SegmentedSplineParams_c9_internal
{
	float coefsLow[10];    // coefs for B-spline between minPoint and midPoint (units of log luminance)
	float coefsHigh[10];   // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	Float2 minPoint; // {luminance, luminance} linear extension below this
	Float2 midPoint; // {luminance, luminance} 
	Float2 maxPoint; // {luminance, luminance} linear extension above this
	Float2 slope;
	Float2 limits; // min and max prior to RRT
};



//
//  Spline function used by RRT
//
//////////////////////////////////////////////////////////////////////////////////////////
static float segmented_spline_c5_fwd(float x)
{
	// RRT_PARAMS
	const SegmentedSplineParams_c5 C =
	{
		// coefsLow[6]
		{ -4.0000000000f, -4.0000000000f, -3.1573765773f, -0.4852499958f, 1.8477324706f, 1.8477324706f },
		// coefsHigh[6]
		{ -0.7185482425f, 2.0810307172f, 3.6681241237f, 4.0000000000f, 4.0000000000f, 4.0000000000f },
		{ 0.18f*exp2(-15.0f), 0.0001f },    // minPoint
		{ 0.18f, 4.8f },    // midPoint
		{ 0.18f*exp2(18.0f), 10000.f },    // maxPoint
		0.0f,  // slopeLow
		0.0f   // slopeHigh
	};

	// Textbook monomial to basis-function conversion matrix.

	static const Float3 M[3] =
	{
		{ 0.5f, -1.0f, 0.5f },
		{-1.0f, 1.0f, 0.5f },
		{ 0.5f, 0.0f, 0.0f }
	};

	const int N_KNOTS_LOW = 4;
	const int N_KNOTS_HIGH = 4;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to ACESMIN.1
	float xCheck = x <= 0 ? exp2(-14.0f) : x;

	float logx = log10(xCheck);
	float logy;

	if (logx <= log10(C.minPoint.X))
	{
		logy = logx * C.slopeLow + (log10(C.minPoint.Y) - C.slopeLow * log10(C.minPoint.X));
	}
	else if ((logx > log10(C.minPoint.X)) && (logx < log10(C.midPoint.X)))
	{
		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(C.minPoint.X)) / (log10(C.midPoint.X) - log10(C.minPoint.X));
		int j = int(knot_coord);
		float t = knot_coord - j;

		Float3 cf = { C.coefsLow[j], C.coefsLow[j + 1], C.coefsLow[j + 2] };

		Float3 monomials = { t * t, t, 1 };
		//logy = dot(monomials, mul(cf, M));
		//logy = Float3::DotProduct(monomials, M.TransformVector(cf));
		Float3 basis = cf.X * M[0] + cf.Y * M[1] + cf.Z * M[2];
		logy = Float3::Dot(monomials, basis);
	}
	else if ((logx >= log10(C.midPoint.X)) && (logx < log10(C.maxPoint.X)))
	{
		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(C.midPoint.X)) / (log10(C.maxPoint.X) - log10(C.midPoint.X));
		int j = int(knot_coord);
		float t = knot_coord - j;

		Float3 cf = { C.coefsHigh[j], C.coefsHigh[j + 1], C.coefsHigh[j + 2] };

		Float3 monomials = { t * t, t, 1 };
		//logy = dot(monomials, mul(cf, M));
		//logy = Float3::DotProduct(monomials, M.TransformVector(cf));
		Float3 basis = cf.X * M[0] + cf.Y * M[1] + cf.Z * M[2];
		logy = Float3::Dot(monomials, basis);
	}
	else
	{ //if ( logIn >= log10(C.maxPoint.x) ) { 
		logy = logx * C.slopeHigh + (log10(C.maxPoint.Y) - C.slopeHigh * log10(C.maxPoint.X));
	}

	return pow(10.0f, logy);
}


inline void mul3(Float3& res, const Float3 &a, const Float3* M)
{
	res = a.X * M[0] + a.Y * M[1] + a.Z * M[2];
}

//
//  AdaptSpline
//
//    Using a reference ODT spline, adjust middle gray and max levels
//
//////////////////////////////////////////////////////////////////////////////////////////
static SegmentedSplineParams_c9_internal AdaptSpline(const SegmentedSplineParams_c9_internal C, float newMin, float newMax, float outMax, float outMidScale)
{
	// Monomial and inverse monomial matrices
	static const Float3 M[3] =
	{
		{ 0.5f, -1.0f, 0.5f },
		{ -1.0f, 1.0f, 0.5f },
		{ 0.5f, 0.0f, 0.0f }
	};

	static const Float3 iM[3] =
	{
		{ 0.0f, 0.0f, 2.0f },
		{ -0.5f, 0.5f, 1.5f },
		{ 1.0f, 1.0f, 1.0f }
	};

	const int N_KNOTS_LOW = 8;
	const int N_KNOTS_HIGH = 8;

	SegmentedSplineParams_c9_internal C2 = C;

	// Set the new max input and output levels
	C2.maxPoint.X = segmented_spline_c5_fwd(newMax);
	C2.maxPoint.Y = outMax;

	C2.limits.Y = newMax;

	// Set new minimum levels
	C2.minPoint.X = segmented_spline_c5_fwd(newMin);

	C2.limits.X = newMin;

	// scale the middle gray output level
	C2.midPoint.Y *= outMidScale;

	// compute and apply scale used to bring bottom segment of the transform to the level desired for middle gray
	float scale = (log10(C.midPoint[1]) - log10(C.minPoint[1])) / (log10(C2.midPoint[1]) - log10(C2.minPoint[1]));

	for (int j = 0; j < N_KNOTS_LOW + 2; j++)
	{
		C2.coefsLow[j] = (C2.coefsLow[j] - log10(C2.minPoint[1])) / scale + log10(C2.minPoint[1]);
	}

	// compute and apply scale to top segment of the transform to the level matching the new max and middle gray
	scale = (log10(C.maxPoint[1]) - log10(C.midPoint[1])) / (log10(C2.maxPoint[1]) - log10(C2.midPoint[1]));

	float target[10]; // saves the "target" values, as we need to match/relax the spline to properly join the low segment

	for (int j = 0; j < N_KNOTS_HIGH + 2; j++)
	{
		C2.coefsHigh[j] = (C2.coefsHigh[j] - log10(C.midPoint[1])) / scale + log10(C2.midPoint[1]);
		target[j] = C2.coefsHigh[j];
	}

	//
	// Adjust the high spline to properly meet the low spline, then relax the high spline
	//

	// Coefficients for the last segment of the low range
	Float3 cfl = { C2.coefsLow[7], C2.coefsLow[8], C2.coefsLow[9] };

	// Coeffiecients for the first segment of the high range
	Float3 cfh = { C2.coefsHigh[0], C2.coefsHigh[1], C2.coefsHigh[2] };

	Float3 cflt, cfht;

	// transform the coefficients by the monomial matrix
	mul3(cflt, cfl, M);
	mul3(cfht, cfh, M);

	// low and high curves cover different ranges, so compute scaling factor needed to match slopes at the join point
	float scaleLow = 1.0f / (log10f(C2.midPoint[0]) - log10f(C2.minPoint[0]));
	float scaleHigh = 1.0f / (log10f(C2.maxPoint[0]) - log10f(C2.midPoint[0]));


	// compute the targeted exit point for the segment 
	float outRef = cfht[0] * 2.0f + cfht[1]; //slope at t == 1

	// match slopes and intersection
	cfht[2] = cflt[2]; 
	cfht[1] = (scaleLow * cflt[1]) / scaleHigh;

	// compute the exit point the segment has after the adjustment
	float out = cfht[0] * 2.0f + cfht[1]; //result at t == 1

	// ease spline toward target and adjust
	float outTarget = (outRef*7.0f + out*1.0f) / 8.0f;
	cfht[0] = (outTarget - cfht[1]) / 2.0f;

	// back-transform  the adjustments and save them
	Float3 acfh;

	mul3(acfh, cfht, iM);

	C2.coefsHigh[0] = acfh[0];
	C2.coefsHigh[1] = acfh[1];
	C2.coefsHigh[2] = acfh[2];

	// now correct the rest of the spline
	for (int j = 1; j < N_KNOTS_HIGH; j++)
	{
		//  Original rescaled spline values for the segment (ideal "target")
		Float3 cfoh = { target[j], target[j + 1], target[j + 2] };

		//  New spline values for the segment based on alterations to prior ranges
		Float3 cfh = { C2.coefsHigh[j], C2.coefsHigh[j + 1], C2.coefsHigh[j + 2] };

		Float3 cfht, cfoht;

		mul3(cfht, cfh, M);
		mul3(cfoht, cfoh, M);

		//Compute exit slope for segments
		float out = cfht[0] * 2.0f + cfht[1]; //slope at t == 1
		float outRef = cfoht[0] * 2.0f + cfoht[1]; //slope at t == 1

		//Ease spline toward targetted slope
		float outTarget = (outRef*(7.0f - j) + out*(1.0f + j)) / 8.0f;
		cfht[0] = (outTarget - cfht[1]) / 2.0f;

		// Back transform and save
		Float3 acfh;

		mul3(acfh, cfht, iM);

		C2.coefsHigh[j] = acfh[0];
		C2.coefsHigh[j + 1] = acfh[1];
		C2.coefsHigh[j + 2] = acfh[2];
	}

	return C2;
}

//
//  GetACESData
//
//    Select a curve used as part of the ACES ODT. Optionally, derive a modified
//  version of the base curve with an altered middle gray and maximum number of
//  stops.
//
//////////////////////////////////////////////////////////////////////////////////////////
SegmentedSplineParams_c9 GetAcesODTData(ODTCurve BaseCurve, float MinStop, float MaxStop, float MaxLevel, float MidGrayScale)
{
	//
	// Standard ACES ODT curves
	//

	// ODT_48nits
	static const SegmentedSplineParams_c9_internal ODT_48nits =
	{
		// coefs[10]
		{ -1.6989700043f, -1.6989700043f, -1.4779000000f, -1.2291000000f, -0.8648000000f, -0.4480000000f, 0.0051800000f, 0.4511080334f, 0.9113744414f, 0.9113744414f },
		// coefsHigh[10]
		{ 0.5154386965f, 0.8470437783f, 1.1358000000f, 1.3802000000f, 1.5197000000f, 1.5985000000f, 1.6467000000f, 1.6746091357f, 1.6878733390f, 1.6878733390f },
		{ segmented_spline_c5_fwd(0.18f*exp2(-6.5f)), 0.02f },    // minPoint
		{ segmented_spline_c5_fwd(0.18f), 4.8f },    // midPoint  
		{ segmented_spline_c5_fwd(0.18f*exp2(6.5f)), 48.0f },    // maxPoint
		{ 0.0f, 0.04f },  // slope
		{ 0.18f*exp2(-6.5f), 0.18f*exp2(6.5f) } // limits
	};


	// ODT_1000nits
	static const SegmentedSplineParams_c9_internal ODT_1000nits =
	{
		// coefsLow[10]
		{ -2.3010299957f, -2.3010299957f, -1.9312000000f, -1.5205000000f, -1.0578000000f, -0.4668000000f, 0.1193800000f, 0.7088134201f, 1.2911865799f, 1.2911865799f },
		// coefsHigh[10]
		{ 0.8089132070f, 1.1910867930f, 1.5683000000f, 1.9483000000f, 2.3083000000f, 2.6384000000f, 2.8595000000f, 2.9872608805f, 3.0127391195f, 3.0127391195f },
		{ segmented_spline_c5_fwd(0.18f*pow(2.f, -12.f)), 0.005f },    // minPoint
		{ segmented_spline_c5_fwd(0.18f), 10.0f },    // midPoint  
		{ segmented_spline_c5_fwd(0.18f*pow(2.f, 10.f)), 1000.0f },    // maxPoint
		{ 0.0f, 0.06f },  // slope
		{ 0.18f*exp2(-12.f), 0.18f*exp2(10.f) } // limits
	};

	// ODT_2000nits
	static const SegmentedSplineParams_c9_internal ODT_2000nits =
	{
		// coefsLow[10]
		{ -2.3010299957f, -2.3010299957f, -1.9312000000f, -1.5205000000f, -1.0578000000f, -0.4668000000f, 0.1193800000f, 0.7088134201f, 1.2911865799f, 1.2911865799f },
		// coefsHigh[10]
		{ 0.8019952042f, 1.1980047958f, 1.5943000000f, 1.9973000000f, 2.3783000000f, 2.7684000000f, 3.0515000000f, 3.2746293562f, 3.3274306351f, 3.3274306351f },
		{ segmented_spline_c5_fwd(0.18f*pow(2.f, -12.f)), 0.005f },    // minPoint
		{ segmented_spline_c5_fwd(0.18f), 10.0f },    // midPoint  
		{ segmented_spline_c5_fwd(0.18f*pow(2.f, 11.f)), 2000.0f },    // maxPoint
		{ 0.0f, 0.12f },  // slope
		{ 0.18f*exp2(-12.f), 0.18f*exp2(11.f) } // limits
	};

	// ODT_4000nits
	static const SegmentedSplineParams_c9_internal ODT_4000nits =
	{
		// coefsLow[10]
		{ -2.3010299957f, -2.3010299957f, -1.9312000000f, -1.5205000000f, -1.0578000000f, -0.4668000000f, 0.1193800000f, 0.7088134201f, 1.2911865799f, 1.2911865799f },
		// coefsHigh[10]
		{ 0.7973186613f, 1.2026813387f, 1.6093000000f, 2.0108000000f, 2.4148000000f, 2.8179000000f, 3.1725000000f, 3.5344995451f, 3.6696204376f, 3.6696204376f },
		{ segmented_spline_c5_fwd(0.18f*pow(2.f, -12.f)), 0.005f },    // minPoint
		{ segmented_spline_c5_fwd(0.18f), 10.0 },    // midPoint  
		{ segmented_spline_c5_fwd(0.18f*pow(2.f, 12.f)), 4000.0f },    // maxPoint
		{ 0.0f, 0.3f },   // slope
		{ 0.18f*exp2(-12.f), 0.18f*exp2(12.f) } // limits
	};


	// convert, defaulting to 48 nits
	const SegmentedSplineParams_c9_internal *Src = &ODT_48nits;
	SegmentedSplineParams_c9_internal Generated;
	SegmentedSplineParams_c9 C;


	switch (BaseCurve)
	{
	case ODT_LDR_Ref:     Src = &ODT_48nits; break;
	case ODT_1000Nit_Ref: Src = &ODT_1000nits; break;
	case ODT_2000Nit_Ref: Src = &ODT_2000nits; break;
	case ODT_4000Nit_Ref: Src = &ODT_4000nits; break;

	// Adjustable curves
	case ODT_LDR_Adj:
		Src = &Generated;
		MaxLevel = MaxLevel > 0.0f ? MaxLevel : 48.0f;
		MaxStop = MaxStop > 0 ? MaxStop : 6.5f;
		MinStop = MinStop < 0 ? MinStop : -6.5f;
		Generated = AdaptSpline(ODT_48nits, 0.18f*pow(2.f, MinStop), 0.18f*pow(2.f, MaxStop), MaxLevel, MidGrayScale);
		break;

	case ODT_1000Nit_Adj:
		Src = &Generated;
		MaxLevel = MaxLevel > 0.0f ? MaxLevel : 1000.0f;
		MaxStop = MaxStop > 0 ? MaxStop : 10.0f;
		MinStop = MinStop < 0 ? MinStop : -12.0f;
		Generated = AdaptSpline(ODT_1000nits, 0.18f*pow(2.f, MinStop), 0.18f*pow(2.f, MaxStop), MaxLevel, MidGrayScale);
		break;

	case ODT_2000Nit_Adj:
		Src = &Generated;
		MaxLevel = MaxLevel > 0.0f ? MaxLevel : 2000.0f;
		MaxStop = MaxStop > 0 ? MaxStop : 11.0f;
		MinStop = MinStop < 0 ? MinStop : -12.0f;
		Generated = AdaptSpline(ODT_2000nits, 0.18f*pow(2.f, MinStop), 0.18f*pow(2.f, MaxStop), MaxLevel, MidGrayScale);
		break;

	case ODT_4000Nit_Adj:
		Src = &Generated;
		MaxLevel = MaxLevel > 0.0f ? MaxLevel : 4000.0f;
		MaxStop = MaxStop > 0 ? MaxStop : 12.0f;
		MinStop = MinStop < 0 ? MinStop : -12.0f;
		Generated = AdaptSpline(ODT_4000nits, 0.18f*pow(2.f, MinStop), 0.18f*pow(2.f, MaxStop), MaxLevel, MidGrayScale);
		break;

	};

	{
		const SegmentedSplineParams_c9_internal &Curve = *Src;

		for (int Index = 0; Index < 10; Index++)
		{
			C.coefs[Index] = Float4(Curve.coefsLow[Index], Curve.coefsHigh[Index], 0.0f, 0.0f);
		}
		C.minPoint = Curve.minPoint;
		C.midPoint = Curve.midPoint;
		C.maxPoint = Curve.maxPoint;
		C.slope = Curve.slope;
		C.limits = Curve.limits;
	}


	return C;
}

/*****************************************************************************************************************/

Float3 max(const Float3& a, const Float3& b)
{
	Float3 c;
	c.X = max(a.X, b.X);
	c.Y = max(a.Y, b.Y);
	c.Z = max(a.Z, b.Z);

	return c;
}

Float3 max(const Float3& a, const float b)
{
	Float3 c;
	c.X = max(a.X, b);
	c.Y = max(a.Y, b);
	c.Z = max(a.Z, b);

	return c;
}

Float3 min(const Float3& a, const Float3& b)
{
	Float3 c;
	c.X = min(a.X, b.X);
	c.Y = min(a.Y, b.Y);
	c.Z = min(a.Z, b.Z);

	return c;
}

Float3 min(const Float3& a, const float b)
{
	Float3 c;
	c.X = min(a.X, b);
	c.Y = min(a.Y, b);
	c.Z = min(a.Z, b);

	return c;
}

Float3 clamp(const Float3& a, const float b, const float c)
{
	Float3 d;
	d.X = max( b, min(a.X, c));
	d.Y = max( b, min(a.Y, c));
	d.Z = max( b, min(a.Z, c));

	return d;
}

static const float TINY = 1e-10f;
static const float M_PI = 3.1415927f;
static const float HALF_MAX = 65504.0f;

static const Float3x3 AP0_2_XYZ_MAT =
{
	0.95255238f, 0.00000000f, 0.00009368f,
	0.34396642f, 0.72816616f, -0.07213254f,
	-0.00000004f, 0.00000000f, 1.00882506f
};

static const Float3x3 XYZ_2_AP0_MAT =
{
	1.04981101f, -0.00000000f, -0.00009748f,
	-0.49590296f, 1.37331295f, 0.09824003f,
	0.00000004f, -0.00000000f, 0.99125212f
};

static const Float3x3 AP1_2_XYZ_MAT =
{
	0.66245413f, 0.13400421f, 0.15618768f,
	0.27222872f, 0.67408168f, 0.05368952f,
	-0.00557466f, 0.00406073f, 1.01033902f
};

static const Float3x3 XYZ_2_AP1_MAT =
{
	1.64102352f, -0.32480335f, -0.23642471f,
	-0.66366309f, 1.61533189f, 0.01675635f,
	0.01172191f, -0.00828444f, 0.98839492f
};

static const Float3x3 AP0_2_AP1_MAT =
{
	1.45143950f, -0.23651081f, -0.21492855f,
	-0.07655388f, 1.17623007f, -0.09967594f,
	0.00831613f, -0.00603245f, 0.99771625f
};

static const Float3x3 AP1_2_AP0_MAT =
{
	0.69545215f, 0.14067869f, 0.16386905f,
	0.04479461f, 0.85967094f, 0.09553432f,
	-0.00552587f, 0.00402521f, 1.00150073f
};

// EHart - need to check this, might be a transpose issue with CTL
static const Float3 AP1_RGB2Y = { 0.27222872f, 0.67408168f, 0.05368952f };


float max_f3(const Float3& In)
{
	return max(In.X, max(In.Y, In.Z));
}

float min_f3(const Float3& In)
{
	return min(In.X, min(In.Y, In.Z));
}

float rgb_2_saturation(const Float3& rgb)
{
	return (max(max_f3(rgb), TINY) - max(min_f3(rgb), TINY)) / max(max_f3(rgb), 1e-2f);
}

/* ---- Conversion Functions ---- */
// Various transformations between color encodings and data representations
//

// Transformations between CIE XYZ tristimulus values and CIE x,y 
// chromaticity coordinates
Float3 XYZ_2_xyY(const Float3& XYZ)
{
	Float3 xyY;
	float divisor = (XYZ[0] + XYZ[1] + XYZ[2]);
	if (divisor == 0.) divisor = 1e-10f;
	xyY[0] = XYZ[0] / divisor;
	xyY[1] = XYZ[1] / divisor;
	xyY[2] = XYZ[1];

	return xyY;
}

Float3 xyY_2_XYZ(const Float3& xyY)
{
	Float3 XYZ;
	XYZ[0] = xyY[0] * xyY[2] / max(xyY[1], 1e-10f);
	XYZ[1] = xyY[2];
	XYZ[2] = (1.0f - xyY[0] - xyY[1]) * xyY[2] / max(xyY[1], 1e-10f);

	return XYZ;
}


// Transformations from RGB to other color representations
float rgb_2_hue(const Float3& rgb)
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
		hue = (180.f / M_PI) * atan2(sqrt(3.f)*(rgb[1] - rgb[2]), 2.f * rgb[0] - rgb[1] - rgb[2]);
	}

	if (hue < 0.f) hue = hue + 360.f;

	return hue;
}

float rgb_2_yc(const Float3& rgb, float ycRadiusWeight = 1.75)
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

	return (b + g + r + ycRadiusWeight * chroma) / 3.f;
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
static const float DIM_SURROUND_GAMMA = 0.9811f;

Float3 darkSurround_to_dimSurround(const Float3& linearCV)
{
	Float3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

	Float3 xyY = XYZ_2_xyY(XYZ);
	xyY[2] = max(xyY[2], 0.f);
	xyY[2] = pow(xyY[2], DIM_SURROUND_GAMMA);
	XYZ = xyY_2_XYZ(xyY);

	return mul(XYZ_2_AP1_MAT, XYZ);
}

Float3 dimSurround_to_darkSurround(const Float3& linearCV)
{
	Float3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

	Float3 xyY = XYZ_2_xyY(XYZ);
	xyY[2] = max(xyY[2], 0.f);
	xyY[2] = pow(xyY[2], 1.f / DIM_SURROUND_GAMMA);
	XYZ = xyY_2_XYZ(xyY);

	return mul(XYZ_2_AP1_MAT, XYZ);
}

Float3 alter_surround(const Float3& linearCV, float gamma)
{
	Float3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

	Float3 xyY = XYZ_2_xyY(XYZ);
	xyY[2] = max(xyY[2], 0.0f);
	xyY[2] = pow(xyY[2], gamma);
	XYZ = xyY_2_XYZ(xyY);

	return mul(XYZ_2_AP1_MAT, XYZ);
}



Float3x3 calc_sat_adjust_matrix(float sat, const Float3& rgb2Y)
{
	//
	// This function determines the terms for a 3x3 saturation matrix that is
	// based on the luminance of the input.
	//
	Float3x3 M;

	M.m[0] = (1.0f - sat) * rgb2Y[0] + sat;
	M.m[3] = (1.0f - sat) * rgb2Y[0];
	M.m[6] = (1.0f - sat) * rgb2Y[0];

	M.m[1] = (1.0f - sat) * rgb2Y[1];
	M.m[4] = (1.0f - sat) * rgb2Y[1] + sat;
	M.m[7] = (1.0f - sat) * rgb2Y[1];
	 
	M.m[2] = (1.0f - sat) * rgb2Y[2];
	M.m[5] = (1.0f - sat) * rgb2Y[2];
	M.m[8] = (1.0f - sat) * rgb2Y[2] + sat;

	// EHart - removed transpose, as the indexing in CTL is transposed

	return M;
}

/* ---- Signal encode/decode functions ---- */

float moncurve_f(float x, float gamma, float offs)
{
	// Forward monitor curve
	float y;
	const float fs = ((gamma - 1.0f) / offs) * pow(offs * gamma / ((gamma - 1.0f) * (1.0f + offs)), gamma);
	const float xb = offs / (gamma - 1.0f);
	if (x >= xb)
		y = pow((x + offs) / (1.0f + offs), gamma);
	else
		y = x * fs;
	return y;
}

float moncurve_r(float y, float gamma, float offs)
{
	// Reverse monitor curve
	float x;
	const float yb = pow(offs * gamma / ((gamma - 1.0f) * (1.0f + offs)), gamma);
	const float rs = pow((gamma - 1.0f) / offs, gamma - 1.0f) * pow((1.0f + offs) / gamma, gamma);
	if (y >= yb)
		x = (1.0f + offs) * pow(y, 1.0f / gamma) - offs;
	else
		x = y * rs;
	return x;
}

// Base functions from SMPTE ST 2084-2014

// Constants from SMPTE ST 2084-2014
static const float pq_m1 = 0.1593017578125f; // ( 2610.0 / 4096.0 ) / 4.0;
static const float pq_m2 = 78.84375f; // ( 2523.0 / 4096.0 ) * 128.0;
static const float pq_c1 = 0.8359375f; // 3424.0 / 4096.0 or pq_c3 - pq_c2 + 1.0;
static const float pq_c2 = 18.8515625f; // ( 2413.0 / 4096.0 ) * 32.0;
static const float pq_c3 = 18.6875f; // ( 2392.0 / 4096.0 ) * 32.0;

static const float pq_C = 10000.0f;

// Converts from the non-linear perceptually quantized space to linear cd/m^2
// Note that this is in float, and assumes normalization from 0 - 1
// (0 - pq_C for linear) and does not handle the integer coding in the Annex 
// sections of SMPTE ST 2084-2014
float pq_f(float N)
{
	// Note that this does NOT handle any of the signal range
	// considerations from 2084 - this assumes full range (0 - 1)
	float Np = pow(N, 1.0f / pq_m2);
	float L = Np - pq_c1;
	if (L < 0.0f)
		L = 0.0f;
	L = L / (pq_c2 - pq_c3 * Np);
	L = pow(L, 1.0f / pq_m1);
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
	float N = (pq_c1 + pq_c2 * Lm) / (1.0f + pq_c3 * Lm);
	N = pow(N, pq_m2);
	return N;
}

Float3 pq_r_f3(const Float3& In)
{
	// converts from linear cd/m^2 to PQ code values

	Float3 Out;
	Out[0] = pq_r(In[0]);
	Out[1] = pq_r(In[1]);
	Out[2] = pq_r(In[2]);

	return Out;
}

Float3 pq_f_f3( const Float3& In)
{
	// converts from PQ code values to linear cd/m^2

	Float3 Out;
	Out[0] = pq_f(In[0]);
	Out[1] = pq_f(In[1]);
	Out[2] = pq_f(In[2]);

	return Out;
}



float glow_fwd(float ycIn, float glowGainIn, float glowMid)
{
	float glowGainOut;

	if (ycIn <= 2.f / 3.f * glowMid) {
		glowGainOut = glowGainIn;
	}
	else if (ycIn >= 2.f * glowMid) {
		glowGainOut = 0.f;
	}
	else {
		glowGainOut = glowGainIn * (glowMid / ycIn - 1.f / 2.f);
	}

	return glowGainOut;
}

float cubic_basis_shaper(float x, float w   /* full base width of the shaper function (in degrees)*/)
{
	const float M[4][4] =
	{
		{ -1.f / 6, 3.f / 6, -3.f / 6, 1.f / 6 },
		{ 3.f / 6, -6.f / 6, 3.f / 6, 0.f / 6 },
		{ -3.f / 6, 0.f / 6, 3.f / 6, 0.f / 6 },
		{ 1.f / 6, 4.f / 6, 1.f / 6, 0.f / 6 }
	};

	float knots[5] =
	{
		-w / 2.f,
		-w / 4.f,
		0.f,
		w / 4.f,
		w / 2.f
	};

	// EHart - init y, because CTL does by default
	float y = 0;
	if ((x > knots[0]) && (x < knots[4])) {
		float knot_coord = (x - knots[0]) * 4.f / w;
		int j = int(knot_coord);
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
			y = 0.0f;
		}
	}

	return y * 3 / 2.f;
}

float sign(float x)
{
	if (x < 0.0f)
		return -1.0f;
	if (x > 0.0f)
		return 1.0f;
	return 0.0f;
}


float sigmoid_shaper(float x)
{
	// Sigmoid function in the range 0 to 1 spanning -2 to +2.

	float t = max(1.f - abs(x / 2.f), 0.f);
	float y = 1.f + sign(x) * (1.f - t * t);

	return y / 2.f;
}

float center_hue(float hue, float centerH)
{
	float hueCentered = hue - centerH;
	if (hueCentered < -180.f) hueCentered = hueCentered + 360.f;
	else if (hueCentered > 180.f) hueCentered = hueCentered - 360.f;
	return hueCentered;
}

Float3 rrt( const Float3 &rgbIn)
{

	// "Glow" module constants
	const float RRT_GLOW_GAIN = 0.05f;
	const float RRT_GLOW_MID = 0.08f;
	// --- Glow module --- //
	float saturation = rgb_2_saturation(rgbIn);
	float ycIn = rgb_2_yc(rgbIn);
	float s = sigmoid_shaper((saturation - 0.4f) / 0.2f);
	float addedGlow = 1.f + glow_fwd(ycIn, RRT_GLOW_GAIN * s, RRT_GLOW_MID);

	Float3 aces = addedGlow * rgbIn;


	// Red modifier constants
	const float RRT_RED_SCALE = 0.82f;
	const float RRT_RED_PIVOT = 0.03f;
	const float RRT_RED_HUE = 0.f;
	const float RRT_RED_WIDTH = 135.f;
	// --- Red modifier --- //
	float hue = rgb_2_hue(aces);
	float centeredHue = center_hue(hue, RRT_RED_HUE);
	float hueWeight = cubic_basis_shaper(centeredHue, RRT_RED_WIDTH);

	aces[0] = aces[0] + hueWeight * saturation *(RRT_RED_PIVOT - aces[0]) * (1.f - RRT_RED_SCALE);


	// --- ACES to RGB rendering space --- //
	aces = max(aces, 0.0f);  // avoids saturated negative colors from becoming positive in the matrix



	Float3 rgbPre = mul(AP0_2_AP1_MAT, aces);

	rgbPre = clamp(rgbPre, 0.f, HALF_MAX);

	// Desaturation contants
	const float RRT_SAT_FACTOR = 0.96f;
	const Float3x3 RRT_SAT_MAT = calc_sat_adjust_matrix(RRT_SAT_FACTOR, AP1_RGB2Y);
	// --- Global desaturation --- //
	rgbPre = mul(RRT_SAT_MAT, rgbPre);


	// --- Apply the tonescale independently in rendering-space RGB --- //
	Float3 rgbPost;
	rgbPost[0] = segmented_spline_c5_fwd(rgbPre[0]);
	rgbPost[1] = segmented_spline_c5_fwd(rgbPre[1]);
	rgbPost[2] = segmented_spline_c5_fwd(rgbPre[2]);

	// --- RGB rendering space to OCES --- //
	Float3 rgbOces = mul(AP1_2_AP0_MAT, rgbPost);

	return rgbOces;
}

float pow10(float x)
{
	return pow(10.0f, x);
}

float segmented_spline_c9_fwd(float x, SegmentedSplineParams_c9 C)
{
	static const Float3 M[3] =
	{
		{ 0.5f, -1.0f, 0.5f },
		{ -1.0f, 1.0f, 0.5f },
		{ 0.5f, 0.0f, 0.0f }
	};

	const int N_KNOTS_LOW = 8;
	const int N_KNOTS_HIGH = 8;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to OCESMIN.
	float xCheck = x;
	if (xCheck <= 0.0) xCheck = 1e-4f;

	float logx = log10(xCheck);

	float logy;

	if (logx <= log10(C.minPoint.X)) {

		logy = logx * C.slope.X + (log10(C.minPoint.Y) - C.slope.X * log10(C.minPoint.X));

	}
	else if ((logx > log10(C.minPoint.X)) && (logx < log10(C.midPoint.X))) {

		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(C.minPoint.X)) / (log10(C.midPoint.X) - log10(C.minPoint.X));
		int j = int(knot_coord);
		float t = knot_coord - j;

		Float3 cf = { C.coefs[j].X, C.coefs[j + 1].X, C.coefs[j + 2].X };

		Float3 monomials = { t * t, t, 1. };
		
		Float3 basis = cf.X * M[0] + cf.Y * M[1] + cf.Z * M[2];
		logy = Float3::Dot(monomials, basis);

	}
	else if ((logx >= log10(C.midPoint.X)) && (logx < log10(C.maxPoint.X))) {

		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(C.midPoint.X)) / (log10(C.maxPoint.X) - log10(C.midPoint.X));
		int j = int(knot_coord);
		float t = knot_coord - j;

		Float3 cf = { C.coefs[j].Y, C.coefs[j + 1].Y, C.coefs[j + 2].Y };
 
		Float3 monomials = { t * t, t, 1. };

		Float3 basis = cf.X * M[0] + cf.Y * M[1] + cf.Z * M[2];
		logy = Float3::Dot(monomials, basis);

	}
	else { //if ( logIn >= log10(C.maxPoint.X) ) { 

		logy = logx * C.slope.Y + (log10(C.maxPoint.Y) - C.slope.Y * log10(C.maxPoint.X));

	}

	return pow10(logy);
}



static const Float3x3 D65_2_D60_CAT =
{
	1.01303f, 0.00610531f, -0.014971f,
	0.00769823f, 0.998165f, -0.00503203f,
	-0.00284131f, 0.00468516f, 0.924507f,
};
static const Float3x3 sRGB_2_XYZ_MAT =
{
	0.41239089f, 0.35758430f, 0.18048084f,
	0.21263906f, 0.71516860f, 0.07219233f,
	0.01933082f, 0.11919472f, 0.95053232f
};


static const float DISPGAMMA = 2.4f;
static const float OFFSET = 0.055f;

#pragma warning(disable : 4702)

//
//  EvalACES
//
/////////////////////////////////////////////////////////////////////////////////////////
Float3 EvalACES(Float3 InColor, const ACESparams& Params)
{
	Float3 aces = mul(XYZ_2_AP0_MAT, mul(D65_2_D60_CAT, mul(sRGB_2_XYZ_MAT, InColor)));

	Float3 oces = rrt(aces);

	// OCES to RGB rendering space
	Float3 rgbPre = mul(AP0_2_AP1_MAT, oces);

	
	Float3 rgbPost;

	if (Params.tonemapLuminance)
	{
		// luminance only path, for content that has been mastered for an expectation of an oversaturated tonemap operator
		float y = Float3::Dot(rgbPre, AP1_RGB2Y);
		float scale = segmented_spline_c9_fwd(y, Params.C) / y;

		// compute the more desaturated per-channel version
		rgbPost[0] = segmented_spline_c9_fwd(rgbPre[0], Params.C);
		rgbPost[1] = segmented_spline_c9_fwd(rgbPre[1], Params.C);
		rgbPost[2] = segmented_spline_c9_fwd(rgbPre[2], Params.C);

		// lerp between values
		rgbPost = max(Float3::Lerp(rgbPost, rgbPre * scale, Params.saturationLevel), Params.CinemaLimits.X); // clamp to min to prevent the genration of negative values
	}
	else
	{
		// Apply the tonescale independently in rendering-space RGB
		rgbPost[0] = segmented_spline_c9_fwd(rgbPre[0], Params.C);
		rgbPost[1] = segmented_spline_c9_fwd(rgbPre[1], Params.C);
		rgbPost[2] = segmented_spline_c9_fwd(rgbPre[2], Params.C);
	}

	// Scale luminance to linear code value
	Float3 linearCV;
	linearCV[0] = Y_2_linCV(rgbPost[0], Params.CinemaLimits.Y, Params.CinemaLimits.X);
	linearCV[1] = Y_2_linCV(rgbPost[1], Params.CinemaLimits.Y, Params.CinemaLimits.X);
	linearCV[2] = Y_2_linCV(rgbPost[2], Params.CinemaLimits.Y, Params.CinemaLimits.X);

	if (Params.surroundAdjust)
	{
		// Apply gamma adjustment to compensate for surround
		linearCV = alter_surround(linearCV, Params.surroundGamma);
	}

	if (Params.desaturate)
	{
		// Apply desaturation to compensate for luminance difference
		// Saturation compensation factor
		const float ODT_SAT_FACTOR = 0.93f;
		const Float3x3 ODT_SAT_MAT = calc_sat_adjust_matrix(ODT_SAT_FACTOR, AP1_RGB2Y);
		linearCV = mul(ODT_SAT_MAT, linearCV);
	}

	// Convert to display primary encoding
	// Rendering space RGB to XYZ
	Float3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

	if (Params.applyCAT)
	{
		// Apply CAT from ACES white point to assumed observer adapted white point
		// EHart - should recompute this matrix
		const Float3x3 D60_2_D65_CAT =
		{
			0.987224f, -0.00611327f, 0.0159533f,
			-0.00759836f, 1.00186f, 0.00533002f,
			0.00307257f, -0.00509595f, 1.08168f,
		};
		XYZ = mul(D60_2_D65_CAT, XYZ);
	}

	// CIE XYZ to display primaries
	linearCV = mul(Params.XYZ_2_DISPLAY_PRI_MAT, XYZ);

	// Encode linear code values with transfer function
	Float3 outputCV = linearCV;

	if (Params.OutputMode == 0)
	{
		// LDR mode, clamp 0/1 and encode 
		linearCV = clamp(linearCV, 0., 1.);

		outputCV[0] = moncurve_r(linearCV[0], DISPGAMMA, OFFSET);
		outputCV[1] = moncurve_r(linearCV[1], DISPGAMMA, OFFSET);
		outputCV[2] = moncurve_r(linearCV[2], DISPGAMMA, OFFSET);
	}
	else if (Params.OutputMode == 1)
	{
		//scale to bring the ACES data back to the proper range
		linearCV[0] = linCV_2_Y(linearCV[0], Params.CinemaLimits.Y, Params.CinemaLimits.X);
		linearCV[1] = linCV_2_Y(linearCV[1], Params.CinemaLimits.Y, Params.CinemaLimits.X);
		linearCV[2] = linCV_2_Y(linearCV[2], Params.CinemaLimits.Y, Params.CinemaLimits.X);

		// Handle out-of-gamut values
		// Clip values < 0 (i.e. projecting outside the display primaries)
		//rgb = clamp(rgb, 0., HALF_POS_INF);
		linearCV = max(linearCV, 0.);

		// Encode with PQ transfer function
		outputCV = pq_r_f3(linearCV);
	}
	else if (Params.OutputMode == 2)
	{
		// scRGB

		//scale to bring the ACES data back to the proper range
		linearCV[0] = linCV_2_Y(linearCV[0], Params.CinemaLimits.Y, Params.CinemaLimits.X);
		linearCV[1] = linCV_2_Y(linearCV[1], Params.CinemaLimits.Y, Params.CinemaLimits.X);
		linearCV[2] = linCV_2_Y(linearCV[2], Params.CinemaLimits.Y, Params.CinemaLimits.X);

		// Handle out-of-gamut values
		// Clip values < 0 (i.e. projecting outside the display primaries)
		//rgb = clamp(rgb, 0., HALF_POS_INF);
		linearCV = max(linearCV, 0.0f);


		const Float3x3 XYZ_2_sRGB_MAT =
		{
			3.24096942f, -1.53738296f, -0.49861076f,
			-0.96924388f, 1.87596786f, 0.04155510f,
			0.05563002f, -0.20397684f, 1.05697131f,
		};

		// convert from eported display primaries to sRGB primaries
		linearCV = mul( Params.DISPLAY_PRI_MAT_2_XYZ, linearCV);
		linearCV = mul(XYZ_2_sRGB_MAT, linearCV);

		// map 1.0 to 80 nits (or max nit level if it is lower)
		outputCV = linearCV * (1.0f / min(80.0f, Params.CinemaLimits.Y));
	}

	return outputCV;
}


