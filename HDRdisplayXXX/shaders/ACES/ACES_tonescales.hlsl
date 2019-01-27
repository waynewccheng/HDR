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


float pow10(float f)
{
	return pow(10.0f, f);
}

// Textbook monomial to basis-function conversion matrix.
static const float3x3 M = {
		{ 0.5, -1.0, 0.5 },
		{ -1.0, 1.0, 0.5 },
		{ 0.5, 0.0, 0.0 }
};



struct SplineMapPoint
{
	float x;
	float y;
};

struct SegmentedSplineParams_c5
{
	float coefsLow[6];    // coefs for B-spline between minPoint and midPoint (units of log luminance)
	float coefsHigh[6];   // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	SplineMapPoint minPoint; // {luminance, luminance} linear extension below this
	SplineMapPoint midPoint; // {luminance, luminance} 
	SplineMapPoint maxPoint; // {luminance, luminance} linear extension above this
	float slopeLow;       // log-log slope of low linear extension
	float slopeHigh;      // log-log slope of high linear extension
};

struct SegmentedSplineParams_c9
{
	float coefsLow[10];    // coefs for B-spline between minPoint and midPoint (units of log luminance)
	float coefsHigh[10];   // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	SplineMapPoint minPoint; // {luminance, luminance} linear extension below this
	SplineMapPoint midPoint; // {luminance, luminance} 
	SplineMapPoint maxPoint; // {luminance, luminance} linear extension above this
	float slopeLow;       // log-log slope of low linear extension
	float slopeHigh;      // log-log slope of high linear extension
};


static const SegmentedSplineParams_c5 RRT_PARAMS =
{
	// coefsLow[6]
	{ -4.0000000000, -4.0000000000, -3.1573765773, -0.4852499958, 1.8477324706, 1.8477324706 },
	// coefsHigh[6]
	{ -0.7185482425, 2.0810307172, 3.6681241237, 4.0000000000, 4.0000000000, 4.0000000000 },
	{ 0.18*pow(2., -15), 0.0001 },    // minPoint
	{ 0.18, 4.8 },    // midPoint  
	{ 0.18*pow(2., 18), 10000. },    // maxPoint
	0.0,  // slopeLow
	0.0   // slopeHigh
};


float segmented_spline_c5_fwd ( float x,  SegmentedSplineParams_c5 C = RRT_PARAMS)
{
	const int N_KNOTS_LOW = 4;
	const int N_KNOTS_HIGH = 4;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to ACESMIN.
	float xCheck = x;
	if (xCheck <= 0.0) xCheck = pow(2., -14.);

	float logx = log10(xCheck);

	float logy;

	if (logx <= log10(C.minPoint.x)) {

		logy = logx * C.slopeLow + (log10(C.minPoint.y) - C.slopeLow * log10(C.minPoint.x));

	}
	else if ((logx > log10(C.minPoint.x)) && (logx < log10(C.midPoint.x))) {

		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(C.minPoint.x)) / (log10(C.midPoint.x) - log10(C.minPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = { C.coefsLow[j], C.coefsLow[j + 1], C.coefsLow[j + 2] };


		float3 monomials = { t * t, t, 1. };
		logy = dot(monomials, mul(cf, M));

	}
	else if ((logx >= log10(C.midPoint.x)) && (logx < log10(C.maxPoint.x))) {

		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(C.midPoint.x)) / (log10(C.maxPoint.x) - log10(C.midPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = { C.coefsHigh[j], C.coefsHigh[j + 1], C.coefsHigh[j + 2] };


		float3 monomials = { t * t, t, 1. };
		logy = dot(monomials, mul(cf, M));

	}
	else { //if ( logIn >= log10(C.maxPoint.x) ) { 

		logy = logx * C.slopeHigh + (log10(C.maxPoint.y) - C.slopeHigh * log10(C.maxPoint.x));

	}

	return pow10(logy);

}



static const SegmentedSplineParams_c9 ODT_48nits =
{
	// coefsLow[10]
	{ -1.6989700043, -1.6989700043, -1.4779000000, -1.2291000000, -0.8648000000, -0.4480000000, 0.0051800000, 0.4511080334, 0.9113744414, 0.9113744414 },
	// coefsHigh[10]
	{ 0.5154386965, 0.8470437783, 1.1358000000, 1.3802000000, 1.5197000000, 1.5985000000, 1.6467000000, 1.6746091357, 1.6878733390, 1.6878733390 },
	{ segmented_spline_c5_fwd(0.18*pow(2., -6.5)), 0.02 },    // minPoint
	{ segmented_spline_c5_fwd(0.18), 4.8 },    // midPoint  
	{ segmented_spline_c5_fwd(0.18*pow(2., 6.5)), 48.0 },    // maxPoint
	0.0,  // slopeLow
	0.04  // slopeHigh
};

static const SegmentedSplineParams_c9 ODT_1000nits =
{
	// coefsLow[10]
	{ -2.3010299957, -2.3010299957, -1.9312000000, -1.5205000000, -1.0578000000, -0.4668000000, 0.1193800000, 0.7088134201, 1.2911865799, 1.2911865799 },
	// coefsHigh[10]
	{ 0.8089132070, 1.1910867930, 1.5683000000, 1.9483000000, 2.3083000000, 2.6384000000, 2.8595000000, 2.9872608805, 3.0127391195, 3.0127391195 },
	{ segmented_spline_c5_fwd(0.18*pow(2., -12.)), 0.005 },    // minPoint
	{ segmented_spline_c5_fwd(0.18), 10.0 },    // midPoint  
	{ segmented_spline_c5_fwd(0.18*pow(2., 10.)), 1000.0 },    // maxPoint
	0.0,  // slopeLow
	0.06  // slopeHigh
};

static const SegmentedSplineParams_c9 ODT_2000nits =
{
	// coefsLow[10]
	{ -2.3010299957, -2.3010299957, -1.9312000000, -1.5205000000, -1.0578000000, -0.4668000000, 0.1193800000, 0.7088134201, 1.2911865799, 1.2911865799 },
	// coefsHigh[10]
	{ 0.8019952042, 1.1980047958, 1.5943000000, 1.9973000000, 2.3783000000, 2.7684000000, 3.0515000000, 3.2746293562, 3.3274306351, 3.3274306351 },
	{ segmented_spline_c5_fwd(0.18*pow(2., -12.)), 0.005 },    // minPoint
	{ segmented_spline_c5_fwd(0.18), 10.0 },    // midPoint  
	{ segmented_spline_c5_fwd(0.18*pow(2., 11.)), 2000.0 },    // maxPoint
	0.0,  // slopeLow
	0.12  // slopeHigh
};

static const SegmentedSplineParams_c9 ODT_4000nits =
{
	// coefsLow[10]
	{ -2.3010299957, -2.3010299957, -1.9312000000, -1.5205000000, -1.0578000000, -0.4668000000, 0.1193800000, 0.7088134201, 1.2911865799, 1.2911865799 },
	// coefsHigh[10]
	{ 0.7973186613, 1.2026813387, 1.6093000000, 2.0108000000, 2.4148000000, 2.8179000000, 3.1725000000, 3.5344995451, 3.6696204376, 3.6696204376 },
	{ segmented_spline_c5_fwd(0.18*pow(2., -12.)), 0.005 },    // minPoint
	{ segmented_spline_c5_fwd(0.18), 10.0 },    // midPoint  
	{ segmented_spline_c5_fwd(0.18*pow(2., 12.)), 4000.0 },    // maxPoint
	0.0,  // slopeLow
	0.3   // slopeHigh
};






float segmented_spline_c9_fwd( float x, SegmentedSplineParams_c9 C)
{
	const int N_KNOTS_LOW = 8;
	const int N_KNOTS_HIGH = 8;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to OCESMIN.
	float xCheck = x;
	if (xCheck <= 0.0) xCheck = 1e-4;

	float logx = log10(xCheck);

	float logy;

	if (logx <= log10(C.minPoint.x)) {

		logy = logx * C.slopeLow + (log10(C.minPoint.y) - C.slopeLow * log10(C.minPoint.x));

	}
	else if ((logx > log10(C.minPoint.x)) && (logx < log10(C.midPoint.x))) {

		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(C.minPoint.x)) / (log10(C.midPoint.x) - log10(C.minPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = { C.coefsLow[j], C.coefsLow[j + 1], C.coefsLow[j + 2] };
		
		float3 monomials = { t * t, t, 1. };
		logy = dot(monomials, mul(cf, M));

	}
	else if ((logx >= log10(C.midPoint.x)) && (logx < log10(C.maxPoint.x))) {

		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(C.midPoint.x)) / (log10(C.maxPoint.x) - log10(C.midPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = { C.coefsHigh[j], C.coefsHigh[j + 1], C.coefsHigh[j + 2] };

		float3 monomials = { t * t, t, 1. };
		logy = dot(monomials, mul(cf, M));

	}
	else { //if ( logIn >= log10(C.maxPoint.x) ) { 

		logy = logx * C.slopeHigh + (log10(C.maxPoint.y) - C.slopeHigh * log10(C.maxPoint.x));

	}

	return pow10(logy);
}

float segmented_spline_c9_fwd(float x)
{
	return segmented_spline_c9_fwd(x, ODT_48nits);
}










