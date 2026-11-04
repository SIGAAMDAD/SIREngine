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

#ifndef __SIRENGINE_SIRTHREAD_H__
#define __SIRENGINE_SIRTHREAD_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined( __linux__ )
#include <pthread.h>
#else
#error "SIREngine threads require a Windows or Linux backend"
#endif

// Thread object must outlive its callback. Destruction joins the worker.
class SIRThread
{
public:
	typedef void ( *threadFunction_t )( void *pUserData );

	SIRThread( void ) noexcept;
	SIRThread( const SIRThread& ) = delete;
	SIRThread& operator=( const SIRThread& ) = delete;
	~SIRThread();

	bool start( threadFunction_t pFunction, void *pUserData ) noexcept;
	bool join( void ) noexcept;
	bool isJoinable( void ) const noexcept;
	bool isAlive( void ) const noexcept;

private:
#if defined( _WIN32 )
	static DWORD WINAPI threadEntry( LPVOID pUserData );
	HANDLE m_hThread;
	DWORD m_nThreadId;
#else
	static void *threadEntry( void *pUserData );
	pthread_t m_hThread;
#endif
	threadFunction_t m_pFunction;
	void *m_pUserData;
	std::atomic<bool> m_bAlive;
	bool m_bJoinable;
};

#endif // __SIRENGINE_SIRTHREAD_H__
