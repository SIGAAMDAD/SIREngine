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

#include <Core/Platform/SIRThread.h>

#include <assert.h>
#include <stdlib.h>

SIRThread::SIRThread( void ) noexcept
#if defined( _WIN32 )
    : m_hThread( nullptr ), m_nThreadId( 0 ),
#else
    : m_hThread{},
#endif
      m_pFunction( nullptr ), m_pUserData( nullptr ), m_bAlive( false ), m_bJoinable( false )
{
}

SIRThread::~SIRThread()
{
	if ( m_bJoinable && !join() ) {
		// Destroying the thread object from its own callback is invalid.
		abort();
	}
}

bool SIRThread::start( threadFunction_t pFunction, void *pUserData ) noexcept
{
	if ( m_bJoinable || pFunction == nullptr ) {
		return false;
	}
	m_pFunction = pFunction;
	m_pUserData = pUserData;
	m_bAlive.store( true, std::memory_order_release );
#if defined( _WIN32 )
	DWORD nThreadId = 0;
	HANDLE hThread = CreateThread( nullptr, 0, &SIRThread::threadEntry, this, 0, &nThreadId );
	if ( hThread == nullptr ) {
		m_bAlive.store( false, std::memory_order_release );
		return false;
	}
	m_hThread = hThread;
	m_nThreadId = nThreadId;
#else
	pthread_t hThread{};
	if ( pthread_create( &hThread, nullptr, &SIRThread::threadEntry, this ) != 0 ) {
		m_bAlive.store( false, std::memory_order_release );
		return false;
	}
	m_hThread = hThread;
#endif
	m_bJoinable = true;
	return true;
}

bool SIRThread::join( void ) noexcept
{
	if ( !m_bJoinable ) {
		return true;
	}
#if defined( _WIN32 )
	if ( GetCurrentThreadId() == m_nThreadId || WaitForSingleObject( m_hThread, INFINITE ) != WAIT_OBJECT_0 ) {
		return false;
	}
	CloseHandle( m_hThread );
	m_hThread = nullptr;
	m_nThreadId = 0;
#else
	if ( pthread_equal( pthread_self(), m_hThread ) || pthread_join( m_hThread, nullptr ) != 0 ) {
		return false;
	}
	m_hThread = pthread_t{};
#endif
	m_bJoinable = false;
	m_pFunction = nullptr;
	m_pUserData = nullptr;
	return true;
}

bool SIRThread::isJoinable( void ) const noexcept
{
	return m_bJoinable;
}

bool SIRThread::isAlive( void ) const noexcept
{
	return m_bAlive.load( std::memory_order_acquire );
}

#if defined( _WIN32 )
DWORD WINAPI SIRThread::threadEntry( LPVOID pUserData )
#else
void *SIRThread::threadEntry( void *pUserData )
#endif
{
	SIRThread *pThread = ( SIRThread * )pUserData;
	pThread->m_pFunction( pThread->m_pUserData );
	pThread->m_bAlive.store( false, std::memory_order_release );
#if defined( _WIN32 )
	return 0;
#else
	return nullptr;
#endif
}
