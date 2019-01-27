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

// Simple reduction to compute log mean luminance for automatic exposure

#pragma once

#define NOMINMAX

#include <d3d11.h>
#include "shaderCompile.h"
#include "common_util.h"

class ExposureReduction
{
	ID3D11Device *device;
	ID3D11ComputeShader *shader;
	ID3D11Buffer *constants;

	// setup to match constant buffer layout
	struct ConstantData
	{
		unsigned int dimensions[2];
		unsigned int mode;
		float scale;
		unsigned int groupCount[2];
		unsigned int elements;
		float pad;
	};

	ID3D11Buffer *tempBuffer[2];
	ID3D11UnorderedAccessView *tempUAV[2];
	ID3D11ShaderResourceView *tempSRV[2];

	int currentWidth;
	int currentHeight;

	void UpdateConstants(ID3D11DeviceContext *ctx, const ConstantData& data)
	{
		D3D11_MAPPED_SUBRESOURCE map;

		ctx->Map(constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

		memcpy(map.pData, &data, sizeof(ConstantData));

		ctx->Unmap(constants, 0);
	}

	void CreateIntermediates()
	{

		for (int i = 0; i < 2; i++)
		{
			SAFE_RELEASE(tempSRV[i]);
			SAFE_RELEASE(tempUAV[i]);
			SAFE_RELEASE(tempBuffer[i]);
		}

		const size_t size = (currentWidth*currentHeight + 255) / 256;
		D3D11_BUFFER_DESC desc;

		desc.StructureByteStride = 0;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.ByteWidth = unsigned int(size * sizeof(float));
		desc.CPUAccessFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.MiscFlags = 0;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = unsigned int(size);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

		uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = unsigned int(size);
		uavDesc.Buffer.Flags = 0;

		for (int i = 0; i < 2; i++)
		{
			device->CreateBuffer(&desc, nullptr, &(tempBuffer[i]));
			device->CreateShaderResourceView(tempBuffer[i], &srvDesc, &(tempSRV[i]));
			device->CreateUnorderedAccessView(tempBuffer[i], &uavDesc, &(tempUAV[i]));
		}
	}

public:

	ExposureReduction( ID3D11Device *inDevice) :
		device(inDevice),
		currentWidth(3840),
		currentHeight(2160)
	{
		D3D11_BUFFER_DESC desc;

		desc.StructureByteStride = 0;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.ByteWidth = unsigned int(sizeof(ConstantData));
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.MiscFlags = 0;

		device->CreateBuffer(&desc, nullptr, &constants);

		shader = nullptr;
		HRESULT hr = S_OK;
		ID3DBlob* pErrorBlob = nullptr;
		ID3DBlob* pShaderBuffer = nullptr;

		hr = CompileShaderFromFile("exposure_reduction.hlsl", NULL, "main", "cs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, &pShaderBuffer, &pErrorBlob);
		if (FAILED(hr))
		{
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		}
		else
		{
			device->CreateComputeShader(pShaderBuffer->GetBufferPointer(), pShaderBuffer->GetBufferSize(), nullptr, &shader);
		}
		SAFE_RELEASE(pErrorBlob);

		for (int i = 0; i < 2; i++)
		{
			tempSRV[i] = nullptr;
			tempUAV[i] = nullptr;
			tempBuffer[i] = nullptr;
		}

		CreateIntermediates();

	}

	~ExposureReduction()
	{
		SAFE_RELEASE(shader);
		SAFE_RELEASE(constants);

		for (int i = 0; i < 2; i++)
		{
			SAFE_RELEASE(tempSRV[i]);
			SAFE_RELEASE(tempUAV[i]);
			SAFE_RELEASE(tempBuffer[i]);
		}
		
	}


	void Process(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView * srv, ID3D11UnorderedAccessView *uav, int width, int height)
	{
		if (width > currentWidth || height > currentHeight)
		{
			CreateIntermediates();
		}

		ConstantData data;

		data.dimensions[0] = width;
		data.dimensions[1] = height;
		data.mode = 0;
		data.scale = float(1.0 / (width * height));
		data.groupCount[0] = (width + 15) / 16;
		data.groupCount[1] = (height + 15) / 16;

		// first pass does the sampling from the 2D texture, and reduces to the buffer
		ctx->CSSetShader(shader, nullptr, 0);

		UpdateConstants( ctx, data);
		
		ctx->CSSetShaderResources(0, 1, &srv);
		ctx->CSSetShaderResources(1, 1, &(tempSRV[1])); // not actually used on pass 1
		ctx->CSSetUnorderedAccessViews(0, 1, &(tempUAV[0]), nullptr);
		ctx->CSSetConstantBuffers(0, 1, &constants);
		
		ctx->Dispatch((width + 15) / 16, (height + 15) / 16, 1);

		// for resetting, to silence D3D warnings about input/output collisions
		ID3D11UnorderedAccessView *nullUAV = nullptr;

		int srcUAV = 0;
		int numBlocks = ((width + 15) / 16) * ((height + 15) / 16);

		// intermediate passes reduce from one temp buffer to the other
		while ( numBlocks >= 256)
		{
			ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			ctx->CSSetShaderResources(1, 1, &(tempSRV[srcUAV])); 
			ctx->CSSetUnorderedAccessViews(0, 1, &(tempUAV[!srcUAV]), nullptr);

			data.mode = 1;
			data.groupCount[0] = (numBlocks + 255) / 256;
			data.groupCount[1] = 1;
			data.elements = numBlocks;
			data.scale = 1.0f;

			UpdateConstants(ctx, data);

			ctx->CSSetConstantBuffers(0, 1, &constants);

			ctx->Dispatch((numBlocks + 255) / 256, 1, 1);

			// reverse the buffers
			srcUAV = !srcUAV;

			// decrement
			numBlocks = (numBlocks + 255) / 256;
		}

		data.mode = 1;
		data.groupCount[0] = 1;
		data.groupCount[1] = 1;
		data.elements = numBlocks;
		data.scale = 1.0f;

		UpdateConstants(ctx, data);

		ctx->CSSetConstantBuffers(0, 1, &constants);

		// final pass reduces to the output uav
		ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		ctx->CSSetShaderResources(1, 1, &(tempSRV[srcUAV]));
		ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		ctx->Dispatch(1, 1, 1);

		ctx->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	}
};
