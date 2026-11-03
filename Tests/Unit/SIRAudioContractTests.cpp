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

#include <Audio/SIRAudioMockBackend.h>
#include <Audio/SIRAudioSystem.h>
#include <Core/Containers/SIRContainerResult.h>
#include <assert.h>
#include <stdlib.h>
#include <thread>

static void *testAllocate( void *, size_t nBytes, size_t ) noexcept
{
	return malloc( nBytes );
}

static void testFree( void *, void *pData, size_t, size_t ) noexcept
{
	free( pData );
}

static void testQueue( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioQueue<sirAudioCommand_t> queue;
	assert( !queue.initialize( pAllocator, 3 ) );
	assert( queue.initialize( pAllocator, 4 ) );
	sirAudioCommand_t command = {};
	command.nSequence = 1;
	for ( uint64_t iIndex = 1; iIndex <= 4; ++iIndex ) {
		command.nSequence = iIndex;
		assert( queue.tryPush( command ) );
	}
	assert( !queue.tryPush( command ) );
	for ( uint64_t iIndex = 1; iIndex <= 4; ++iIndex ) {
		assert( queue.tryPop( command ) );
		assert( command.nSequence == iIndex );
	}
	assert( !queue.tryPop( command ) );
	queue.shutdown();
}

static void testConcurrentQueue( const sirContainerAllocator_t *pAllocator )
{
	SIRAudioQueue<sirAudioCommand_t> queue;
	assert( queue.initialize( pAllocator, 256 ) );
	constexpr uint32_t N_PER_PRODUCER = 2000;
	std::thread producers[4];
	for ( uint32_t iProducer = 0; iProducer < 4; ++iProducer ) {
		producers[iProducer] = std::thread( [&queue, iProducer]() {
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
		assert( command.nSequence >= 1 && command.nSequence <= N_PER_PRODUCER * 4 );
		assert( !seen[command.nSequence - 1] );
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
	assert( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	sirAudioPlayingHandle_t first = {};
	assert( audio.playEvent( 100, {}, first ) == SIR_AUDIO_PENDING );
	assert( audio.update() == 1 );
	sirAudioPlaybackState_t eState = SIR_AUDIO_PLAYBACK_FREE;
	assert( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_PLAYING );
	assert( audio.stopEvent( first, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_PENDING );
	assert( audio.update() == 1 );
	assert( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FINISHED );
	sirAudioPlayingHandle_t second = {};
	assert( audio.playEvent( 101, {}, second ) == SIR_AUDIO_PENDING );
	assert( audio.update() == 1 );
	assert( audio.stopEvent( first, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );
	assert( audio.setParameter( second, 99, 0.5f ) == SIR_AUDIO_PENDING );
	assert( audio.update() == 1 );
	backend.failNextPlay();
	assert( audio.playEvent( 102, {}, first ) == SIR_AUDIO_PENDING );
	assert( audio.update() == 1 );
	assert( audio.getPlaybackState( first, eState ) == SIR_AUDIO_OK && eState == SIR_AUDIO_PLAYBACK_FAILED );
	audio.shutdown();
	assert( audio.initialize( config, pAllocator, SIRAudioMockBackend::getOps(), &backend ) == SIR_AUDIO_OK );
	assert( audio.stopEvent( second, SIR_AUDIO_STOP_IMMEDIATE ) == SIR_AUDIO_INVALID_HANDLE );
	audio.shutdown();
}

int main( void )
{
	const sirContainerAllocator_t allocator = { nullptr, testAllocate, testFree, 0, 0 };
	testQueue( &allocator );
	testConcurrentQueue( &allocator );
	testLifecycle( &allocator );
	return 0;
}
