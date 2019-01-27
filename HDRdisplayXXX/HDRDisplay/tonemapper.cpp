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

#include "tonemapper.h"

#include "ACES.h"

const float Tonemapper::ColorMatrices[12 * 3] =
{
	// rec 709
	3.24096942f, -1.53738296f, -0.49861076f, 0.0f,
	-0.96924388f, 1.87596786f, 0.04155510f, 0.0f,
	0.05563002f, -0.20397684f, 1.05697131f, 0.0f,
	// DCI-P3
	2.72539496f, -1.01800334f, -0.44016343f, 0.0f,
	-0.79516816f, 1.68973231f, 0.02264720f, 0.0f,
	0.04124193f, -0.08763910f, 1.10092998f, 0.0f,
	// BT2020
	1.71665096f, -0.35567081f, -0.25336623f, 0.0f,
	-0.66668433f, 1.61648130f, 0.01576854f, 0.0f,
	0.01763985f, -0.04277061f, 0.94210327f, 0.0f,
};

const float Tonemapper::ColorMatricesInv[12 * 3] =
{
	//rec709 to XYZ
	0.41239089f, 0.35758430f, 0.18048084f, 0.0f,
	0.21263906f, 0.71516860f, 0.07219233f, 0.0f,
	0.01933082f, 0.11919472f, 0.95053232f, 0.0f,
	//DCI - P3 2 XYZ
	0.44516969f, 0.27713439f, 0.17228261f, 0.0f,
	0.20949161f, 0.72159523f, 0.06891304f, 0.0f,
	0.00000000f, 0.04706058f, 0.90735501f, 0.0f,
	//bt2020 2 XYZ
	0.63695812f, 0.14461692f, 0.16888094f, 0.0f,
	0.26270023f, 0.67799807f, 0.05930171f, 0.0f,
	0.00000000f, 0.02807269f, 1.06098485f, 0.0f
};


/////////////////////////////////////////////////////////////////////////////////////////
//
// Tonemapper functions
//
/////////////////////////////////////////////////////////////////////////////////////////


