/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D Contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureZstdCodec.h>

#if defined( SIR_VT_WITH_ZSTD )
#include <zstd.h>
#endif

bool SIRVirtualTextureZstdCodec::available( void ) noexcept
{
#if defined( SIR_VT_WITH_ZSTD )
	return true;
#else
	return false;
#endif
}

bool SIRVirtualTextureZstdCodec::decode( void *, const sirVirtualTextureCodecFrame_t& frame, uint32_t *pProducedBytes ) noexcept
{
	if ( pProducedBytes == nullptr ) {
		return false;
	}
	*pProducedBytes = 0u;
#if defined( SIR_VT_WITH_ZSTD )
	if ( frame.pInput == nullptr || frame.pOutput == nullptr || frame.nInputBytes == 0u || frame.nOutputCapacity == 0u ) {
		return false;
	}
	const size_t decoded = ZSTD_decompress( frame.pOutput, frame.nOutputCapacity, frame.pInput, frame.nInputBytes );
	if ( ZSTD_isError( decoded ) != 0u || decoded == 0u || decoded > UINT32_MAX ) {
		return false;
	}
	*pProducedBytes = ( uint32_t )decoded;
	return true;
#else
	( void )frame;
	return false;
#endif
}

sirVirtualTextureClusterCodec_t SIRVirtualTextureZstdCodec::makeDecoder( void ) noexcept
{
	return { nullptr, available() ? &SIRVirtualTextureZstdCodec::decode : nullptr };
}
