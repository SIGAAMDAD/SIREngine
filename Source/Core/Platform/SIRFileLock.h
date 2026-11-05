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

#ifndef __SIRENGINE_SIRFILELOCK_H__
#define __SIRENGINE_SIRFILELOCK_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

// Locks a persistent, caller-chosen file path across processes. The lock file
// must not be unlinked while any process may hold or acquire its lock.
class SIRFileLock
{
public:
	explicit SIRFileLock( const char *pszLockPath ) noexcept;
	SIRFileLock( const SIRFileLock& ) = delete;
	SIRFileLock( SIRFileLock&& ) = delete;
	~SIRFileLock( void ) noexcept;

	SIRFileLock& operator=( const SIRFileLock& ) = delete;
	SIRFileLock& operator=( SIRFileLock&& ) = delete;

	bool isLocked( void ) const noexcept;

protected:
private:
	intptr_t m_nHandle;
};

#endif // __SIRENGINE_SIRFILELOCK_H__
