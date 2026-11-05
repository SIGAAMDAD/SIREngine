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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREPACKFILESOURCE_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREPACKFILESOURCE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRPlatformFile.h>
#include <Renderer/VirtualTexturing/SIRVirtualTextureClusterDecode.h>

typedef struct sirVirtualTexturePackMetadataBuffer_s
{
	uint8_t *pBytes;
	uint64_t nCapacity;
} sirVirtualTexturePackMetadataBuffer_t;

// Native Terrain3D Sparse V1 file source, without Godot FileAccess or heap
// payload staging. Metadata remains caller-owned; the file remains open for
// bounded payload reads. Same file must remain immutable while in use.
class SIRVirtualTexturePackFileSource final
{
public:
	SIRVirtualTexturePackFileSource( void ) noexcept;
	~SIRVirtualTexturePackFileSource( void ) noexcept;

	uint64_t getFileBytes( void ) const noexcept;
	uint64_t getPayloadBytes( void ) const noexcept;
	uint32_t getPackageGeneration( void ) const noexcept;
	uint64_t getReadCount( void ) const noexcept;
	bool isOpen( void ) const noexcept;

	bool open( const char *pszPath, const sirVirtualTexturePackMetadataBuffer_t& buffer, bool bStrictHash,
	           sirVirtualTexturePackView_t *pView ) noexcept;
	bool readPayload( uint64_t nOffset, void *pDestination, uint32_t nBytes ) noexcept;
	bool readCluster( const sirVirtualTexturePackView_t& view, uint32_t nClusterId, const sirVirtualTextureClusterBuffers_t& buffers,
	                  const sirVirtualTextureClusterCodec_t *pCodec ) noexcept;
	void close( void ) noexcept;

protected:
private:
	SIRPlatformFile m_File;
	uint64_t m_nFileBytes;
	uint64_t m_nPayloadOffset;
	uint64_t m_nPayloadBytes;
	uint64_t m_nReadCount;
	uint32_t m_nPackageGeneration;
	uint64_t m_nPackageSignature;
};

#endif // __SIRENGINE_SIRVIRTUALTEXTUREPACKFILESOURCE_H__
