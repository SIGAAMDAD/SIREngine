/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureZstdCodec.h>

#include <zstd.h>

static bool sirVTDecodeNativeZstdFrame( void *, const sirVirtualTextureCodecFrame_t& frame, uint32_t *pProducedBytes ) noexcept
{
	if ( pProducedBytes == nullptr ) {
		return false;
	}
	*pProducedBytes = 0;
	if ( frame.pInput == nullptr || frame.pOutput == nullptr || frame.nInputBytes == 0 || frame.nOutputCapacity == 0 ) {
		return false;
	}
	const uintptr_t nInput = ( uintptr_t )frame.pInput;
	const uintptr_t nOutput = ( uintptr_t )frame.pOutput;
	if ( frame.nInputBytes > UINTPTR_MAX - nInput || frame.nOutputCapacity > UINTPTR_MAX - nOutput ||
	     ( nInput < nOutput + frame.nOutputCapacity && nOutput < nInput + frame.nInputBytes ) ) {
		return false;
	}
	const size_t nDecodedBytes = ZSTD_decompress( frame.pOutput, frame.nOutputCapacity, frame.pInput, frame.nInputBytes );
	if ( ZSTD_isError( nDecodedBytes ) || nDecodedBytes > UINT32_MAX ) {
		return false;
	}
	*pProducedBytes = ( uint32_t )nDecodedBytes;
	return true;
}

const sirVirtualTextureClusterCodec_t *sirVTGetZstdClusterCodec( void ) noexcept
{
	static const sirVirtualTextureClusterCodec_t s_Codec = { nullptr, sirVTDecodeNativeZstdFrame };
	return &s_Codec;
}
