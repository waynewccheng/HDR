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

Texture2D hdrTex : register(t0);
Texture2D ldrTex : register(t1);
Texture2D orgTex : register(t2);
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
	uint mode;
	uint texA;
	uint texB;
	float fade;
	float2 scroll;
};


////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader


float4 main(VS_OUTPUT input) : SV_Target0
{
	float3 rgb = 0;
	float3 texels[3];
	float3 a, b;

	texels[0] = hdrTex.Load(int3(input.P.xy, 0)).xyz;
	texels[1] = ldrTex.Load(int3(input.P.xy, 0)).xyz;
	texels[2] = orgTex.Load(int3(input.P.xy, 0)).xyz;

	a = texels[texA];
	b = texels[texB];

	if (mode == 0)
	{
		rgb = a;
	}
	else if (mode == 1)
	{
		rgb = input.TC.x < 0.49f ? a : rgb;
		rgb = input.TC.x > 0.51f ? b : rgb;
	}
	else if (mode == 2)
	{
		float2 uv = input.TC * 4.0f;
		int idx = int(uv.x) + int(uv.y);
		uv = frac(uv);
		rgb = (idx & 1) ? a : b;
		rgb = (uv.x < 0.005) || (uv.y < 0.005) || (uv.x > 0.995) || (uv.y > 0.995) ? 0.0f : rgb;
	}
	else if (mode == 3)
	{
		float2 uv = (input.TC + scroll) * 4.0f;
			int idx = int(uv.x) + int(uv.y);
		uv = frac(uv);
		rgb = (idx & 1) ? a : b;
		rgb = (uv.x < 0.005) || (uv.y < 0.005) || (uv.x > 0.995) || (uv.y > 0.995) ? 0.0f : rgb;
	}

	rgb = lerp(rgb, 0.0f, fade);

	return float4(rgb, 1);
}


