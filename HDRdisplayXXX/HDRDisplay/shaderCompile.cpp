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

#include "shaderCompile.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <vector>

std::string FindFile(const char* file, const char* alternate = nullptr)
{
	const char* paths[] =
	{
		"",
		"..\\..\\HDRDisplay\\assets\\shaders\\",
		".\\shaders\\"
	};
	struct _stat s;


	for (size_t i = 0; i < sizeof(paths) / sizeof(char*); i++)
	{
		std::string path = std::string(paths[i]) + std::string(file);

		if (_stat(path.c_str(), &s) == 0)
		{
			return path;
		}

		// try adding the sub-path
		if (alternate)
		{
			std::string path = std::string(paths[i]) + std::string(alternate) + std::string(file);

			if (_stat(path.c_str(), &s) == 0)
			{
				return path;
			}
		}
	}

	//failed, just return the original string
	return std::string(file);
}

struct Includer : public ID3DInclude
{
	std::string altPath;
	std::vector<char> buffer;
	/*
	std::string FindFile(const char* file)
	{
		const char* paths[] =
		{
			"",
			"..\\..\\HDRDisplay\\assets\\shaders\\",
			".\\shaders\\",
			"..\\..\\HDRDisplay\\assets\\shaders\\ACES\\",
			".\\shaders\\ACES\\"
		};
		struct _stat s;


		for (size_t i = 0; i < sizeof(paths) / sizeof(char*); i++)
		{
			std::string path = std::string(paths[i]) + std::string(file);

			if (_stat(path.c_str(), &s) == 0)
			{
				return path;
			}
		}

		//failed, just return the original string
		return std::string(file);
	}
	*/

	Includer(const std::string alt) : altPath(alt)
	{
	}

	Includer()
	{
	}

	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
	{
		std::string path = FindFile(pFileName, altPath.c_str());

		FILE* fp = fopen(path.c_str(), "rb");
		if (!fp)
		{
			return E_FAIL;
		}

		fseek(fp, 0, SEEK_END);
		int size = ftell(fp);

		buffer.resize(size + 1);

		fseek(fp, 0, SEEK_SET);
		fread(&buffer[0], 1, size, fp);
		buffer[size] = 0;

		*ppData = &buffer[0];
		*pBytes = size;

		fclose(fp);
		return S_OK;
	}

	HRESULT Close(LPCVOID pData)
	{
		buffer.clear();
		return S_OK;
	}
};

HRESULT CompileShaderFromFile(
	const char*             pSrcFile,
	const D3D_SHADER_MACRO            *pDefines,
	LPCSTR              pFunctionName,
	LPCSTR              pProfile,
	DWORD               Flags,
	ID3DBlob         **ppShader,
	ID3DBlob         **ppErrorMsgs
	)
{
	//initialize the error pointer
	*ppErrorMsgs = nullptr;

	FILE *fp = nullptr;

	// extract an alternate root path
	size_t pos = std::string(pSrcFile).find_last_of("\\/");
	std::string alt = pos != std::string::npos ? std::string(pSrcFile, pos + 1) : std::string();

	//search in expected paths
	std::string fn = FindFile(pSrcFile);

	fp = fopen(fn.c_str(), "rb");

	if (!fp)
	{
		return ERROR_FILE_NOT_FOUND;
	}


	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);

	std::vector<char> buffer;
	buffer.resize(size + 1);

	fseek(fp, 0, SEEK_SET);
	fread(&buffer[0], 1, size, fp);
	buffer[size] = 0;

	fclose(fp);

	Includer inc(alt);

	HRESULT hr = D3DCompile(
		&buffer[0],
		size,
		pSrcFile,
		pDefines,
		&inc, //D3D_COMPILE_STANDARD_FILE_INCLUDE,
		pFunctionName,
		pProfile,
		Flags,
		0,
		ppShader,
		ppErrorMsgs
		);

	return hr;
}