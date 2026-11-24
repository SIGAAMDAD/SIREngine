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

#include <Audio/FMOD/SIRFMODAudioThread.h>
#include <atomic>
#include <Core/Containers/SIRRawStorage.h>
#include <chrono>
#include <new>
#include <stdlib.h>
#include <thread>

static constexpr uint32_t SIR_FMOD_THREAD_EMPTY = 0;
static constexpr uint32_t SIR_FMOD_THREAD_STARTING = 1;
static constexpr uint32_t SIR_FMOD_THREAD_READY = 2;
static constexpr uint32_t SIR_FMOD_THREAD_FAILED = 3;
static constexpr uint32_t SIR_FMOD_SLOT_FREE = 0;
static constexpr uint32_t SIR_FMOD_SLOT_RESERVED = 1;
static constexpr uint32_t SIR_FMOD_SLOT_QUEUED = 2;
static constexpr uint32_t SIR_FMOD_SLOT_EXECUTING = 3;
static constexpr uint32_t SIR_FMOD_SLOT_DONE = 4;
static constexpr uint32_t SIR_FMOD_SLOT_CONSUMING = 5;
static std::atomic<uint64_t> s_nNextFMODThreadTicket( 1 );

SIRFMODAudioThread::~SIRFMODAudioThread( void ) noexcept
{
	if ( shutdown() != SIRAudioResult::SIR_AUDIO_OK ) {
		abort();
	}
}

bool SIRFMODAudioThread::enterClient( void ) noexcept
{
	if ( m_bAccepting.load( std::memory_order_acquire ) == 0 ) {
		return false;
	}
	m_nActiveClients.fetch_add( 1, std::memory_order_acq_rel );
	if ( m_bAccepting.load( std::memory_order_acquire ) == 0 ) {
		m_nActiveClients.fetch_sub( 1, std::memory_order_release );
		return false;
	}
	return true;
}

void SIRFMODAudioThread::leaveClient( void ) noexcept
{
	m_nActiveClients.fetch_sub( 1, std::memory_order_release );
}

void SIRFMODAudioThread::releaseStorage( void ) noexcept
{
	m_Requests.shutdown();
	m_Critical.shutdown();
	m_Lifecycle.shutdown();
	if ( m_pSlots != nullptr ) {
		for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
			m_pSlots[iSlot].~sirFMODThreadSlot_t();
		}
		SIRRawStorage::deallocate( m_pAllocator, m_pSlots, static_cast<size_t>(m_nCapacity * sizeof(  sirFMODThreadSlot_t  )),
		                           alignof( sirFMODThreadSlot_t ) );
	}
	m_pSlots = nullptr;
	m_pAllocator = nullptr;
	m_nCapacity = 0;
	m_Config = {};
	m_nState.store( SIR_FMOD_THREAD_EMPTY, std::memory_order_release );
}

SIRAudioResult SIRFMODAudioThread::start( const sirFMODAudioThreadConfig_t& config ) noexcept
{
	if ( m_nState.load( std::memory_order_acquire ) != SIR_FMOD_THREAD_EMPTY || m_Thread.isJoinable() ||
	     !SIRRawStorage::isValidAllocator( config.runtime.pAllocator ) || config.nRequestCapacity < 4 || config.nRequestCapacity > 4096 ||
	     ( config.nRequestCapacity & ( config.nRequestCapacity - 1 ) ) != 0 || config.nIdleSleepMilliseconds > 100 ) {
		return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
	}
	m_Config = config;
	if ( m_Config.nIdleSleepMilliseconds == 0 ) {
		m_Config.nIdleSleepMilliseconds = 1;
	}
	m_pAllocator = config.runtime.pAllocator;
	m_nCapacity = config.nRequestCapacity;
	size_t nBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( m_nCapacity, sizeof( sirFMODThreadSlot_t ), &nBytes ) ) {
		releaseStorage();
		return SIRAudioResult::SIR_AUDIO_CAPACITY;
	}
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( m_pAllocator, nBytes, alignof( sirFMODThreadSlot_t ), &pMemory ) != SIRContainerResult::SIR_CONTAINER_OK ) {
		releaseStorage();
		return SIRAudioResult::SIR_AUDIO_CAPACITY;
	}
	m_pSlots = static_cast<sirFMODThreadSlot_t *>(pMemory);
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		new ( m_pSlots + iSlot ) sirFMODThreadSlot_t();
		m_pSlots[iSlot].nGeneration = 0;
		m_pSlots[iSlot].nState.store( SIR_FMOD_SLOT_FREE, std::memory_order_relaxed );
	}
	if ( !m_Requests.initialize( m_pAllocator, m_nCapacity ) ||
	     !m_Critical.initialize( m_pAllocator, m_nCapacity < 8 ? 2 : m_nCapacity >> 2 ) ||
	     !m_Lifecycle.initialize( m_pAllocator, m_nCapacity ) ) {
		releaseStorage();
		return SIRAudioResult::SIR_AUDIO_CAPACITY;
	}
	m_nActiveClients.store( 0, std::memory_order_relaxed );
	m_nNextSlot.store( 0, std::memory_order_relaxed );
	m_nWakes.store( 0, std::memory_order_relaxed );
	m_nDroppedLifecycle.store( 0, std::memory_order_relaxed );
	m_nAutoRecoveries.store( 0, std::memory_order_relaxed );
	m_nAutoRecoveryFailures.store( 0, std::memory_order_relaxed );
	m_nWorkerResult.store( SIRAudioResult::SIR_AUDIO_OK, std::memory_order_relaxed );
	m_bStop.store( 0, std::memory_order_relaxed );
	m_bAccepting.store( 0, std::memory_order_relaxed );
	m_nState.store( SIR_FMOD_THREAD_STARTING, std::memory_order_release );
	if ( !m_Thread.start( threadCallback, this ) ) {
		releaseStorage();
		return SIRAudioResult::SIR_AUDIO_BACKEND_ERROR;
	}
	while ( m_nState.load( std::memory_order_acquire ) == SIR_FMOD_THREAD_STARTING ) {
		std::this_thread::yield();
	}
	if ( m_nState.load( std::memory_order_acquire ) != SIR_FMOD_THREAD_READY ) {
		const SIRAudioResult eResult = static_cast<SIRAudioResult>(m_nWorkerResult.load( std::memory_order_acquire ));
		if ( !m_Thread.join() ) {
			return SIRAudioResult::SIR_AUDIO_BUSY;
		}
		releaseStorage();
		return eResult;
	}
	m_bAccepting.store( 1, std::memory_order_release );
	return SIRAudioResult::SIR_AUDIO_OK;
}

SIRAudioResult SIRFMODAudioThread::shutdown( void ) noexcept
{
	if ( m_nState.load( std::memory_order_acquire ) == SIR_FMOD_THREAD_EMPTY ) {
		return SIRAudioResult::SIR_AUDIO_OK;
	}
	m_bAccepting.store( 0, std::memory_order_release );
	while ( m_nActiveClients.load( std::memory_order_acquire ) != 0 ) {
		std::this_thread::yield();
	}
	m_bStop.store( 1, std::memory_order_release );
	m_nWakes.fetch_add( 1, std::memory_order_release );
	m_WaitCondition.notify_one();
	if ( !m_Thread.join() ) {
		return SIRAudioResult::SIR_AUDIO_BUSY;
	}
	const SIRAudioResult eResult = static_cast<SIRAudioResult>(m_nWorkerResult.load( std::memory_order_acquire ));
	if ( eResult == SIRAudioResult::SIR_AUDIO_OK ) {
		releaseStorage();
	}
	return eResult;
}

SIRAudioResult SIRFMODAudioThread::submit( const sirFMODThreadRequest_t& request, uint64_t& nTicket ) noexcept
{
	nTicket = 0;
	if ( !enterClient() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	if ( request.eOperation < SIRFMODThreadOperation::SIR_FMOD_THREAD_LOAD_PACKAGE || request.eOperation > SIRFMODThreadOperation::SIR_FMOD_THREAD_QUERY_SAMPLE ) {
		leaveClient();
		return SIRAudioResult::SIR_AUDIO_INVALID_ARGUMENT;
	}
	const uint32_t iFirstSlot = m_nNextSlot.fetch_add( 1, std::memory_order_relaxed ) & ( m_nCapacity - 1 );
	for ( uint32_t nAttempt = 0; nAttempt < m_nCapacity; ++nAttempt ) {
		const uint32_t iSlot = ( iFirstSlot + nAttempt ) & ( m_nCapacity - 1 );
		sirFMODThreadSlot_t& slot = m_pSlots[iSlot];
		uint32_t nExpected = SIR_FMOD_SLOT_FREE;
		if ( !slot.nState.compare_exchange_strong( nExpected, SIR_FMOD_SLOT_RESERVED, std::memory_order_acq_rel ) ) {
			continue;
		}
		const uint64_t nGeneration = s_nNextFMODThreadTicket.fetch_add( 1, std::memory_order_relaxed );
		const uint64_t nNextTicket = ( nGeneration << 16 ) | iSlot;
		slot.nGeneration = static_cast<uint32_t>(nGeneration);
		slot.nTicket.store( nNextTicket, std::memory_order_relaxed );
		slot.nCancelTicket.store( 0, std::memory_order_relaxed );
		slot.nOperation.store( static_cast<uint8_t>(request.eOperation), std::memory_order_relaxed );
		slot.request = request;
		slot.result = {};
		slot.nState.store( SIR_FMOD_SLOT_QUEUED, std::memory_order_release );
		// Preserve PLAY -> STOP FIFO ordering within the normal request lane.
		const bool bCritical = request.eOperation == SIRFMODThreadOperation::SIR_FMOD_THREAD_RECOVER_OUTPUT;
		if ( !( bCritical ? m_Critical.tryPush( nNextTicket ) : m_Requests.tryPush( nNextTicket ) ) ) {
			slot.nState.store( SIR_FMOD_SLOT_FREE, std::memory_order_release );
			leaveClient();
			return SIRAudioResult::SIR_AUDIO_CAPACITY;
		}
		nTicket = nNextTicket;
		m_nWakes.fetch_add( 1, std::memory_order_release );
		m_WaitCondition.notify_one();
		leaveClient();
		return SIRAudioResult::SIR_AUDIO_PENDING;
	}
	leaveClient();
	return SIRAudioResult::SIR_AUDIO_CAPACITY;
}

SIRAudioResult SIRFMODAudioThread::cancel( uint64_t nTicket ) noexcept
{
	if ( !enterClient() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	const uint32_t iSlot = static_cast<uint32_t>( nTicket & 0xFFFFU );
	SIRAudioResult eResult = SIRAudioResult::SIR_AUDIO_INVALID_HANDLE;
	if ( nTicket != 0 && iSlot < m_nCapacity ) {
		sirFMODThreadSlot_t& slot = m_pSlots[iSlot];
		const uint32_t nState = slot.nState.load( std::memory_order_acquire );
		if ( slot.nTicket.load( std::memory_order_acquire ) == nTicket &&
		     ( nState == SIR_FMOD_SLOT_QUEUED || nState == SIR_FMOD_SLOT_EXECUTING ) ) {
			// Only a running asynchronous bank transaction can still be
			// cancelled after dispatch. Other native calls are noninterruptible.
			if ( nState == SIR_FMOD_SLOT_EXECUTING && slot.nOperation.load( std::memory_order_acquire ) != SIRFMODThreadOperation::SIR_FMOD_THREAD_LOAD_PACKAGE ) {
				eResult = SIRAudioResult::SIR_AUDIO_BUSY;
			} else {
				slot.nCancelTicket.store( nTicket, std::memory_order_release );
				eResult = SIRAudioResult::SIR_AUDIO_OK;
				m_nWakes.fetch_add( 1, std::memory_order_release );
				m_WaitCondition.notify_one();
			}
		}
	}
	leaveClient();
	return eResult;
}

SIRAudioResult SIRFMODAudioThread::poll( uint64_t nTicket, sirFMODThreadResult_t& result ) noexcept
{
	result = {};
	if ( !enterClient() ) {
		return SIRAudioResult::SIR_AUDIO_NOT_INITIALIZED;
	}
	const uint32_t iSlot = static_cast<uint32_t>( nTicket & 0xFFFFU );
	SIRAudioResult eResult = SIRAudioResult::SIR_AUDIO_INVALID_HANDLE;
	if ( nTicket != 0 && iSlot < m_nCapacity ) {
		sirFMODThreadSlot_t& slot = m_pSlots[iSlot];
		if ( slot.nTicket.load( std::memory_order_acquire ) == nTicket ) {
			uint32_t nExpected = SIR_FMOD_SLOT_DONE;
			if ( slot.nState.compare_exchange_strong( nExpected, SIR_FMOD_SLOT_CONSUMING, std::memory_order_acq_rel ) ) {
				result = slot.result;
				slot.nState.store( SIR_FMOD_SLOT_FREE, std::memory_order_release );
				eResult = SIRAudioResult::SIR_AUDIO_OK;
			} else if ( nExpected == SIR_FMOD_SLOT_QUEUED || nExpected == SIR_FMOD_SLOT_EXECUTING || nExpected == SIR_FMOD_SLOT_RESERVED ) {
				eResult = SIRAudioResult::SIR_AUDIO_PENDING;
			}
		}
	}
	leaveClient();
	return eResult;
}

bool SIRFMODAudioThread::pollLifecycle( sirAudioCompletion_t& completion ) noexcept
{
	if ( !enterClient() ) {
		return false;
	}
	const bool bReady = m_Lifecycle.tryPop( completion );
	leaveClient();
	return bReady;
}

bool SIRFMODAudioThread::isReady( void ) const noexcept
{
	return m_nState.load( std::memory_order_acquire ) == SIR_FMOD_THREAD_READY;
}

uint32_t SIRFMODAudioThread::getDroppedLifecycleCount( void ) const noexcept
{
	return m_nDroppedLifecycle.load( std::memory_order_acquire );
}

uint32_t SIRFMODAudioThread::getAutoRecoveryCount( void ) const noexcept
{
	return m_nAutoRecoveries.load( std::memory_order_acquire );
}

uint32_t SIRFMODAudioThread::getAutoRecoveryFailureCount( void ) const noexcept
{
	return m_nAutoRecoveryFailures.load( std::memory_order_acquire );
}

sirFMODMemoryBudgetStats_t SIRFMODAudioThread::getEngineMemoryStats( void ) noexcept
{
	if ( !enterClient() ) {
		return {};
	}
	const sirFMODMemoryBudgetStats_t stats = m_Runtime.getMemoryStats();
	leaveClient();
	return stats;
}

sirFMODMemoryBudgetStats_t SIRFMODAudioThread::getNativeMemoryStats( void ) noexcept
{
	if ( !enterClient() ) {
		return {};
	}
	const sirFMODMemoryBudgetStats_t stats = m_Runtime.getNativeMemoryStats();
	leaveClient();
	return stats;
}

void SIRFMODAudioThread::lifecycleCallback( void *pContext, const sirAudioCompletion_t *pCompletion ) noexcept
{
	SIRFMODAudioThread *pThread = static_cast<SIRFMODAudioThread *>(pContext);
	if ( !pThread->m_Lifecycle.tryPush( *pCompletion ) ) {
		pThread->m_nDroppedLifecycle.fetch_add( 1, std::memory_order_relaxed );
	}
}

void SIRFMODAudioThread::complete( uint64_t nTicket, const sirFMODThreadResult_t& result ) noexcept
{
	const uint32_t iSlot = static_cast<uint32_t>( nTicket & 0xFFFFU );
	sirFMODThreadSlot_t& slot = m_pSlots[iSlot];
	if ( slot.nTicket.load( std::memory_order_relaxed ) != nTicket ) {
		abort();
	}
	slot.result = result;
	slot.result.nTicket = nTicket;
	slot.nState.store( SIR_FMOD_SLOT_DONE, std::memory_order_release );
}

SIRAudioResult SIRFMODAudioThread::execute( const sirFMODThreadRequest_t& request, sirFMODThreadResult_t& result ) noexcept
{
	SIRAudioSystem& audio = m_Runtime.getAudio();
	switch ( request.eOperation ) {
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_LOAD_PACKAGE:
		return m_Runtime.beginLoadPackage( request.nId, request.bFlag != 0 );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_RELEASE_PACKAGE:
		return m_Runtime.releasePackage( request.nId );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_COLLECT_UNUSED:
		return m_Runtime.collectUnused();
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_RECOVER_OUTPUT:
		return m_Runtime.recoverOutput( request.iDriver, request.bFlag != 0 );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_PLAY_EVENT:
		return audio.playEvent( request.nId, request.hEmitter, result.hPlaying );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_STOP_EVENT:
		return audio.stopEvent( request.hPlaying, request.eStopMode );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_PARAMETER:
		return audio.setParameter( request.hPlaying, request.nArgumentId, request.fValue );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_VOLUME:
		return audio.setPlayingVolume( request.hPlaying, request.fValue );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_PITCH:
		return audio.setPlayingPitch( request.hPlaying, request.fValue );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_PAUSED:
		return audio.setPlayingPaused( request.hPlaying, request.bFlag != 0 );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SEEK_TIMELINE:
		return audio.seekPlayingTimeline( request.hPlaying, request.nTimeMilliseconds );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_GLOBAL_PARAMETER:
		return audio.setGlobalParameter( request.nArgumentId, request.fValue );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_BUS_VOLUME:
		return audio.setBusVolume( request.nId, request.fValue );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_BUS_MUTE:
		return audio.setBusMute( request.nId, request.bFlag != 0 );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_SET_VCA_VOLUME:
		return audio.setVCAVolume( request.nId, request.fValue );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_START_SNAPSHOT:
		return audio.startSnapshot( request.nId );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_STOP_SNAPSHOT:
		return audio.stopSnapshot( request.nId, request.eStopMode );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_QUERY_PLAYBACK:
		return audio.getPlaybackState( request.hPlaying, result.ePlaybackState );
	case SIRFMODThreadOperation::SIR_FMOD_THREAD_QUERY_SAMPLE:
		return m_Runtime.getSampleState( request.nId, result.eSampleState );
	default:
		return SIRAudioResult::SIR_AUDIO_UNSUPPORTED;
	}
}

void SIRFMODAudioThread::threadCallback( void *pContext )
{
	static_cast<SIRFMODAudioThread *>(pContext)->threadMain();
}

void SIRFMODAudioThread::threadMain( void ) noexcept
{
	const SIRAudioResult eInit = m_Runtime.initialize( m_Config.runtime, m_Config.nativeOps );
	if ( eInit != SIRAudioResult::SIR_AUDIO_OK ) {
		m_nWorkerResult.store( eInit, std::memory_order_release );
		m_nState.store( SIR_FMOD_THREAD_FAILED, std::memory_order_release );
		return;
	}
	m_Runtime.getAudio().setLifecycleCallback( { this, lifecycleCallback } );
	m_nState.store( SIR_FMOD_THREAD_READY, std::memory_order_release );
	uint64_t nActiveLoadTicket = 0;
	uint32_t nRecoveryBackoff = 0;
	while ( m_bStop.load( std::memory_order_acquire ) == 0 ) {
		m_Runtime.update();
		if ( m_Runtime.getContext().needsDeviceRecovery() ) {
			if ( nRecoveryBackoff == 0 ) {
				const SIRAudioResult eRecovery = m_Runtime.recoverOutput( m_Config.runtime.context.iOutputDriver, true );
				if ( eRecovery == SIRAudioResult::SIR_AUDIO_OK ) {
					m_nAutoRecoveries.fetch_add( 1, std::memory_order_relaxed );
				} else {
					m_nAutoRecoveryFailures.fetch_add( 1, std::memory_order_relaxed );
					nRecoveryBackoff = 10;
				}
			} else {
				--nRecoveryBackoff;
			}
		} else {
			nRecoveryBackoff = 0;
		}
		if ( nActiveLoadTicket != 0 ) {
			sirFMODThreadSlot_t& slot = m_pSlots[static_cast<uint32_t>( nActiveLoadTicket & 0xFFFFU )];
			const bool bCancelled = slot.nCancelTicket.load( std::memory_order_acquire ) == nActiveLoadTicket;
			const SIRAudioResult eProgress = bCancelled ? m_Runtime.cancelLoadPackage() : m_Runtime.pollLoadPackage();
			if ( m_Runtime.getPendingPackageId() == 0 ) {
				sirFMODThreadResult_t result {};
				result.eResult = bCancelled && eProgress == SIRAudioResult::SIR_AUDIO_CANCELLED ? SIRAudioResult::SIR_AUDIO_CANCELLED : eProgress;
				complete( nActiveLoadTicket, result );
				nActiveLoadTicket = 0;
			}
		}
		for ( uint32_t iBatch = 0; iBatch < 32; ++iBatch ) {
			uint64_t nTicket = 0;
			if ( !m_Critical.tryPop( nTicket ) && !m_Requests.tryPop( nTicket ) ) {
				break;
			}
			sirFMODThreadSlot_t& slot = m_pSlots[static_cast<uint32_t>( nTicket & 0xFFFFU )];
			if ( slot.nTicket.load( std::memory_order_acquire ) != nTicket ) {
				abort();
			}
			slot.nState.store( SIR_FMOD_SLOT_EXECUTING, std::memory_order_release );
			sirFMODThreadResult_t result {};
			if ( slot.nCancelTicket.load( std::memory_order_acquire ) == nTicket ) {
				result.eResult = SIRAudioResult::SIR_AUDIO_CANCELLED;
			} else if ( nActiveLoadTicket != 0 && slot.request.eOperation == SIRFMODThreadOperation::SIR_FMOD_THREAD_LOAD_PACKAGE ) {
				// A pending root load does not block playback, mixer, or
				// query traffic. A second root load fails without replacing
				// the first transaction's active completion ticket.
				result.eResult = SIRAudioResult::SIR_AUDIO_BUSY;
			} else {
				result.eResult = execute( slot.request, result );
			}
			if ( slot.request.eOperation == SIRFMODThreadOperation::SIR_FMOD_THREAD_LOAD_PACKAGE && result.eResult == SIRAudioResult::SIR_AUDIO_PENDING &&
			     m_Runtime.getPendingPackageId() != 0 ) {
				nActiveLoadTicket = nTicket;
			} else {
				complete( nTicket, result );
			}
		}
		std::unique_lock<std::mutex> lock( m_WaitMutex );
		m_WaitCondition.wait_for( lock, std::chrono::milliseconds( m_Config.nIdleSleepMilliseconds ), [this]() {
			return m_bStop.load( std::memory_order_acquire ) != 0 || m_nWakes.exchange( 0, std::memory_order_acq_rel ) != 0;
		} );
	}
	if ( nActiveLoadTicket != 0 ) {
		sirFMODThreadResult_t result {};
		result.eResult = m_Runtime.cancelLoadPackage();
		complete( nActiveLoadTicket, result );
	}
	uint64_t nTicket = 0;
	while ( m_Critical.tryPop( nTicket ) || m_Requests.tryPop( nTicket ) ) {
		sirFMODThreadResult_t result {};
		result.eResult = SIRAudioResult::SIR_AUDIO_CANCELLED;
		complete( nTicket, result );
	}
	m_Runtime.getAudio().setLifecycleCallback( {} );
	const SIRAudioResult eStop = m_Runtime.shutdown();
	m_nWorkerResult.store( eStop, std::memory_order_release );
	m_nState.store( SIR_FMOD_THREAD_FAILED, std::memory_order_release );
}
