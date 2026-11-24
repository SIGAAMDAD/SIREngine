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

#ifndef __SIR_SYNC_H__
#define __SIR_SYNC_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Storage/SIRPlatformNativeStorage.h>

#include <atomic>

class SIRConditionVariable;

class SIRMutex
{
public:
	explicit SIRMutex( void ) noexcept;
	explicit SIRMutex( const SIRMutex& ) = delete;
	explicit SIRMutex( SIRMutex&& ) = delete;
	~SIRMutex();

	SIRMutex& operator=( const SIRMutex& ) = delete;
	SIRMutex& operator=( SIRMutex&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	friend class SIRConditionVariable;
	sirPlatformNativeStorage_t m_Native;
};

class SIRRecursiveMutex
{
public:
	explicit SIRRecursiveMutex( void ) noexcept;
	explicit SIRRecursiveMutex( const SIRRecursiveMutex& ) = delete;
	explicit SIRRecursiveMutex( SIRRecursiveMutex&& ) = delete;
	~SIRRecursiveMutex();

	SIRRecursiveMutex& operator=( const SIRRecursiveMutex& ) = delete;
	SIRRecursiveMutex& operator=( SIRRecursiveMutex&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	sirPlatformNativeStorage_t m_Native;
};

class SIRConditionVariable
{
public:
	explicit SIRConditionVariable( void ) noexcept;
	explicit SIRConditionVariable( const SIRConditionVariable& ) = delete;
	explicit SIRConditionVariable( SIRConditionVariable&& ) = delete;
	~SIRConditionVariable();

	SIRConditionVariable& operator=( const SIRConditionVariable& ) = delete;
	SIRConditionVariable& operator=( SIRConditionVariable&& ) = delete;
	void wait( SIRMutex& mutex ) noexcept;
	void notifyOne( void ) noexcept;
	void notifyAll( void ) noexcept;

private:
	sirPlatformNativeStorage_t m_Native;
};

class SIRSpinLock
{
public:
	explicit SIRSpinLock( void ) noexcept;
	explicit SIRSpinLock( const SIRSpinLock& ) = delete;
	explicit SIRSpinLock( SIRSpinLock&& ) = delete;

	SIRSpinLock& operator=( const SIRSpinLock& ) = delete;
	SIRSpinLock& operator=( SIRSpinLock&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	std::atomic<bool> m_Lock;
};

#endif // __SIR_SYNC_H__
