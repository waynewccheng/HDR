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

// ACES implementation

#pragma once



/*
* Simple math utils to handle ACES spline math
*
*/
struct Float2
{
	float X;
	float Y;

	float& operator[](int i)
	{
		if (i == 0)
			return X;
		return Y;
	}

	const float& operator[](int i) const
	{
		if (i == 0)
			return X;
		return Y;
	}
};

struct Float3x3
{
	float m[9];
};

struct Float3
{
	float X;
	float Y;
	float Z;

	static float Dot(const Float3& lhs, const Float3& rhs)
	{
		return lhs.X*rhs.X + lhs.Y*rhs.Y + lhs.Z*rhs.Z;
	}

	static Float3 Lerp(const Float3& lhs, const Float3& rhs, const float a)
	{
		const float b = 1.0f - a;
		return lhs* b + rhs * a;
	}

	friend Float3 operator*(float x, const Float3& rhs)
	{
		Float3 ret = rhs;
		ret.X *= x;
		ret.Y *= x;
		ret.Z *= x;
		return ret;
	}

	friend Float3 operator*(const Float3& lhs, float x)
	{
		Float3 ret = lhs;
		ret.X *= x;
		ret.Y *= x;
		ret.Z *= x;
		return ret;
	}

	friend Float3 operator+(const Float3& lhs, const Float3& rhs)
	{
		Float3 ret = lhs;
		ret.X += rhs.X;
		ret.Y += rhs.Y;
		ret.Z += rhs.Z;
		return ret;
	}

	float& operator[](int i)
	{
		if (i == 0)
			return X;
		if (i == 1)
			return Y;
		return Z;
	}

	const float& operator[](int i) const
	{
		if (i == 0)
			return X;
		if (i == 1)
			return Y;
		return Z;
	}
};

inline Float3 mul(const Float3x3& m, const Float3& v)
{
	Float3 res;

	res.X = v.X * m.m[0] + v.Y * m.m[1] + v.Z * m.m[2];
	res.Y = v.X * m.m[3] + v.Y * m.m[4] + v.Z * m.m[5];
	res.Z = v.X * m.m[6] + v.Y * m.m[7] + v.Z * m.m[8];

	return res;
}


struct Float4
{
	float X;
	float Y;
	float Z;
	float W;

	Float4(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w)
	{}

	Float4() : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f)
	{}
};

// simplified fp32/fp16 conversion, good enough for our purposes
inline unsigned short float2half(float f)
{
	union fasi { float f; unsigned int i; };
	fasi v;
	v.f = f;

	unsigned short sign = (v.i >> 31) & 0x1;
	short exp = ((v.i >> 23) & 0xff) - 127;
	unsigned short mant = (v.i >> 13) & 0x3ff;

	if (exp < -14)
	{
		exp = 0;
		mant = (0x400 & mant) >> (-exp - 14);
	}
	else if (exp < 16)
	{
		exp += 15;
	}
	else
	{
		// just make it inf, ignore NaN
		exp = 31;
		mant = 0;
	}

	return (sign << 15) | (exp << 10) | mant;
}

typedef enum ODTCurve
{
	// reference curves, no parameterization
	ODT_LDR_Ref,
	ODT_1000Nit_Ref,
	ODT_2000Nit_Ref,
	ODT_4000Nit_Ref,

	// Adjustable curves, parameterized for range, level, etc
	ODT_LDR_Adj,
	ODT_1000Nit_Adj,
	ODT_2000Nit_Adj,
	ODT_4000Nit_Adj,

	ODT_Invalid = 0xffffffff 
};


/*
* Struct with ODT spline parameters, configured for easy DX constant buffer compatibility
*/
struct SegmentedSplineParams_c9
{
	Float4 coefs[10];
	Float2 minPoint; // {luminance, luminance} linear extension below this
	Float2 midPoint; // {luminance, luminance} 
	Float2 maxPoint; // {luminance, luminance} linear extension above this
	Float2 slope;
	Float2 limits; // limits in ODT curve prior to RRT adjustment
};

SegmentedSplineParams_c9 GetAcesODTData(ODTCurve BaseCurve, float MinStop, float MaxStop, float MaxLevel, float MidGrayScale);

struct ACESparams
{
	SegmentedSplineParams_c9 C;
	Float3x3 XYZ_2_DISPLAY_PRI_MAT;
	Float3x3 DISPLAY_PRI_MAT_2_XYZ;
	Float2 CinemaLimits;
	int OutputMode;
	float surroundGamma;
	bool desaturate;
	bool surroundAdjust;
	bool applyCAT;
	bool tonemapLuminance;
	float saturationLevel;
};

Float3 EvalACES(Float3 InColor, const ACESparams& Params);