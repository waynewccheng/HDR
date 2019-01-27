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



////////////////////////////////////////////////////////////////////////////////
// Resources

Texture2D texInput : register(t0);

Buffer<float> bufInput : register(t1);

RWBuffer<float> uav : register(u0);

////////////////////////////////////////////////////////////////////////////////
// IO Structures




cbuffer cbObject : register(b0)
{
	uint2 dimensions;
	uint mode;
	float scale;
	uint2 groupCount;
	uint elements;
};


////////////////////////////////////////////////////////////////////////////////
// Compute Shader


groupshared float logLumArray[16 * 16];

void ReduceArray(uint index, uint stride)
{
	if (index < stride)
	{
		logLumArray[index] = logLumArray[index] + logLumArray[index + stride];
	}
}

[numthreads(16,16,1)]
void main(uint3 group : SV_GroupID, uint3 thread : SV_GroupThreadID, uint3 dispatchThread : SV_DispatchThreadID)
{

	uint threadIdx = thread.y * 16 + thread.x;

	for (int index = threadIdx; index < 256; index += 256) {
		logLumArray[index] = 0.f;
	}

	GroupMemoryBarrierWithGroupSync();

	//fetch the data
	float lum = 0.0f;
	if (mode == 0)
	{
		//pass 0, reducing from 2D framebuffer with RGB
		float luminance = 0.0f;

		if (all(dispatchThread.xy < dimensions.xy))
		{
			float4 rgb = texInput.Load(uint3(dispatchThread.xy, 0));

			luminance = dot(rgb.xyz, float3(0.41239089f, 0.35758430f, 0.18048084f));

			// prevent any NaNs from bad data
			luminance = max(0.0f, luminance);

			luminance = log2(luminance + 0.0000001);
		}

		logLumArray[threadIdx] = luminance;
	}
	else
	{
		// pass n, groups are really linear
		uint index = group.x * 256 + thread.y * 16 + thread.x;
		logLumArray[threadIdx] = (index < elements) ? bufInput.Load(index) : 0.0f;
	}

	GroupMemoryBarrierWithGroupSync();

	ReduceArray(threadIdx, 128);

	GroupMemoryBarrierWithGroupSync();

	ReduceArray(threadIdx, 64);

	GroupMemoryBarrierWithGroupSync();

	ReduceArray(threadIdx, 32);

	GroupMemoryBarrierWithGroupSync();

	ReduceArray(threadIdx, 16);

	GroupMemoryBarrierWithGroupSync();

	// finish reduction and output on thread 0
	if (threadIdx == 0)
	{
		float result = logLumArray[0];

		for (int i = 1; i < 16; i++)
		{
			result += logLumArray[i];
		}

		uint groupIndex = group.y * groupCount.x + group.x;

		uav[groupIndex] =  result * scale;

	}

}

