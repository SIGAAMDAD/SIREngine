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

#ifndef __SIRENGINE_SIRSCRIPTMEMORYROUTER_H__
#define __SIRENGINE_SIRSCRIPTMEMORYROUTER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptScheduler.h>

class SIRAllocationRouter;

// Adapter from SIRMemory's individually reclaimable router allocations to
// the script scheduler's opaque payload leases. Router and its backing pools
// must outlive the scripting runtime, and share its owner thread.
class SIRScriptMemoryRouter
{
public:
	static sirScriptPayloadAllocatorAPI_t makeAPI( SIRAllocationRouter *pRouter, uint32_t nMaximumBytes,
	                                               uint32_t nMaximumAlignment ) noexcept;

private:
	static sirScriptResult_t allocate( void *pContext, uint32_t nBytes, uint32_t nAlignment,
	                                   sirScriptPayloadAllocation_t *pAllocation ) noexcept;
	static void release( void *pContext, sirScriptPayloadAllocation_t allocation ) noexcept;
};

#endif // __SIRENGINE_SIRSCRIPTMEMORYROUTER_H__
