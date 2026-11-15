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

#ifndef __SIRENGINE_SIRVIRTUALTEXTURECOOKER_H__
#define __SIRENGINE_SIRVIRTUALTEXTURECOOKER_H__

#include <SIRPCH.hpp>

// Cold editor input. All source images are borrowed for the duration of the
// call; the published Sparse V1 package never retains any image pointers.
typedef struct sirVirtualTextureCookImage_s
{
	const uint8_t *pPixels;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nComponents;
	uint32_t nRowBytes;
	uint64_t nBackingBytes; // Borrowed buffer size; required for every authored channel.
} sirVirtualTextureCookImage_t;

typedef struct sirVirtualTextureCookSource_s
{
	uint64_t nStableKey;
	sirVirtualTextureCookImage_t images[6];
	uint64_t nSourceFingerprint[6];
	uint32_t nAuthoredChannelMask;
	uint32_t nOrmSourceLayout;
	uint32_t nWrapMode;
	uint32_t nConsumer;
	uint32_t nAllocationFlags;
	uint32_t nGenerationSeed;
	uint8_t bAlphaScissor;
	uint8_t nAlphaThreshold;
} sirVirtualTextureCookSource_t;

typedef struct sirVirtualTextureCookAllocator_s
{
	void *pUser;
	void *( *pfnAllocate )( void *pUser, uint64_t nBytes, uint32_t nAlignment );
	void ( *pfnFree )( void *pUser, void *pMemory );
} sirVirtualTextureCookAllocator_t;

typedef struct sirVirtualTextureCookBatch_s
{
	const sirVirtualTextureCookSource_t *pSources;
	uint32_t nSourceCount;
	sirVirtualTextureCookAllocator_t allocator;
	bool bZstd;
} sirVirtualTextureCookBatch_t;

typedef struct sirVirtualTextureCookOutput_s
{
	uint8_t *pBytes;
	uint64_t nCapacity;
	uint64_t nWritten;
} sirVirtualTextureCookOutput_t;

// Native boundary for the original Terrain3D reference encoder, Sparse V1
// cooker and pack serializer. Callers own source data, transient allocations,
// and output capacity. The package is independent of editable source files.
class SIRVirtualTextureCooker final
{
public:
	static bool supportsZstd( void ) noexcept;
	static bool cook( const sirVirtualTextureCookBatch_t& batch, sirVirtualTextureCookOutput_t& output ) noexcept;
	static bool publishAtomic( const char *pszDestination, const char *pszStaging, const sirVirtualTextureCookOutput_t& output ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRVIRTUALTEXTURECOOKER_H__
