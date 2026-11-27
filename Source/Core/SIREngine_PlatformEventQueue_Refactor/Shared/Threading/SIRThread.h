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

#ifndef __SIR_THREAD_H__
#define __SIR_THREAD_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>

#include <Core/Platform/Shared/Storage/SIRPlatformNativeStorage.h>

typedef struct sirThreadId_s
{
	uint64_t nValue;
} sirThreadId_t;

// Thread object must outlive its callback. Destruction joins the worker.
class SIRThread
{
public:
	typedef void ( *threadFunction_t )( void *pUserData );

	explicit SIRThread( void ) noexcept;
	explicit SIRThread( const SIRThread& ) = delete;
	explicit SIRThread( SIRThread&& ) = delete;
	~SIRThread();

	SIRThread& operator=( const SIRThread& ) = delete;
	SIRThread& operator=( SIRThread&& ) = delete;

	bool start( threadFunction_t pFunction, void *pUserData ) noexcept;
	bool join( void ) noexcept;
	bool isJoinable( void ) const noexcept;
	bool isAlive( void ) const noexcept;
	bool cancelSynchronousIo( void ) noexcept;
	static void cpuRelax( void ) noexcept;
	static void yieldThread( void ) noexcept;
	static void sleepThread( uint32_t nMilliseconds ) noexcept;
	static sirThreadId_t currentThreadId( void ) noexcept;
	static bool isCurrentThread( sirThreadId_t hThread ) noexcept;

private:
#if defined( _WIN32 )
	static unsigned long __stdcall threadEntry( void *pUserData );
#else
	static void *threadEntry( void *pUserData );
#endif
	sirPlatformNativeStorage_t m_Native;
	threadFunction_t m_pFunction;
	void *m_pUserData;
	std::atomic<bool> m_bAlive;
	bool m_bJoinable;
};

#endif // __SIR_THREAD_H__
