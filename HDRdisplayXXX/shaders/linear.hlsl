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

#include "ACES/ACES_util.hlsl"
#include "utilities.hlsl"


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


cbuffer cbObject : register(b0)
{
	float scale;
	float gamma;
	int mode;
	row_major float3x3 OutputColorXform;
};

static const float3x3 sRGB_2_XYZ_MAT =
{
	0.41239089f, 0.35758430f, 0.18048084f,
	0.21263906f, 0.71516860f, 0.07219233f,
	0.01933082f, 0.11919472f, 0.95053232f
};

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader

float4 main(VS_OUTPUT input) : SV_Target0
{
	float4 texLookup = texInput.SampleLevel(sampLinearWrap, input.TC, 0);

	float3 rgb = texLookup.rgb;

	// adjust level
	rgb *= scale;

	//convert color spaces
	float3 xyz = mul(sRGB_2_XYZ_MAT, rgb);
	rgb = mul(OutputColorXform, xyz);

	// encode gamma/PQ
	if (mode == 0)
	{
		//sRGB
		static const float DISPGAMMA = 2.4;
		static const float OFFSET = 0.055;

		rgb[0] = moncurve_r(rgb[0], DISPGAMMA, OFFSET);
		rgb[1] = moncurve_r(rgb[1], DISPGAMMA, OFFSET);
		rgb[2] = moncurve_r(rgb[2], DISPGAMMA, OFFSET);

	}
	else if (mode == 1)
	{
		//gamma
		rgb = pow( max(rgb, 0.0), 1.0/gamma);
	}
	else if (mode == 2)
	{
		//PQ
		rgb = pq_r_f3(rgb);

	}
	// mode == 3 use scRGB, no extra encoding
	
	//encode it with sRGB, to counteract a Windows display driver transform
	if (mode != 3)
	{
		rgb = sRGB_2_Linear(rgb);
	}

	return float4(rgb, 1);
}


