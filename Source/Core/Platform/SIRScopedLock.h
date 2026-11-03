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

#ifndef __SIRENGINE_SIRSCOPEDLOCK_H__
#define __SIRENGINE_SIRSCOPEDLOCK_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Lock ownership is always lexical; the lock object must outlive the guard.
template <typename Lockable>
class SIRScopedLock
{
public:
	typedef Lockable mutex_type;
	explicit SIRScopedLock( Lockable &lock ) noexcept;
	~SIRScopedLock() noexcept;
	SIRScopedLock( const SIRScopedLock & ) = delete;
	SIRScopedLock &operator=( const SIRScopedLock & ) = delete;
	SIRScopedLock( SIRScopedLock && ) = delete;
	SIRScopedLock &operator=( SIRScopedLock && ) = delete;

private:
	Lockable &m_Lock;
};

template <typename Lockable>
SIRScopedLock<Lockable>::SIRScopedLock( Lockable &lock ) noexcept : m_Lock( lock )
{
	m_Lock.lock();
}

template <typename Lockable>
SIRScopedLock<Lockable>::~SIRScopedLock() noexcept
{
	m_Lock.unlock();
}

template <typename Lockable>
SIRScopedLock( Lockable & ) -> SIRScopedLock<Lockable>;

template <typename Lockable>
class SIRConditionalScopedLock
{
public:
	explicit SIRConditionalScopedLock( Lockable &lock, bool bEnabled ) noexcept;
	~SIRConditionalScopedLock() noexcept;
	SIRConditionalScopedLock( const SIRConditionalScopedLock & ) = delete;
	SIRConditionalScopedLock &operator=( const SIRConditionalScopedLock & ) = delete;
	SIRConditionalScopedLock( SIRConditionalScopedLock && ) = delete;
	SIRConditionalScopedLock &operator=( SIRConditionalScopedLock && ) = delete;

private:
	Lockable &m_Lock;
	bool m_bEnabled;
};

template <typename Lockable>
SIRConditionalScopedLock<Lockable>::SIRConditionalScopedLock( Lockable &lock, bool bEnabled ) noexcept
    : m_Lock( lock ), m_bEnabled( bEnabled )
{
	if ( m_bEnabled ) {
		m_Lock.lock();
	}
}

template <typename Lockable>
SIRConditionalScopedLock<Lockable>::~SIRConditionalScopedLock() noexcept
{
	if ( m_bEnabled ) {
		m_Lock.unlock();
	}
}

template <typename Lockable>
SIRConditionalScopedLock( Lockable &, bool ) -> SIRConditionalScopedLock<Lockable>;

#endif // __SIRENGINE_SIRSCOPEDLOCK_H__
