/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_TEST_AUDIO_MANIFEST_SOURCE_FIXTURE_H__
#define __SIRENGINE_TEST_AUDIO_MANIFEST_SOURCE_FIXTURE_H__

#include <Audio/SIRAudioResources.h>
#include <Resources/SIRResourceGraph.h>
#include <stdlib.h>
#include <string.h>

static void *sirTestAudioSourceAllocate( void *, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	void *pData = nullptr;
#ifdef _WIN32
	pData = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nAlign = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pData, nAlign, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	return pData;
}

static void sirTestAudioSourceFree( void *, void *pData, uint64_t, uint32_t, uint32_t ) noexcept
{
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static SIRResourceGraphConfig sirTestAudioSourceConfig( void ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { nullptr, sirTestAudioSourceAllocate, sirTestAudioSourceFree };
	config.m_nMetadataDomain = 1;
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 16;
	config.m_nMaxProviderPins = 8;
	config.m_nMaxCommittedBytes = 1024 * 1024;
	config.nMaxReaders = 16;
	return config;
}

struct sirTestAudioManifestSource_t
{
	SIRResourceGraph graph;
	sirTestAudioManifestSource_t( void ) noexcept : graph( sirTestAudioSourceConfig() )
	{
	}

	sirAudioManifestResult_t load( SIRAudioResourceStore& rStore, const void *pData, size_t nBytes,
	                               const sirContainerAllocator_t *pAllocator ) noexcept
	{
		if ( !graph.isReady() || !pData || !nBytes || nBytes > UINT32_MAX ) {
			return SIR_AUDIO_MANIFEST_INVALID_ARGUMENT;
		}
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		SIRResourceStagingHandle hStaging = {};
		SIRResourceMutableBytes staging = {};
		if ( !pWriter || pWriter->allocateStaging( nBytes, 16, 1, &hStaging, &staging ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_AUDIO_MANIFEST_OUT_OF_MEMORY;
		}
		memcpy( staging.m_pData, pData, nBytes );
		const uint8_t *pSource = ( const uint8_t * )pData;
		uint64_t nHash = UINT64_C( 0xcbf29ce484222325 );
		for ( size_t i = 0; i < nBytes; ++i ) {
			nHash = ( nHash ^ pSource[i] ) * UINT64_C( 0x100000001b3 );
		}
		const SIRResourceChunkPublication chunk = { { 1 }, { nHash, nHash ^ UINT64_C( 0x42cbb718 ), 1, 0 }, hStaging, false };
		const SIRResourcePublication publication = { { 790, 11, 0 }, SIR_RESOURCE_KIND_BLOB, 1, 1, &chunk, 1, nullptr, 0 };
		SIRResourceHandle hManifest = {};
		if ( pWriter->publish( publication, &hManifest ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_AUDIO_MANIFEST_INVALID_ARGUMENT;
		}
		const sirAudioManifestLoadOptions_t options = { pAllocator, 0, SIR_AUDIO_PROVIDER_FMOD, 0x41554449 };
		return rStore.load( graph, hManifest, { 1 }, options );
	}
};

#endif
