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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREZSTDCODEC_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREZSTDCODEC_H__

#include <Renderer/VirtualTexturing/SIRVirtualTextureClusterDecode.h>

// Uses the approved native Zstd dependency for the donor's V1 outer cluster
// codec. No texture-format or package-format translation occurs.
class SIRVirtualTextureZstdCodec final
{
public:
	static bool available( void ) noexcept;
	static bool decode( void *pContext, const sirVirtualTextureCodecFrame_t& frame, uint32_t *pProducedBytes ) noexcept;
	static sirVirtualTextureClusterCodec_t makeDecoder( void ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRVIRTUALTEXTUREZSTDCODEC_H__
