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

Texture3D LUT : register(t1);

SamplerState sampLinearClamp : register(s1);

////////////////////////////////////////////////////////////////////////////////
// IO Structures

struct VS_OUTPUT
{
	float4 P  : SV_POSITION;
	float2 TC : TEXCOORD0;
};

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader


cbuffer Constants : register(b0)
{
	float scale;
	float bias;
	uint scRGB_output;
	uint shaper_mode;
};


float3 pq(float3 val)
{
	val.x = pq_r(val.x);
	val.y = pq_r(val.y);
	val.z = pq_r(val.z);

	return val;
}

//Derived from rec709 ODT
float4 main(VS_OUTPUT input) : SV_Target0
{
	float4 texLookup = texInput.SampleLevel(sampLinearWrap, input.TC, 0);

	float3 lutCoord = 0.0f;

	if (shaper_mode == 0)
	{
		lutCoord = log2(texLookup.rgb) * scale + bias;
	}
	else if (shaper_mode == 1)
	{
		lutCoord = pq(texLookup.rgb * scale + bias);
	}
	else
	{
		// make the fallthrough case look obviously wrong
		lutCoord = 1.0 - (texLookup.rgb * scale + bias);
	}

	float3 outputCV = LUT.SampleLevel(sampLinearClamp, lutCoord , 0).rgb;
	
	if (scRGB_output == 0)
	{
		//encode it with sRGB, to counteract a Windows display driver transform
		outputCV = sRGB_2_Linear(outputCV);
	}

	return float4(outputCV, 1);
}


