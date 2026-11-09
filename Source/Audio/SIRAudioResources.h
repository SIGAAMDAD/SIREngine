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

#ifndef __SIRENGINE_SIRAUDIORESOURCES_H__
#define __SIRENGINE_SIRAUDIORESOURCES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/SIRAudioTypes.h>
#include <Core/Containers/SIRContainerResult.h>
#include <Resources/SIRResourceTypes.h>

class SIRResourceGraph;
class SIRResourceReadScope;

constexpr uint32_t SIR_AUDIO_MANIFEST_MAGIC = 0x41524953U; // "SIRA" on little-endian hosts.
constexpr uint16_t SIR_AUDIO_MANIFEST_VERSION = 1;
constexpr uint8_t SIR_AUDIO_MANIFEST_LITTLE_ENDIAN = 1;

typedef enum sirAudioProvider_e : uint8_t
{
	SIR_AUDIO_PROVIDER_NONE = 0,
	SIR_AUDIO_PROVIDER_FMOD = 1,
	SIR_AUDIO_PROVIDER_WWISE = 2,
	SIR_AUDIO_PROVIDER_OPENAL = 3
} sirAudioProvider_t;

typedef enum sirAudioBindingMode_e : uint8_t
{
	SIR_AUDIO_BINDING_REJECT = 0,
	SIR_AUDIO_BINDING_NATIVE = 1,
	SIR_AUDIO_BINDING_EMULATED = 2
} sirAudioBindingMode_t;

typedef enum sirAudioManifestResult_e : uint8_t
{
	SIR_AUDIO_MANIFEST_OK = 0,
	SIR_AUDIO_MANIFEST_INVALID_ARGUMENT = 1,
	SIR_AUDIO_MANIFEST_BAD_SCHEMA = 2,
	SIR_AUDIO_MANIFEST_PROVIDER_MISMATCH = 3,
	SIR_AUDIO_MANIFEST_HASH_MISMATCH = 4,
	SIR_AUDIO_MANIFEST_INVALID_RECORD = 5,
	SIR_AUDIO_MANIFEST_DUPLICATE_ID = 6,
	SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY = 7,
	SIR_AUDIO_MANIFEST_MISSING_CAPABILITY = 8,
	SIR_AUDIO_MANIFEST_OUT_OF_MEMORY = 9,
	SIR_AUDIO_MANIFEST_EPOCH_EXHAUSTED = 10,
	SIR_AUDIO_MANIFEST_CYCLIC_DEPENDENCY = 11
} sirAudioManifestResult_t;

// Little-endian cooked v1 header. Tables follow directly in this order:
// packages, events, controls, then uint64_t package-dependency IDs.
// The hash covers every byte after the nContentHash field, including header metadata.
typedef struct sirAudioManifestHeader_s
{
	uint64_t nContentHash;
	uint64_t nRequiredCapabilities;
	uint64_t nBuildFingerprint;
	uint32_t nMagic;
	uint32_t nTotalBytes;
	uint32_t nPackageCount;
	uint32_t nEventCount;
	uint32_t nControlCount;
	uint32_t nDependencyCount;
	uint16_t nVersion;
	uint16_t nHeaderBytes;
	sirAudioProvider_t eProvider;
	uint8_t nEndianness;
	uint16_t nFlags;
	uint64_t nReserved;
} sirAudioManifestHeader_t;

typedef struct sirAudioPackageRecord_s
{
	uint64_t nPackageId;
	uint64_t nProviderPackageKey;
	uint64_t nRequiredCapabilities;
	uint32_t iDependencyStart;
	uint32_t nDependencyCount;
} sirAudioPackageRecord_t;

typedef struct sirAudioEventRecord_s
{
	uint64_t nEventId;
	uint64_t nPackageId;
	uint64_t nProviderEventKey;
	uint64_t nRequiredCapabilities;
} sirAudioEventRecord_t;

typedef struct sirAudioControlRecord_s
{
	uint64_t nControlId;
	uint64_t nProviderControlId;
	uint64_t nRequiredCapabilities;
	float fDefault;
	float fMinimum;
	float fMaximum;
	sirAudioControlScope_t eScope;
	sirAudioControlType_t eType;
	sirAudioBindingMode_t eBindingMode;
	uint8_t nReserved;
} sirAudioControlRecord_t;

typedef struct sirAudioEventHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nEpoch;
} sirAudioEventHandle_t;

typedef struct sirAudioPackageHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nEpoch;
} sirAudioPackageHandle_t;

static_assert( sizeof( sirAudioManifestHeader_t ) == 64, "Audio manifest header ABI changed" );
static_assert( sizeof( sirAudioPackageRecord_t ) == 32, "Audio package record ABI changed" );
static_assert( sizeof( sirAudioEventRecord_t ) == 32, "Audio event record ABI changed" );
static_assert( sizeof( sirAudioControlRecord_t ) == 40, "Audio control record ABI changed" );
static_assert( sizeof( sirAudioEventHandle_t ) == 16, "Audio event handle ABI changed" );
static_assert( sizeof( sirAudioPackageHandle_t ) == 16, "Audio package handle ABI changed" );

typedef struct sirAudioManifestLoadOptions_s
{
	const sirContainerAllocator_t *pAllocator;
	uint64_t nAvailableCapabilities;
	sirAudioProvider_t eExpectedProvider;
	uint32_t nSourceProviderId;
} sirAudioManifestLoadOptions_t;

// The store pins one exact GRDG revision for the lifetime of all its lookups.
// An audio provider pin is independent of an active read scope: the audio
// playback lifetime does not stall reclamation of unrelated resource versions.
// The immutable manifest is never copied into audio-owned source storage.
// Load/reload/shutdown and all lookups are serialized by the audio owner.
class SIRAudioResourceStore
{
private:
	const uint8_t *m_pBytes = nullptr;
	SIRResourceGraph *m_pSourceGraph = nullptr;
	SIRResourcePinHandle m_SourcePin = {};
	SIRResourceHandle m_SourceHandle = {};
	uint64_t m_nSourceRevision = 0;
	const sirContainerAllocator_t *m_pAllocator = nullptr;
	sirAudioManifestHeader_t m_Header = {};
	uint64_t m_nEpoch = 0;
	size_t m_nPackageOffset = 0;
	size_t m_nEventOffset = 0;
	size_t m_nControlOffset = 0;
	size_t m_nDependencyOffset = 0;

	bool findPackageIndex( uint64_t nPackageId, uint32_t& iPackage ) const noexcept;
	bool findEventIndex( uint64_t nEventId, uint32_t& iEvent ) const noexcept;

public:
	SIRAudioResourceStore( void ) noexcept = default;
	~SIRAudioResourceStore( void ) noexcept;
	SIRAudioResourceStore( const SIRAudioResourceStore& ) = delete;
	SIRAudioResourceStore& operator=( const SIRAudioResourceStore& ) = delete;

	static uint64_t hashPayload( const void *pBytes, size_t nBytes ) noexcept;
	sirAudioManifestResult_t load( SIRResourceGraph& rGraph, SIRResourceHandle hManifest, SIRResourceChunkId chunkId,
	                               const sirAudioManifestLoadOptions_t& rOptions ) noexcept;
	const uint8_t *getBorrowedManifestBytes( void ) const noexcept;
	uint64_t getSourceRevision( void ) const noexcept;
	void shutdown( void ) noexcept;
	bool isLoaded( void ) const noexcept;
	uint64_t getEpoch( void ) const noexcept;
	sirAudioProvider_t getProvider( void ) const noexcept;
	uint64_t getBuildFingerprint( void ) const noexcept;
	uint32_t getEventCount( void ) const noexcept;
	bool getEventAt( uint32_t iEvent, sirAudioEventRecord_t& event ) const noexcept;
	bool findEvent( uint64_t nEventId, sirAudioEventRecord_t& event, sirAudioEventHandle_t *pHandle = nullptr ) const noexcept;
	bool resolveEvent( sirAudioEventHandle_t hEvent, sirAudioEventRecord_t& event ) const noexcept;
	uint32_t getPackageCount( void ) const noexcept;
	bool getPackageAt( uint32_t iPackage, sirAudioPackageRecord_t& package ) const noexcept;
	bool getPackageDependency( uint64_t nPackageId, uint32_t iDependency, uint64_t& nDependencyId ) const noexcept;
	bool findPackage( uint64_t nPackageId, sirAudioPackageRecord_t& package, sirAudioPackageHandle_t *pHandle = nullptr ) const noexcept;
	bool resolvePackage( sirAudioPackageHandle_t hPackage, sirAudioPackageRecord_t& package ) const noexcept;
	uint32_t getControlCount( void ) const noexcept;
	bool getControlAt( uint32_t iControl, sirAudioControlRecord_t& control ) const noexcept;
	bool findControl( uint64_t nControlId, sirAudioControlRecord_t& control ) const noexcept;
	sirAudioResult_t validateControl( uint64_t nControlId, sirAudioControlScope_t eScope, float fValue ) const noexcept;
};

#endif // __SIRENGINE_SIRAUDIORESOURCES_H__
