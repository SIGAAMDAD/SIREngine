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

#ifndef __SIRENGINE_SIRSYNC_H__
#define __SIRENGINE_SIRSYNC_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined( __linux__ )
#include <pthread.h>
#else
#error "SIREngine synchronization requires a Windows or Linux backend"
#endif

#include <atomic>

class SIRConditionVariable;

class SIRMutex
{
public:
	SIRMutex( void ) noexcept;
	~SIRMutex();
	SIRMutex( const SIRMutex & ) = delete;
	SIRMutex &operator=( const SIRMutex & ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	friend class SIRConditionVariable;
#if defined( _WIN32 )
	SRWLOCK m_hMutex;
#else
	pthread_mutex_t m_hMutex;
#endif
};

class SIRRecursiveMutex
{
public:
	SIRRecursiveMutex( void ) noexcept;
	~SIRRecursiveMutex();
	SIRRecursiveMutex( const SIRRecursiveMutex & ) = delete;
	SIRRecursiveMutex &operator=( const SIRRecursiveMutex & ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
#if defined( _WIN32 )
	CRITICAL_SECTION m_hMutex;
#else
	pthread_mutex_t m_hMutex;
#endif
};

class SIRConditionVariable
{
public:
	SIRConditionVariable( void ) noexcept;
	~SIRConditionVariable();
	SIRConditionVariable( const SIRConditionVariable & ) = delete;
	SIRConditionVariable &operator=( const SIRConditionVariable & ) = delete;
	void wait( SIRMutex &mutex ) noexcept;
	void notifyOne( void ) noexcept;
	void notifyAll( void ) noexcept;

private:
#if defined( _WIN32 )
	CONDITION_VARIABLE m_hCondition;
#else
	pthread_cond_t m_hCondition;
#endif
};

class SIRSpinLock
{
public:
	SIRSpinLock( void ) noexcept;
	SIRSpinLock( const SIRSpinLock & ) = delete;
	SIRSpinLock &operator=( const SIRSpinLock & ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	std::atomic_flag m_bLocked = ATOMIC_FLAG_INIT;
};

#endif // __SIRENGINE_SIRSYNC_H__
