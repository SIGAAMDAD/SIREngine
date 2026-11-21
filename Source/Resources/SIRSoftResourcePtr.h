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

#ifndef SIRENGINE_SIRSOFTRESOURCEPTR_H
#define SIRENGINE_SIRSOFTRESOURCEPTR_H

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRResourcePtr.h"
#include "SIRResourceReadPtr.h"

// A persistent asset key, not a claim on GRDG residency or an allocation.
// The caller supplies the graph and an active read scope at resolution time.
template <typename T>
class SIRSoftResourcePtr
{
public:
	SIRSoftResourcePtr( void ) noexcept = default;
	explicit SIRSoftResourcePtr( const SIRResourceKey& rKey ) noexcept : m_Key( rKey )
	{
	}
	bool isValid( void ) const noexcept
	{
		return m_Key.m_nTypeId != 0;
	}
	SIRResourceKey getKey( void ) const noexcept
	{
		return m_Key;
	}
	void reset( void ) noexcept
	{
		m_Key = {};
	}
	SIRResourceResult find( const SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, SIRResourceHandle *pHandle ) const noexcept
	{
		if ( !isValid() || pHandle == nullptr ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		return rGraph.find( rScope, m_Key, pHandle );
	}
	SIRResourceResult read( const SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, SIRResourceReadPtr<T> *pOut ) const noexcept
	{
		if ( pOut == nullptr ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		SIRResourceHandle hResource = {};
		SIRResourceResult eResult = find( rGraph, rScope, &hResource );
		if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
			eResult = pOut->acquire( rGraph, rScope, hResource );
		}
		return eResult;
	}
	SIRResourceResult lock( SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, SIRResourcePtr<T> *pOut ) const noexcept
	{
		if ( pOut == nullptr ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		SIRResourceHandle hResource = {};
		const SIRResourceResult eResult = find( rGraph, rScope, &hResource );
		return eResult == SIR_RESOURCE_RESULT_SUCCESS ? SIRResourcePtr<T>::acquire( rGraph, hResource, pOut ) : eResult;
	}

private:
	SIRResourceKey m_Key = {};
};

#endif // SIRENGINE_SIRSOFTRESOURCEPTR_H
