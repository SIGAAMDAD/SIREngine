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

#ifndef __SIR_DERIVED_RESOURCE_PTR_H__
#define __SIR_DERIVED_RESOURCE_PTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceGraph.h>
#include <stdlib.h>

// Move-only retirement request for an ALREADY published derived representation.
// The required completion set must describe the real provider lane boundary.
// Destruction runs on the derived registry's writer thread; the registry itself
// retains the native backing until completion is reported.
class SIRDerivedResourcePtr
{
public:
	SIRDerivedResourcePtr( void ) noexcept = default;
	SIRDerivedResourcePtr( const SIRDerivedResourcePtr& ) = delete;
	SIRDerivedResourcePtr( SIRDerivedResourcePtr&& rOther ) noexcept
	    : m_pRegistry( rOther.m_pRegistry ), m_hDerived( rOther.m_hDerived ), m_Required( rOther.m_Required )
	{
		rOther.m_pRegistry = nullptr;
		rOther.m_hDerived = {};
		rOther.m_Required = {};
	}
	~SIRDerivedResourcePtr() noexcept
	{
		if ( reset() != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			abort();
		}
	}

	SIRDerivedResourcePtr& operator=( const SIRDerivedResourcePtr& ) = delete;
	SIRDerivedResourcePtr& operator=( SIRDerivedResourcePtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			if ( reset() != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
				abort();
			}
			m_pRegistry = rOther.m_pRegistry;
			m_hDerived = rOther.m_hDerived;
			m_Required = rOther.m_Required;
			rOther.m_pRegistry = nullptr;
			rOther.m_hDerived = {};
			rOther.m_Required = {};
		}
		return *this;
	}

	bool isValid( void ) const noexcept
	{
		return m_pRegistry != nullptr;
	}
	SIRDerivedResourceHandle getHandle( void ) const noexcept
	{
		return m_hDerived;
	}
	SIRResourceResult adopt( SIRResourceDerivedRegistry& rRegistry, SIRDerivedResourceHandle hDerived,
	                         const sirProviderCompletionSet_t& rRequired ) noexcept
	{
		if ( isValid() ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
		}
		if ( !sirProviderCompletionValid( rRequired ) ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		SIRDerivedResourceAccounting accounting {};
		const SIRResourceResult eResult = rRegistry.getAccounting( hDerived, &accounting );
		if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		m_pRegistry = &rRegistry;
		m_hDerived = hDerived;
		m_Required = rRequired;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
	SIRResourceResult reset( void ) noexcept
	{
		if ( m_pRegistry == nullptr ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
		}
		const SIRResourceResult eResult = m_pRegistry->retireRepresentation( m_hDerived, m_Required );
		if ( eResult == SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			m_pRegistry = nullptr;
			m_hDerived = {};
			m_Required = {};
		}
		return eResult;
	}

private:
	SIRResourceDerivedRegistry *m_pRegistry = nullptr;
	SIRDerivedResourceHandle m_hDerived {};
	sirProviderCompletionSet_t m_Required {};
};

#endif // __SIR_DERIVED_RESOURCE_PTR_H__
