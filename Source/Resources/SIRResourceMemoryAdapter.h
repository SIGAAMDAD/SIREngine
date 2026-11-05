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

#ifndef __SIRENGINE_SIRRESOURCEMEMORYADAPTER_H__
#define __SIRENGINE_SIRRESOURCEMEMORYADAPTER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/SIRMemoryManager.h>
#include <Resources/SIRResourceGraph.h>

class SIRGeneralHeap;

// Non-owning, domain-validated binding of GRDG allocations to Core heaps.
// Bound heaps and this adapter must outlive each graph using getAllocator().
// The graph's writer thread owns allocation and retirement; heap worker
// attachment and shutdown remain Core's responsibility.
class SIRResourceMemoryAdapter
{
public:
	SIRResourceMemoryAdapter( void ) noexcept;
	SIRResourceMemoryAdapter( const SIRResourceMemoryAdapter& ) = delete;
	SIRResourceMemoryAdapter& operator=( const SIRResourceMemoryAdapter& ) = delete;

	bool bindDomain( sirMemoryDomain_t eDomain, SIRGeneralHeap *pHeap ) noexcept;
	SIRResourceAllocator getAllocator( void ) noexcept;

private:
	static void *allocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept;
	static void release( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept;

	SIRGeneralHeap *m_pHeaps[SIR_MEMORY_DOMAIN_COUNT];
};

#endif // __SIRENGINE_SIRRESOURCEMEMORYADAPTER_H__
