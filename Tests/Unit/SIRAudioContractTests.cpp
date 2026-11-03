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

#include "SIRAudioMockBackend.h"
#include <Audio/SIRAudioSystem.h>
#include <Core/Containers/SIRContainerResult.h>
#include <stdlib.h>
#include <thread>
#include <atomic>

static void testCheck( bool bCondition ) noexcept
{
	if ( !bCondition ) {
		abort();
	}
}

static std::atomic<uint32_t> s_nLiveAllocations( 0 );

static void *testAllocate( void *, size_t nBytes, size_t ) noexcept
{
	void *pMemory = malloc( nBytes );
	if ( pMemory != nullptr ) {
		s_nLiveAllocations.fetch_add( 1 );
	}
	return pMemory;
}

static void testFree( void *, void *pData, size_t, size_t ) noexcept
{
	if ( pData != nullptr ) {
		s_nLiveAllocations.fetch_sub( 1 );
		free( pData );
	}
}

static void testQueue( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioQueue<sirAudioCommand_t> queue;
	testCheck( !queue.initialize( pAllocator, 3 ) );
	testCheck( queue.initialize( pAllocator, 4 ) );
	sirAudioCommand_t command = {};
	command.nSequence = 1;
	for ( uint64_t iIndex = 1; iIndex <= 4; ++iIndex ) {
		command.nSequence = iIndex;
		testCheck( queue.tryPush( command ) );
	}
	testCheck( !queue.tryPush( command ) );
	for ( uint64_t iIndex = 1; iIndex <= 4; ++iIndex ) {
		testCheck( queue.tryPop( command ) );
		testCheck( command.nSequence == iIndex );
	}
	testCheck( !queue.tryPop( command ) );
	queue.shutdown();
}

static void testConcurrentQueue( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioQueue<sirAudioCommand_t> queue;
	testCheck( queue.initialize( pAllocator, 256 ) );
	constexpr uint32_t N_PER_PRODUCER = 2000;
	std::thread producers[4];
	for ( uint32_t iProducer = 0; iProducer < 4; ++iProducer ) {
		producers[iProducer] = std::thread( [&, iProducer]() {
			sirAudioCommand_t command = {};
			for ( uint32_t iIndex = 0; iIndex < N_PER_PRODUCER; ++iIndex ) {
				command.nSequence = ( uint64_t )iProducer * N_PER_PRODUCER + iIndex + 1;
				while ( !queue.tryPush( command ) ) {
					std::this_thread::yield();
				}
			}
		} );
	}
	uint8_t seen[N_PER_PRODUCER * 4] = {};
	uint32_t nReceived = 0;
	sirAudioCommand_t command = {};
	while ( nReceived < N_PER_PRODUCER * 4 ) {
		if ( !queue.tryPop( command ) ) {
			std::this_thread::yield();
			continue;
		}
		testCheck( command.nSequence >= 1 && command.nSequence <= N_PER_PRODUCER * 4 );
		testCheck( !seen[command.nSequence - 1] );
		seen[command.nSequence - 1] = 1;
		++nReceived;
	}
	for ( std::thread &producer : producers ) {
		producer.join();
	}
	queue.shutdown();
}

static void testLifecycle( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 2, 0, 4, 2, 8 };
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t first = {};
	testCheck( audio.playEvent( 100, {}, first ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	testCheck( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	testCheck( audio.stopEvent( first, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	testCheck( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	sirAudioPlayingHandle_t second = {};
	testCheck( audio.playEvent( 101, {}, second ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	testCheck( audio.stopEvent( first, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );
	testCheck( audio.setParameter( second, 99, 0.5f ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	backend.failNextPlay();
	testCheck( audio.playEvent( 102, {}, first ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	testCheck( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FAILED );
	audio.shutdown();
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	testCheck( audio.stopEvent( second, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );
	audio.shutdown();
}

static void testQueueBoundaries( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioQueue<sirAudioCompletion_t> queue;
	testCheck( !queue.initialize( nullptr, 4 ) );
	testCheck( !queue.initialize( pAllocator, 1 ) );
	testCheck( !queue.initialize( pAllocator, 5 ) );
	testCheck( queue.initialize( pAllocator, 2 ) );
	testCheck( !queue.initialize( pAllocator, 2 ) );
	sirAudioCompletion_t item = {};
	testCheck( queue.tryPush( item ) );
	testCheck( queue.tryPush( item ) );
	testCheck( !queue.tryPush( item ) );
	testCheck( queue.tryPop( item ) );
	testCheck( queue.tryPop( item ) );
	testCheck( !queue.tryPop( item ) );
	queue.shutdown();
	testCheck( !queue.tryPush( item ) );
	testCheck( !queue.tryPop( item ) );
}

static void testFailureAndCapacity( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 4, 0, 2, 2, 2 };
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t handles[4] = {};
	testCheck( audio.playEvent( 100, {}, handles[0] ) == SIR_AUDIO_PENDING );
	testCheck( audio.playEvent( 101, {}, handles[1] ) == SIR_AUDIO_PENDING );
	testCheck( audio.playEvent( 102, {}, handles[2] ) == SIR_AUDIO_CAPACITY );
	testCheck( handles[2].nEpoch == 0 );
	testCheck( audio.update() == 2 );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	testCheck( audio.getPlaybackState( handles[0], eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	testCheck( audio.getPlaybackState( handles[1], eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	testCheck( audio.stopEvent( handles[0], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.stopEvent( handles[0], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );
	backend.failNextStop();
	testCheck( audio.update() == 1 );
	testCheck( audio.getPlaybackState( handles[0], eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	testCheck( audio.stopEvent( handles[0], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.stopEvent( handles[1], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 2 );
	testCheck( audio.getPlaybackState( handles[0], eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	testCheck( audio.getPlaybackState( handles[1], eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	audio.shutdown();
}

static void testStaleAfterReuse( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 1, 0, 2, 2, 2 };
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t first = {};
	testCheck( audio.playEvent( 200, {}, first ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	testCheck( audio.stopEvent( first, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	sirAudioPlayingHandle_t second = {};
	testCheck( audio.playEvent( 201, {}, second ) == SIR_AUDIO_PENDING );
	testCheck( first.nSlot == second.nSlot && first.nGeneration != second.nGeneration );
	testCheck( audio.stopEvent( first, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );
	testCheck( audio.update() == 1 );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	const sirAudioCompletion_t late = { first, 1, SIR_AUDIO_BACKEND_ERROR, SIR_AUDIO_COMPLETION_FAILED, {} };
	testCheck( backend.injectCompletion( late ) );
	testCheck( audio.update() == 0 );
	testCheck( audio.getPlaybackState( second, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	audio.shutdown();
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	testCheck( audio.stopEvent( second, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );
	audio.shutdown();
}

static void testLateParameterFailure( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 1, 0, 4, 2, 2 };
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t playing = {};
	testCheck( audio.playEvent( 300, {}, playing ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	testCheck( audio.setParameter( playing, 55, 0.75f ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	const sirAudioCompletion_t failedControl = { playing, 2, SIR_AUDIO_UNSUPPORTED, SIR_AUDIO_COMPLETION_FAILED, {} };
	testCheck( backend.injectCompletion( failedControl ) );
	testCheck( audio.update() == 0 );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	testCheck( audio.getPlaybackState( playing, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	audio.shutdown();
}

static void *testRejectAllocation( void *, size_t, size_t ) noexcept
{
	return nullptr;
}

static void testInitializationRollback( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t valid = { 2, 0, 4, 2, 2 };
	sirAudioBackendOps_t ops = SIRAudioMockBackend::getOps();
	ops.nAbiVersion = 999;
	testCheck( audio.initialize( valid, pAllocator, ops, &backend ) == SIR_AUDIO_INVALID_ARGUMENT );
	const sirContainerAllocator_t rejecting = { nullptr, testRejectAllocation, testFree, 0, 0 };
	testCheck( audio.initialize( valid, &rejecting, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_CAPACITY );
	const uint32_t nBefore = s_nLiveAllocations.load();
	const sirAudioSystemConfig_t invalidMock = { 2, 0, 4, 2, 3 };
	testCheck( audio.initialize( invalidMock, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_CAPACITY );
	testCheck( s_nLiveAllocations.load() == nBefore );
	testCheck( audio.initialize( valid, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	audio.shutdown();
}

static void testStopReservedQueue( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 4, 0, 4, 2, 2 };
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t handles[3] = {};
	for ( uint32_t iIndex = 0; iIndex < 3; ++iIndex ) {
		testCheck( audio.playEvent( 400 + iIndex, {}, handles[iIndex] ) == SIR_AUDIO_PENDING );
	}
	testCheck( audio.update() == 3 );
	testCheck( audio.stopEvent( handles[0], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.stopEvent( handles[1], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.stopEvent( handles[2], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_CAPACITY );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	testCheck( audio.getPlaybackState( handles[2], eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	testCheck( audio.update() == 2 );
	testCheck( audio.stopEvent( handles[2], SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	testCheck( audio.getPlaybackState( handles[2], eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	audio.shutdown();
}

static void testNaturalFinish( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioMockBackend backend( pAllocator );
	SIRAudioSystem audio;
	const sirAudioSystemConfig_t config = { 1, 0, 4, 2, 2 };
	testCheck( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t first = {};
	testCheck( audio.playEvent( 500, {}, first ) == SIR_AUDIO_PENDING );
	testCheck( audio.update() == 1 );
	const sirAudioCompletion_t natural = { first, 1, SIR_AUDIO_OK, SIR_AUDIO_COMPLETION_FINISHED, {} };
	testCheck( backend.injectCompletion( natural ) );
	testCheck( audio.update() == 0 );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	testCheck( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	sirAudioPlayingHandle_t second = {};
	testCheck( audio.playEvent( 501, {}, second ) == SIR_AUDIO_PENDING );
	testCheck( second.nGeneration != first.nGeneration );
	audio.shutdown();
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, testAllocate, testFree, 0, 0 };
	testQueue( &allocator );
	testConcurrentQueue( &allocator );
	testQueueBoundaries( &allocator );
	testLifecycle( &allocator );
	testFailureAndCapacity( &allocator );
	testStaleAfterReuse( &allocator );
	testLateParameterFailure( &allocator );
	testInitializationRollback( &allocator );
	testStopReservedQueue( &allocator );
	testNaturalFinish( &allocator );
	testCheck( s_nLiveAllocations.load() == 0 );
	return 0;
}
