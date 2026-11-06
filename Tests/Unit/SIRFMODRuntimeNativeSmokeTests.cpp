/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Audio/FMOD/SIRFMODRuntime.h>
#include <stdlib.h>

typedef struct sirFMODRuntimeSmokeManifest_s
{
	sirAudioManifestHeader_t header;
	sirAudioPackageRecord_t package;
} sirFMODRuntimeSmokeManifest_t;

static void *smokeAllocate( void *, size_t nBytes, size_t ) noexcept
{
	return malloc( nBytes );
}

static void smokeFree( void *, void *pMemory, size_t, size_t ) noexcept
{
	free( pMemory );
}

static sirAudioResult_t smokeResolvePackage( void *, uint64_t, uint64_t, const char ** ) noexcept
{
	return SIR_AUDIO_INVALID_HANDLE;
}

static sirAudioResult_t smokeResolveEvent( void *, uint64_t, uint64_t, const char ** ) noexcept
{
	return SIR_AUDIO_INVALID_HANDLE;
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, smokeAllocate, smokeFree, 0, 0 };
	sirFMODRuntimeSmokeManifest_t image = {};
	image.header.nMagic = SIR_AUDIO_MANIFEST_MAGIC;
	image.header.nTotalBytes = sizeof( image );
	image.header.nPackageCount = 1;
	image.header.nVersion = SIR_AUDIO_MANIFEST_VERSION;
	image.header.nHeaderBytes = sizeof( image.header );
	image.header.eProvider = SIR_AUDIO_PROVIDER_FMOD;
	image.header.nEndianness = SIR_AUDIO_MANIFEST_LITTLE_ENDIAN;
	image.package = { 1, 101, 0, 0, 0 };
	image.header.nContentHash = SIRAudioResourceStore::hashPayload( ( const uint8_t * )&image + sizeof( image.header.nContentHash ),
	                                                                sizeof( image ) - sizeof( image.header.nContentHash ) );
	SIRAudioResourceStore manifest;
	if ( manifest.load( &image, sizeof( image ), SIR_AUDIO_PROVIDER_FMOD, 0, &allocator ) != SIR_AUDIO_MANIFEST_OK ) {
		return 1;
	}
	sirFMODRuntimeConfig_t config = {};
	config.context = { 16, -1, 1, 1, 0, 0 };
	config.audio = { 1, 0, 8, 4, 4, 0 };
	config.pAllocator = &allocator;
	config.pManifest = &manifest;
	config.packageResolver = { nullptr, smokeResolvePackage };
	config.eventResolver = { nullptr, smokeResolveEvent };
	config.nMaxRegisteredEvents = 1;
	config.nMaxBanks = 1;
	config.nMaxBankEventBindings = 1;
	config.nMaxMixerControls = 1;
	config.nMaxSnapshots = 1;
	config.nNativeMemoryBudgetBytes = 64 * 1024 * 1024;
	const sirFMODRuntimeNativeOps_t nativeOps = sirGetFMODRuntimeNativeOps();
	for ( uint32_t iPass = 0; iPass < 2; ++iPass ) {
		SIRFMODRuntime runtime;
		if ( runtime.initialize( config, nativeOps ) != SIR_AUDIO_OK ) {
			return 2;
		}
		if ( !runtime.isReady() || runtime.update() != 0 ) {
			return 3;
		}
		uint32_t nPosition = 123;
		if ( runtime.getPlayingTimeline( {}, nPosition ) != SIR_AUDIO_INVALID_HANDLE || nPosition != 0 ) {
			return 4;
		}
		if ( runtime.shutdown() != SIR_AUDIO_OK || runtime.isReady() ) {
			return 5;
		}
	}
	manifest.shutdown();
	return 0;
}
