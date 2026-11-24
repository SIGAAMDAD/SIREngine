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

#ifndef __SIR_SELFRELATIVEPTR_H__
#define __SIR_SELFRELATIVEPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <limits.h>
#include <type_traits>

// 32-bit self-relative offset, appropriate for pointers embedded in cooked
// relocatable blobs. A memcpy of the COMPLETE enclosing blob preserves
// internal links; independently copying just this field does not.
// INT32_MIN encodes null; the one corresponding negative offset is reserved.
template <typename T>
class SIRSelfRelativePtr
{
public:
	SIRSelfRelativePtr( void ) noexcept = default;

	bool set( T *pTarget ) noexcept
	{
		if ( pTarget == nullptr ) {
			m_nOffset = INT32_MIN;
			return true;
		}
		const uintptr_t nBase = reinterpret_cast<uintptr_t>(this);
		const uintptr_t nTarget = reinterpret_cast<uintptr_t>(pTarget);
		if ( nTarget >= nBase ) {
			const uintptr_t nDistance = nTarget - nBase;
			if ( nDistance > INT32_MAX ) {
				return false;
			}
			m_nOffset = static_cast<int32_t>(nDistance);
			return true;
		}

		const uintptr_t nDistance = nBase - nTarget;
		if ( nDistance > INT32_MAX ) {
			return false;
		}
		m_nOffset = -static_cast<int32_t>(nDistance);
		return true;
	}

	T *get( void ) const noexcept
	{
		if ( m_nOffset == INT32_MIN ) {
			return nullptr;
		}
		const uintptr_t nBase = reinterpret_cast<uintptr_t>(this);
		if ( m_nOffset >= 0 ) {
			return ( T * )( nBase + static_cast<uintptr_t>(m_nOffset) );
		}
		return ( T * )( nBase - ( uintptr_t )( -static_cast<int64_t>(m_nOffset) ) );
	}
	T *operator->( void ) const noexcept
	{
		return get();
	}
	T& operator*( void ) const noexcept
	{
		return *get();
	}
	explicit operator bool( void ) const noexcept
	{
		return get() != nullptr;
	}
	int32_t getOffset( void ) const noexcept
	{
		return m_nOffset;
	}

private:
	int32_t m_nOffset = INT32_MIN;
};

static_assert( sizeof( SIRSelfRelativePtr<uint8_t> ) == sizeof( int32_t ), "Self-relative pointers must be 32-bit" );
static_assert( std::is_trivially_copyable<SIRSelfRelativePtr<uint8_t>>::value, "Self-relative representation must be trivially copyable" );

#endif // __SIR_SELFRELATIVEPTR_H__
