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


////////////////////////////////////////////////////////////////////////////////
// IO Structures

struct VS_OUTPUT
{
	float4 P  : SV_POSITION;
	float2 TC : TEXCOORD0;
};


cbuffer cbObject : register(b0)
{
	float2 brightness;
	uint2 cIndex;
	uint mode;
	float size;
	
};


////////////////////////////////////////////////////////////////////////////////
// L*a*b* colors for color checker with D50 illuminant
//  Values are encoded [0,1]
static const float3 colors[14*10] =
{
	// row 0, grayscale only
	{ 1.0f, 0.5f, 0.5f }, // white
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f },
//	{ 0.965820f, 0.496338f, 0.502441f },
	{ 1.965820f, 0.496338f, 0.502441f },
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f },
	{ 0.965820f, 0.496338f, 0.502441f },
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f },
	{ 0.965820f, 0.496338f, 0.502441f },
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f }, // black
	{ 0.497314f, 0.499268f, 0.500488f }, // grey
	{ 0.965820f, 0.496338f, 0.502441f }, // white

	// row 1
	{ 0.065369f, 0.499756f, 0.497559f }, // black
	{ 0.325928f, 0.701660f, 0.457764f },
	{ 0.218140f, 0.567871f, 0.428467f },
	{ 0.841797f, 0.492188f, 0.468262f },
	{ 0.319092f, 0.572754f, 0.585938f },
	{ 0.634277f, 0.578613f, 0.575195f },
	{ 0.463623f, 0.479980f, 0.404785f },
	{ 0.381836f, 0.433594f, 0.620605f },
	{ 0.515625f, 0.535645f, 0.395264f },
	{ 0.689453f, 0.364746f, 0.499023f },
	{ 0.851563f, 0.542480f, 0.567871f },
	{ 0.218506f, 0.634277f, 0.530762f },
	{ 0.418457f, 0.742188f, 0.539063f },
	{ 0.497314f, 0.499268f, 0.500488f }, // grey

	// row 2
	{ 0.497314f, 0.499268f, 0.500488f }, // grey
	{ 0.607422f, 0.602051f, 0.427246f },
	{ 0.415283f, 0.572266f, 0.354736f },
	{ 0.847656f, 0.556641f, 0.501465f },
	{ 0.607422f, 0.650879f, 0.777344f },
	{ 0.349365f, 0.545410f, 0.302246f },
	{ 0.470703f, 0.707031f, 0.580078f },
	{ 0.213135f, 0.613770f, 0.392822f },
	{ 0.708496f, 0.405029f, 0.752930f },
	{ 0.696777f, 0.578613f, 0.807129f },
	{ 0.897461f, 0.435303f, 0.524414f },
	{ 0.426758f, 0.763184f, 0.689453f },
	{ 0.197754f, 0.613770f, 0.469482f },
	{ 0.065369f, 0.499756f, 0.497559f }, // black

	// row 3
	{ 0.965820f, 0.496338f, 0.502441f }, // white
	{ 0.286865f, 0.688477f, 0.347900f },
	{ 0.199829f, 0.499268f, 0.358398f },
	{ 0.848633f, 0.425293f, 0.497070f },
	{ 0.193481f, 0.586426f, 0.270264f },
	{ 0.520508f, 0.327393f, 0.652344f },
	{ 0.360352f, 0.753418f, 0.650391f },
	{ 0.805664f, 0.515137f, 0.850098f },
	{ 0.480469f, 0.715820f, 0.439209f },
	{ 0.478027f, 0.370117f, 0.382324f },
	{ 0.845215f, 0.519531f, 0.476563f },
	{ 0.603516f, 0.642578f, 0.514160f },
	{ 0.395508f, 0.757813f, 0.631836f },
	{ 0.965820f, 0.496338f, 0.502441f }, // white

	// row 4
	{ 0.065369f, 0.499756f, 0.497559f }, // black
	{ 0.493896f, 0.439453f, 0.311035f },
	{ 0.601563f, 0.427734f, 0.377686f },
	{ 0.851563f, 0.552734f, 0.526855f },
	{ 1.0f, 0.5f, 0.5f },
	{ 1.2f, 0.5f, 0.5f  },
	{ 1.4f, 0.5f, 0.5f  },
	{ 1.6f, 0.5f, 0.5f  },
	{ 1.8f, 0.5f, 0.5f  },
	{ 2.0f, 0.5f, 0.5f  },
	{ 2.2f, 0.5f, 0.5f  },
	{ 2.4f, 0.5f, 0.5f  },
	{ 0.523926f, 0.767090f, 0.687012f },
	{ 0.497314f, 0.499268f, 0.500488f }, // grey

	// row 5
	{ 0.497314f, 0.499268f, 0.500488f }, // grey
	{ 0.606445f, 0.379639f, 0.397705f },
	{ 0.199463f, 0.429932f, 0.418213f },
	{ 0.841797f, 0.458984f, 0.604980f },
	{ 0.066650f, 0.500000f, 0.499023f },
	{ 0.306641f, 0.499512f, 0.498047f },
	{ 0.401367f, 0.499268f, 0.498779f },
	{ 0.602539f, 0.500000f, 0.498535f },
	{ 0.751465f, 0.500977f, 0.499512f },
	{ 0.890137f, 0.498291f, 0.498535f },
	{ 0.707520f, 0.500000f, 0.499023f },
	{ 0.620605f, 0.706055f, 0.801270f },
	{ 0.812500f, 0.594238f, 0.841797f },
	{ 0.065369f, 0.499756f, 0.497559f }, // black

	// row 6
	{ 0.965820f, 0.496338f, 0.502441f }, // white
	{ 0.192871f, 0.396973f, 0.476074f },
	{ 0.606934f, 0.476074f, 0.372070f },
	{ 0.617188f, 0.621094f, 0.642578f },
	{ 0.765137f, 0.581055f, 0.588867f },
	{ 0.635742f, 0.556152f, 0.602051f },
	{ 0.437744f, 0.564453f, 0.605957f },
	{ 0.673340f, 0.556152f, 0.566406f },
	{ 0.445557f, 0.602539f, 0.652344f },
	{ 0.634277f, 0.599121f, 0.602539f },
	{ 0.455811f, 0.499756f, 0.500977f },
	{ 0.724121f, 0.461426f, 0.850098f },
	{ 0.817871f, 0.526367f, 0.874023f },
	{ 0.965820f, 0.496338f, 0.502441f }, // white

	// row 7
	{ 0.065369f, 0.499756f, 0.497559f }, // black
	{ 0.601563f, 0.336670f, 0.450928f },
	{ 0.507813f, 0.305420f, 0.462646f },
	{ 0.643555f, 0.581055f, 0.574219f },
	{ 0.728027f, 0.613770f, 0.594727f },
	{ 0.643555f, 0.556641f, 0.566895f },
	{ 0.644043f, 0.566406f, 0.564941f },
	{ 0.646973f, 0.565918f, 0.572754f },
	{ 0.359131f, 0.564941f, 0.603516f },
	{ 0.657715f, 0.585938f, 0.608887f },
	{ 0.203003f, 0.500000f, 0.498779f },
	{ 0.620605f, 0.513672f, 0.722656f },
	{ 0.717285f, 0.436523f, 0.797852f },
	{ 0.497314f, 0.499268f, 0.500488f }, // grey

	// row 8
	{ 0.497314f, 0.499268f, 0.500488f }, // grey
	{ 0.214233f, 0.506348f, 0.534668f },
	{ 0.606445f, 0.344482f, 0.581055f },
	{ 0.503906f, 0.291992f, 0.557129f },
	{ 0.223267f, 0.419189f, 0.522461f },
	{ 0.600098f, 0.326660f, 0.533203f },
	{ 0.607910f, 0.383789f, 0.662109f },
	{ 0.511230f, 0.307373f, 0.673340f },
	{ 0.614746f, 0.293945f, 0.685059f },
	{ 0.614746f, 0.566895f, 0.698242f },
	{ 0.618164f, 0.447510f, 0.716797f },
	{ 0.719727f, 0.393066f, 0.788086f },
	{ 0.203125f, 0.556641f, 0.565430f },
	{ 0.065369f, 0.499756f, 0.497559f }, // black

	// row 9, grayscale only
	{ 0.965820f, 0.496338f, 0.502441f },
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f },
	{ 0.965820f, 0.496338f, 0.502441f },
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f },
	{ 0.965820f, 0.496338f, 0.502441f },
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f },
	{ 0.965820f, 0.496338f, 0.502441f },
	{ 0.497314f, 0.499268f, 0.500488f },
	{ 0.065369f, 0.499756f, 0.497559f },
	{ 0.497314f, 0.499268f, 0.500488f }, // grey
	{ 0.965820f, 0.496338f, 0.502441f }, // white
};

// funtion used in conversion of CIELAB to XYZ
float finv(float t)
{
	return t > 6.0f / 29.0f ? t*t*t : 3.0f * pow(6.0 / 29.0, 2.0) *(t - 4.0 / 29.0);
}

float3 Lab_2_xyz(float3 Lab)
{
	// D50 white
	float2 xyWhite = float2(0.34567, 0.35850);

	float3 white;
	white.x = 1.0f / xyWhite.y * xyWhite.x;
	white.z = 1.0f / xyWhite.y * (1.0f - xyWhite.x - xyWhite.y);
	white.y = 1.0f;

	// convert LAB to RGB
	float3 xyz;
	xyz.x = white.x * finv((Lab.x + 16.0f) / 116.0f + Lab.y / 500.0f);
	xyz.y = white.y * finv((Lab.x + 16.0f) / 116.0f);
	xyz.z = white.z * finv((Lab.x + 16.0f) / 116.0f - Lab.z / 200.0f);

	return xyz;
}

float3 remap_Lab(float3 color)
{
	// Lab wants L=[0,100] and ab=[-128,127] for standard conversion formulas 
	float3 lab = float3(color.x, (color.yz*255.0f - 128.0f)/100.0f) * 100.0f;
	
	return lab;
}
// conversion to sRGB primaries
static const float3x3 XYZ_2_sRGB =
{
	3.24096942f, -1.53738296f, -0.49861076f,
	-0.96924388f, 1.87596786f, 0.04155510f,
	0.05563002f, -0.20397684f, 1.05697131f
};

// Bradford CAT to reference D65
static const float3x3 D50_2_D65 =
{
	0.9555766f, -0.0230393f, 0.0631636f,
	-0.0282895f, 1.0099416f, 0.0210077f,
	0.0122982f, -0.0204830f, 1.3299098f
};


float3 color_chart(float2 uv)
{
	const float3x3 color_transform = mul(XYZ_2_sRGB, D50_2_D65);
	uv *= float2(14.0, 10.0);
	int2 bin = uv;
	float2 p = uv - bin;

	bool border = any(p < 0.1) || any(p > 0.9);

	int index = bin.x + bin.y * 14;

	return border ? 0.0f : mul(color_transform, Lab_2_xyz(remap_Lab(colors[index])));
}

float3 color_chart_simple(float2 uv)
{
	const float3x3 color_transform = mul(XYZ_2_sRGB, D50_2_D65);

	uv *= float2(6.0, 4.0);
	int2 bin = uv;
	float2 p = uv - bin;

	bool border = any(p < 0.1) || any(p > 0.9);

	int index = bin.x + 4 + (bin.y + 1) * 14;

	return border ? 0.0f : mul(color_transform, Lab_2_xyz(remap_Lab(colors[index])));
}

float3 color_square(float2 uv)
{
	const float3x3 color_transform = mul(XYZ_2_sRGB, D50_2_D65);
	
	// make things square
	uv.x = uv.x * 1.5 - 0.25;

	uv = uv*size - (size/2.0) + 0.5;

	bool border = any(uv < 0.0) || any(uv > 1.0);

	return border ? 0.0f : mul(color_transform, Lab_2_xyz(remap_Lab(colors[cIndex.x])));
}

float3 color_ramp(float2 uv)
{
	const float3x3 color_transform = mul(XYZ_2_sRGB, D50_2_D65);


	uv = uv*float2(1.2, size) - float2(0.1, (size/2.0) + 0.5);

	bool border = any(uv < 0.0) || any(uv > 1.0);

	float3 c0 = mul(color_transform, Lab_2_xyz(remap_Lab(colors[cIndex.x]))) * brightness.x;
	float3 c1 = mul(color_transform, Lab_2_xyz(remap_Lab(colors[cIndex.y]))) * brightness.y;

	return border ? 0.0f : lerp( c0, c1, uv.x);
}

////////////////////////////////////////////////////////////////////////////////

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
	float3 rgb = 0;

	if (mode == 0)
	{
		rgb = color_chart(input.TC) * brightness.x;
	}
	else if (mode == 1)
	{
		rgb = color_chart_simple(input.TC) * brightness.x;
	}
	else if (mode == 2)
	{
		// single square
		rgb = color_square(input.TC) * brightness.x;
	}
	else if (mode == 3)
	{
		// ramp
		rgb = color_ramp(input.TC);
	}
	else if (mode == 4)
	{
		// HDR check
		rgb = 1 + color_chart(input.TC) * 2.0f;
	}

	return float4(rgb, 1);
}


