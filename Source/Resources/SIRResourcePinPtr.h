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

#ifndef SIRENGINE_SIRRESOURCEPINPTR_H
#define SIRENGINE_SIRRESOURCEPINPTR_H

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceGraph.h>

// Move-only RAII wrapper for an exact provider-pinned immutable revision.
// Releasing a pin does not signal GPU/audio completion. Callers must release
// only after the provider has finished using that particular revision.
class SIRResourcePinPtr
{
public:
	SIRResourcePinPtr( void ) noexcept;
	~SIRResourcePinPtr( void ) noexcept;
	SIRResourcePinPtr( const SIRResourcePinPtr& ) = delete;
	SIRResourcePinPtr( SIRResourcePinPtr&& rOther ) noexcept;
	SIRResourcePinPtr& operator=( const SIRResourcePinPtr& ) = delete;
	SIRResourcePinPtr& operator=( SIRResourcePinPtr&& rOther ) noexcept;

	bool isValid( void ) const noexcept;
	SIRResourcePinHandle getHandle( void ) const noexcept;
	uint64_t getRevision( void ) const noexcept;
	SIRResourceResult reset( void ) noexcept;
	SIRResourceResult pin( const SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, const SIRResourceView& rView,
	                       uint32_t nProviderId ) noexcept;

private:
	const SIRResourceGraph *m_pGraph;
	SIRResourcePinHandle m_hPin;
	uint64_t m_nRevision;
};

#endif // SIRENGINE_SIRRESOURCEPINPTR_H
