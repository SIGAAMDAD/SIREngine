/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Resources/SIRResourceGraphSync.h>
#include <atomic>
#include <Core/Platform/SIRScopedLock.h>
#include <assert.h>

int main( void )
{
	std::atomic<uint64_t> szEpochs[4] = {};
	SIRResourceGraphSync sync;
	sync.configure( szEpochs, 4 );
	assert( sync.isWriterThread() );
	assert( sync.maxReaders() == 4 );
	const uint64_t nToken = sync.enterRead();
	assert( nToken != 0 && sync.countActiveReaders() == 1 );
	{
		SIRResourceGraphWriteGuard writer( sync );
		assert( writer.isActive() );
		assert( !sync.beginWrite() );
		const uint64_t nEpoch = sync.currentEpoch();
		sync.advanceEpoch( nEpoch );
		assert( sync.currentEpoch() == nEpoch + 1 );
		assert( sync.oldestProtectedEpoch() == nEpoch );
	}
	sync.leaveRead( nToken );
	assert( sync.readersQuiescent() );
	SIRResourceGraphPinMutex mutex;
	{
		SIRScopedLock<SIRResourceGraphPinMutex> guard( mutex );
	}
	assert( mutex.try_lock() );
	mutex.unlock();
	return 0;
}
