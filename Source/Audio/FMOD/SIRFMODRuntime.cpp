/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Audio/FMOD/SIRFMODRuntime.h>

#include <Core/Containers/SIRRawStorage.h>
#include <stdlib.h>
#include <string.h>

SIRFMODRuntime::~SIRFMODRuntime( void ) noexcept
{
	if ( shutdown() != SIRAudioResult::SIR_AUDIO_OK ) {
		abort();
	}
}

void SIRFMODRuntime::releaseBindingStorage( void ) noexcept
{
	if ( m_pEventStates != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pEventStates, static_cast<size_t>(m_nEventCount), alignof( uint8_t ) );
	}
	if ( m_pObjectStates != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pObjectStates, static_cast<size_t>(m_nObjectCount), alignof( uint8_t ) );
	}
	m_pEventStates = nullptr;
	m_pObjectStates = nullptr;
	m_pAllocator = nullptr;
	m_pManifest = nullptr;
	m_pObjects = nullptr;
	m_EventResolver = {};
	m_ObjectResolver = {};
	m_nEventCount = 0;
	m_nObjectCount = 0;
	m_nManifestEpoch = 0;
	m_bInitialized = false;
	m_NativeMemoryStats = nullptr;
	if ( m_Memory.shutdown() != SIRAudioResult::SIR_AUDIO_OK ) {
		abort();
	}
}

SIRAudioResult SIRFMODRuntime::initialize( const sirFMODRuntimeConfig_t& config, const sirFMODRuntimeNativeOps_t& ops ) noexcept
{
	if ( m_pManifest != nullptr || m_bInitialized || config.pManifest == nullptr || !config.pManifest->isLoaded() ||
	     config.pManifest->getProvider() != SIRAudioProvider::SIR_AUDIO_PROVIDER_FMOD || !SIRRawStorage::isValidAllocator( config.pAllocator ) ||
	     config.packageResolver.resolve == nullptr || config.eventResolver.resolve == nullptr || config.nMaxRegisteredEvents == 0 ||
	     config.nMaxRegisteredParameters < config.pManifest->getControlCount() ||
	     config.nMaxRegisteredEvents < config.pManifest->getEventCount() || config.nMaxBanks == 0 || config.nMaxBankEventBindings == 0 ||
	     config.nMaxMixerControls == 0 || config.nMaxSnapshots == 0 ||
	     ( config.nObjectCount != 0 && ( config.pObjects == nullptr || config.objectResolver.resolve == nullptr ) ) ) {
		return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
	}

	for ( uint32_t iObject = 0; iObject < config.nObjectCount; ++iObject ) {
		const sirFMODRuntimeObject_t& object = config.pObjects[iObject];
		sirAudioPackageRecord_t package {};
		if ( object.nObjectId == 0 || object.nPackageId == 0 || object.nProviderKey == 0 ||
		     !config.pManifest->findPackage( object.nPackageId, package ) ||
		     ( object.eKind != SIRFMODRuntimeObjectKind::SIR_FMOD_RUNTIME_BUS && object.eKind != SIRFMODRuntimeObjectKind::SIR_FMOD_RUNTIME_VCA &&
		       object.eKind != SIRFMODRuntimeObjectKind::SIR_FMOD_RUNTIME_SNAPSHOT ) ) {
			return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iObject; ++iPrevious ) {
			if ( config.pObjects[iPrevious].nObjectId == object.nObjectId ) {
				return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
			}
		}
	}

	if ( config.nNativeMemoryBudgetBytes != 0 ) {
		if ( ops.memory.nAbiVersion != 1 || ops.memory.install == nullptr ) {
			return SIRAudioResult::SIR_AUDIO_UNSUPPORTED;
		}
		const SIRAudioResult eNativeMemory = ops.memory.install( config.pAllocator, config.nNativeMemoryBudgetBytes );
		if ( eNativeMemory != SIRAudioResult::SIR_AUDIO_OK ) {
			return eNativeMemory;
		}
	}
	const SIRAudioResult eMemory = m_Memory.configure( config.pAllocator, config.nEngineMemoryBudgetBytes );
	if ( eMemory != SIRAudioResult::SIR_AUDIO_OK ) {
		return eMemory;
	}
	m_pManifest = config.pManifest;
	m_pAllocator = m_Memory.getAllocator();
	m_NativeMemoryStats = ops.memory.getStats;
	m_pObjects = config.pObjects;
	m_nObjectCount = config.nObjectCount;
	m_nEventCount = config.pManifest->getEventCount();
	m_EventResolver = config.eventResolver;
	m_ObjectResolver = config.objectResolver;
	m_nManifestEpoch = m_pManifest->getEpoch();
	m_nLastNativeResult = 0;

	SIRAudioResult eResult = SIRAudioResult::SIR_AUDIO_OK;
	do {
		if ( m_nEventCount != 0 ) {
			void *pStorage = nullptr;
			if ( SIRRawStorage::tryAllocate( m_pAllocator, m_nEventCount, alignof( uint8_t ), &pStorage ) != SIRContainerResult::SIR_CONTAINER_OK ) {
				eResult = SIRAudioResult::SIR_AUDIO_CAPACITY;
				break;
			}
			m_pEventStates = static_cast<uint8_t *>(pStorage);
			memset( m_pEventStates, 0, m_nEventCount );
		}
		if ( m_nObjectCount != 0 ) {
			void *pStorage = nullptr;
			if ( SIRRawStorage::tryAllocate( m_pAllocator, m_nObjectCount, alignof( uint8_t ), &pStorage ) != SIRContainerResult::SIR_CONTAINER_OK ) {
				eResult = SIRAudioResult::SIR_AUDIO_CAPACITY;
				break;
			}
			m_pObjectStates = static_cast<uint8_t *>(pStorage);
			memset( m_pObjectStates, 0, m_nObjectCount );
		}
		if ( m_Context.initialize( config.context, ops.context ) != SIRFMODContextResult::SIR_FMOD_OK ) {
			eResult = SIRAudioResult::SIR_AUDIO_BACKEND_ERROR;
			break;
		}
		eResult = m_Mixer.configure( &m_Context, m_pAllocator, ops.mixer, config.nMaxMixerControls );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Mixer.initialize();
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Snapshots.configure( &m_Context, m_pAllocator, ops.snapshot, config.nMaxSnapshots );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Snapshots.initialize();
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Voices.configure( &m_Context, m_pAllocator, ops.voice, config.nMaxRegisteredEvents, config.nMaxRegisteredParameters,
		                              config.audio.nMaxEmitters, config.audio.nMaxListeners, config.nMaxEmitterParameters );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Voices.bindControllers( &m_Mixer, &m_Snapshots );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Audio.initialize( config.audio, m_pAllocator, SIRFMODVoiceBackend::getBackendOps(), &m_Voices, m_pManifest );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Banks.initialize( &m_Context, &m_Voices, m_pAllocator, ops.bank, config.nMaxBanks, config.nMaxBankEventBindings );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = m_Packages.initialize( m_pManifest, &m_Banks, m_pAllocator, config.packageResolver );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		eResult = registerNativeParameters();
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			break;
		}
		m_bInitialized = true;
	} while ( false );

	if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
		const SIRAudioResult eRollback = shutdown();
		return eRollback == SIRAudioResult::SIR_AUDIO_OK ? eResult : eRollback;
	}
	return SIRAudioResult::SIR_AUDIO_OK;
}

SIRAudioResult SIRFMODRuntime::registerNativeParameters( void ) noexcept
{
	for ( uint32_t iControl = 0; iControl < m_pManifest->getControlCount(); ++iControl ) {
		sirAudioControlRecord_t control {};
		if ( !m_pManifest->getControlAt( iControl, control ) ) {
			return SIRAudioResult::SIR_AUDIO_INVALID_HANDLE;
		}
		if ( control.eBindingMode != SIRAudioBindingMode::SIR_AUDIO_BINDING_NATIVE ) {
			continue;
		}
		const SIRAudioResult eResult = m_Voices.registerParameter( control.nControlId, control.nProviderControlId, control.eScope );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			return eResult;
		}
	}
	return SIRAudioResult::SIR_AUDIO_OK;
}

SIRAudioResult SIRFMODRuntime::bindPackageResources( void ) noexcept
{
	for ( uint32_t iEvent = 0; iEvent < m_nEventCount; ++iEvent ) {
		if ( m_pEventStates[iEvent] != 0 ) {
			continue;
		}
		sirAudioEventRecord_t event {};
		if ( !m_pManifest->getEventAt( iEvent, event ) ) {
			return SIRAudioResult::SIR_AUDIO_INVALID_HANDLE;
		}
		if ( !m_Banks.hasLoadedBank( event.nPackageId ) ) {
			continue;
		}
		const char *pszPath = nullptr;
		const SIRAudioResult ePath =
		    m_EventResolver.resolve( m_EventResolver.pUserData, event.nEventId, event.nProviderEventKey, &pszPath );
		if ( ePath != SIRAudioResult::SIR_AUDIO_OK ) {
			return ePath;
		}
		if ( pszPath == nullptr || pszPath[0] == '\0' ) {
			return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
		}
		const SIRAudioResult eBind = m_Banks.bindEvent( event.nPackageId, event.nEventId, pszPath );
		if ( eBind != SIRAudioResult::SIR_AUDIO_OK ) {
			return eBind;
		}
		m_pEventStates[iEvent] = 2;
	}

	for ( uint32_t iObject = 0; iObject < m_nObjectCount; ++iObject ) {
		if ( m_pObjectStates[iObject] != 0 ) {
			continue;
		}
		const sirFMODRuntimeObject_t& object = m_pObjects[iObject];
		if ( !m_Banks.hasLoadedBank( object.nPackageId ) ) {
			continue;
		}
		const char *pszPath = nullptr;
		const SIRAudioResult ePath =
		    m_ObjectResolver.resolve( m_ObjectResolver.pUserData, object.eKind, object.nObjectId, object.nProviderKey, &pszPath );
		if ( ePath != SIRAudioResult::SIR_AUDIO_OK ) {
			return ePath;
		}
		if ( pszPath == nullptr || pszPath[0] == '\0' ) {
			return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
		}
		SIRAudioResult eBind = SIRAudioResult::SIR_AUDIO_UNSUPPORTED;
		if ( object.eKind == SIRFMODRuntimeObjectKind::SIR_FMOD_RUNTIME_BUS ) {
			eBind = m_Mixer.registerBusFromBank( object.nObjectId, object.nPackageId, &m_Banks, pszPath );
		} else if ( object.eKind == SIRFMODRuntimeObjectKind::SIR_FMOD_RUNTIME_VCA ) {
			eBind = m_Mixer.registerVCAFromBank( object.nObjectId, object.nPackageId, &m_Banks, pszPath );
		} else {
			eBind = m_Snapshots.registerSnapshotFromBank( object.nObjectId, object.nPackageId, &m_Banks, pszPath );
		}
		if ( eBind != SIRAudioResult::SIR_AUDIO_OK ) {
			return eBind;
		}
		m_pObjectStates[iObject] = 2;
	}
	return SIRAudioResult::SIR_AUDIO_OK;
}

void SIRFMODRuntime::rollbackNewBindings( void ) noexcept
{
	for ( uint32_t iObject = m_nObjectCount; iObject != 0; ) {
		--iObject;
		if ( m_pObjectStates[iObject] != 2 ) {
			continue;
		}
		const sirFMODRuntimeObject_t& object = m_pObjects[iObject];
		const SIRAudioResult eResult = object.eKind == SIRFMODRuntimeObjectKind::SIR_FMOD_RUNTIME_SNAPSHOT ? m_Snapshots.unregisterSnapshot( object.nObjectId )
		                                                                           : m_Mixer.unregisterControl( object.nObjectId );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			abort();
		}
		m_pObjectStates[iObject] = 0;
	}
	for ( uint32_t iEvent = m_nEventCount; iEvent != 0; ) {
		--iEvent;
		if ( m_pEventStates[iEvent] != 2 ) {
			continue;
		}
		sirAudioEventRecord_t event {};
		if ( !m_pManifest->getEventAt( iEvent, event ) || m_Banks.unbindEvent( event.nPackageId, event.nEventId ) != SIRAudioResult::SIR_AUDIO_OK ) {
			abort();
		}
		m_pEventStates[iEvent] = 0;
	}
}

void SIRFMODRuntime::commitNewBindings( void ) noexcept
{
	for ( uint32_t iEvent = 0; iEvent < m_nEventCount; ++iEvent ) {
		if ( m_pEventStates[iEvent] == 2 ) {
			m_pEventStates[iEvent] = 1;
		}
	}
	for ( uint32_t iObject = 0; iObject < m_nObjectCount; ++iObject ) {
		if ( m_pObjectStates[iObject] == 2 ) {
			m_pObjectStates[iObject] = 1;
		}
	}
}

void SIRFMODRuntime::synchronizeReleasedBindings( void ) noexcept
{
	for ( uint32_t iEvent = 0; iEvent < m_nEventCount; ++iEvent ) {
		sirAudioEventRecord_t event {};
		if ( m_pEventStates[iEvent] != 0 && m_pManifest->getEventAt( iEvent, event ) && !m_Banks.hasLoadedBank( event.nPackageId ) ) {
			m_pEventStates[iEvent] = 0;
		}
	}
}

SIRAudioResult SIRFMODRuntime::loadPackage( uint64_t nPackageId, bool bPreloadSamples ) noexcept
{
	if ( !isReady() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	SIRAudioResult eResult = m_Packages.loadPackage( nPackageId, bPreloadSamples );
	if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
		return eResult;
	}
	eResult = bindPackageResources();
	if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
		rollbackNewBindings();
		const SIRAudioResult eRelease = m_Packages.releasePackage( nPackageId );
		synchronizeReleasedBindings();
		return eRelease == SIRAudioResult::SIR_AUDIO_OK ? eResult : eRelease;
	}
	commitNewBindings();
	return SIRAudioResult::SIR_AUDIO_OK;
}

SIRAudioResult SIRFMODRuntime::beginLoadPackage( uint64_t nPackageId, bool bPreloadSamples ) noexcept
{
	return isReady() ? m_Packages.beginLoadPackage( nPackageId, bPreloadSamples ) : SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
}

SIRAudioResult SIRFMODRuntime::pollLoadPackage( void ) noexcept
{
	if ( !isReady() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	const uint64_t nPackageId = m_Packages.getPendingPackageId();
	const SIRAudioResult eResult = m_Packages.pollLoadPackage();
	if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
		return eResult;
	}
	const SIRAudioResult eBind = bindPackageResources();
	if ( eBind != SIRAudioResult::SIR_AUDIO_OK ) {
		rollbackNewBindings();
		const SIRAudioResult eRelease = m_Packages.releasePackage( nPackageId );
		synchronizeReleasedBindings();
		return eRelease == SIRAudioResult::SIR_AUDIO_OK ? eBind : eRelease;
	}
	commitNewBindings();
	return SIRAudioResult::SIR_AUDIO_OK;
}

SIRAudioResult SIRFMODRuntime::cancelLoadPackage( void ) noexcept
{
	return isReady() ? m_Packages.cancelLoadPackage() : SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
}

uint64_t SIRFMODRuntime::getPendingPackageId( void ) const noexcept
{
	return isReady() ? m_Packages.getPendingPackageId() : 0;
}

SIRAudioResult SIRFMODRuntime::getSampleState( uint64_t nPackageId, SIRFMODSampleState& eState ) noexcept
{
	return isReady() ? m_Banks.getSampleState( nPackageId, eState ) : SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
}

SIRAudioResult SIRFMODRuntime::recoverOutput( int32_t iPreferredDriver, bool bFallbackToFirst ) noexcept
{
	if ( !isReady() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	const SIRFMODContextResult eResult = m_Context.recoverOutput( iPreferredDriver, bFallbackToFirst );
	if ( eResult == SIRFMODContextResult::SIR_FMOD_OK ) {
		return SIRAudioResult::SIR_AUDIO_OK;
	}
	return eResult == SIRFMODContextResult::SIR_FMOD_INVALID_DRIVER ? SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT : SIRAudioResult::SIR_AUDIO_BACKEND_ERROR;
}

SIRAudioResult SIRFMODRuntime::collectUnused( void ) noexcept
{
	if ( !isReady() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	// Drop only bindings of packages whose final residency lease was released.
	// Live snapshot instances retain their registrations and block retirement.
	for ( uint32_t iObject = 0; iObject < m_nObjectCount; ++iObject ) {
		if ( m_pObjectStates[iObject] == 0 ) {
			continue;
		}
		const sirFMODRuntimeObject_t& object = m_pObjects[iObject];
		uint32_t nReferences = 0;
		uint32_t nRoots = 0;
		if ( !m_Packages.getPackageReferences( object.nPackageId, nReferences, nRoots ) ) {
			return SIRAudioResult::SIR_AUDIO_INVALID_HANDLE;
		}
		if ( nReferences != 0 ) {
			continue;
		}
		const SIRAudioResult eResult = object.eKind == SIRFMODRuntimeObjectKind::SIR_FMOD_RUNTIME_SNAPSHOT ? m_Snapshots.unregisterSnapshot( object.nObjectId )
		                                                                           : m_Mixer.unregisterControl( object.nObjectId );
		if ( eResult != SIRAudioResult::SIR_AUDIO_OK ) {
			return eResult;
		}
		m_pObjectStates[iObject] = 0;
	}
	const SIRAudioResult eResult = m_Packages.collectUnused();
	synchronizeReleasedBindings();
	return eResult;
}

SIRAudioResult SIRFMODRuntime::releasePackage( uint64_t nPackageId ) noexcept
{
	if ( !isReady() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	const SIRAudioResult eResult = m_Packages.releasePackage( nPackageId );
	if ( eResult != SIRAudioResult::SIR_AUDIO_OK && eResult != SIRAudioResult::SIR_AUDIO_BUSY ) {
		return eResult;
	}
	const SIRAudioResult eCollect = collectUnused();
	return eCollect == SIRAudioResult::SIR_AUDIO_OK ? SIRAudioResult::SIR_AUDIO_OK : eCollect;
}

SIRAudioResult SIRFMODRuntime::shutdown( void ) noexcept
{
	m_bInitialized = false;
	// All producer threads must already have stopped submitting commands.
	m_Audio.shutdown();
	const SIRAudioResult eSnapshots = m_Snapshots.shutdown();
	if ( eSnapshots != SIRAudioResult::SIR_AUDIO_OK ) {
		return eSnapshots;
	}
	m_Mixer.shutdown();
	const SIRAudioResult ePackages = m_Packages.shutdown();
	if ( ePackages != SIRAudioResult::SIR_AUDIO_OK ) {
		return ePackages;
	}
	const SIRAudioResult eBanks = m_Banks.shutdown();
	if ( eBanks != SIRAudioResult::SIR_AUDIO_OK ) {
		return eBanks;
	}
	if ( m_Context.shutdown() != SIRFMODContextResult::SIR_FMOD_OK ) {
		return SIRAudioResult::SIR_AUDIO_BACKEND_ERROR;
	}
	releaseBindingStorage();
	return SIRAudioResult::SIR_AUDIO_OK;
}

uint32_t SIRFMODRuntime::update( void ) noexcept
{
	return isReady() ? m_Audio.update() : 0;
}

SIRAudioResult SIRFMODRuntime::getPlayingTimeline( sirAudioPlayingHandle_t hPlaying, uint32_t& nMilliseconds ) noexcept
{
	nMilliseconds = 0;
	return isReady() ? m_Voices.getPlayingTimeline( hPlaying, nMilliseconds ) : SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
}

bool SIRFMODRuntime::isReady( void ) const noexcept
{
	return m_bInitialized && m_Context.isReady() && m_pManifest != nullptr && m_pManifest->isLoaded() &&
	       m_pManifest->getEpoch() == m_nManifestEpoch && m_Packages.isReady();
}

SIRAudioSystem& SIRFMODRuntime::getAudio( void ) noexcept
{
	return m_Audio;
}
SIRFMODContext& SIRFMODRuntime::getContext( void ) noexcept
{
	return m_Context;
}
SIRFMODMixerBackend& SIRFMODRuntime::getMixer( void ) noexcept
{
	return m_Mixer;
}
SIRFMODSnapshotBackend& SIRFMODRuntime::getSnapshots( void ) noexcept
{
	return m_Snapshots;
}
SIRFMODBankStore& SIRFMODRuntime::getBanks( void ) noexcept
{
	return m_Banks;
}
SIRFMODPackageLoader& SIRFMODRuntime::getPackages( void ) noexcept
{
	return m_Packages;
}
sirFMODMemoryBudgetStats_t SIRFMODRuntime::getMemoryStats( void ) const noexcept
{
	return m_Memory.getStats();
}

sirFMODMemoryBudgetStats_t SIRFMODRuntime::getNativeMemoryStats( void ) const noexcept
{
	return m_NativeMemoryStats != nullptr ? m_NativeMemoryStats() : sirFMODMemoryBudgetStats_t{};
}

int32_t SIRFMODRuntime::getLastNativeResult( void ) const noexcept
{
	return m_nLastNativeResult;
}
