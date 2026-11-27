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

#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>

SIRSpinLock::SIRSpinLock( void ) noexcept
    : m_Lock( false )
{
}

void SIRSpinLock::lock( void ) noexcept
{
	uint32_t nSpins = 0;
	while ( m_Lock.exchange( true, std::memory_order_acquire ) ) {
		while ( m_Lock.load( std::memory_order_relaxed ) ) {
			SIRThread::cpuRelax();
			++nSpins;
			if ( nSpins == 1024 ) {
				SIRThread::yieldThread();
				nSpins = 0;
			}
		}
	}
}

bool SIRSpinLock::tryLock( void ) noexcept
{
	return !m_Lock.exchange( true, std::memory_order_acquire );
}

void SIRSpinLock::unlock( void ) noexcept
{
	m_Lock.store( false, std::memory_order_release );
}
