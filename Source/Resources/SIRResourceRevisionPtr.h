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

#ifndef __SIR_RESOURCEREVISIONPTR_H__
#define __SIR_RESOURCEREVISIONPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRResourcePinPtr.h"

// An exact, immutable, published revision lease. The provider ID is supplied
// by the caller; this does not imply GPU completion or ownership of bytes.
template <typename T>
class SIRResourceRevisionPtr
{
public:
	SIRResourceRevisionPtr( void ) noexcept = default;
	SIRResourceRevisionPtr( const SIRResourceRevisionPtr& ) = delete;
	SIRResourceRevisionPtr& operator=( const SIRResourceRevisionPtr& ) = delete;
	SIRResourceRevisionPtr( SIRResourceRevisionPtr&& ) noexcept = default;
	SIRResourceRevisionPtr& operator=( SIRResourceRevisionPtr&& ) noexcept = default;

	bool isValid( void ) const noexcept
	{
		return m_Pin.isValid();
	}
	uint64_t getRevision( void ) const noexcept
	{
		return m_Pin.getRevision();
	}
	SIRResourcePinHandle getPinHandle( void ) const noexcept
	{
		return m_Pin.getHandle();
	}
	SIRResourceHandle getResourceHandle( void ) const noexcept
	{
		return m_hResource;
	}
	SIRResourceResult reset( void ) noexcept
	{
		const SIRResourceResult eResult = m_Pin.reset();
		if ( eResult == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			m_hResource = {};
		}
		return eResult;
	}
	SIRResourceResult pin( const SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, const SIRResourceView& rView,
	                       uint32_t nProviderId ) noexcept
	{
		if ( rView.getKind() != T::RESOURCE_KIND ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_TYPE_MISMATCH;
		}
		const SIRResourceResult eResult = m_Pin.pin( rGraph, rScope, rView, nProviderId );
		if ( eResult == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			m_hResource = rView.getHandle();
		}
		return eResult;
	}

private:
	SIRResourcePinPtr m_Pin;
	SIRResourceHandle m_hResource {};
};

#endif // __SIR_RESOURCEREVISIONPTR_H__
