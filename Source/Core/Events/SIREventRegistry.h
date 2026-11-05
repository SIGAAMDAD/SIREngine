/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIREVENTREGISTRY_H__
#define __SIRENGINE_SIREVENTREGISTRY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIREventTypes.h>
#include <Core/Platform/SIRScopedLock.h>
#include <Core/Platform/SIRSync.h>
#include <stdint.h>

typedef struct sirRegisteredEvent_s
{
	uint32_t nTypeId;
	uint32_t nPayloadBytes;
	uint32_t nPayloadAlignment;
	uint32_t nGeneration;
	char szName[48];
} sirRegisteredEvent_t;

typedef struct sirEventRegistrationSlot_s
{
	sirRegisteredEvent_t descriptor;
	bool bUsed;
} sirEventRegistrationSlot_t;

// Cold registration and discovery. No type-erased callback ownership and no
// unpinned event pointers can escape this registry. Hot paths cache typed signals.
class SIREventRegistry
{
public:
	static constexpr uint32_t MAX_EVENTS = 64;
	SIREventRegistry( void ) noexcept;
	SIREventRegistry( const SIREventRegistry& ) = delete;
	SIREventRegistry& operator=( const SIREventRegistry& ) = delete;
	bool registerEvent( uint32_t nTypeId, const char *pszName, uint32_t nBytes, uint32_t nAlignment, sirEventHandle_t& handle ) noexcept;
	bool unregisterEvent( sirEventHandle_t handle ) noexcept;
	bool findByType( uint32_t nTypeId, sirEventHandle_t& handle ) const noexcept;
	bool getDescriptor( sirEventHandle_t handle, sirRegisteredEvent_t& descriptor ) const noexcept;
	void clear( void ) noexcept;
	uint32_t count( void ) const noexcept;

private:
	mutable SIRMutex m_Mutex;
	sirEventRegistrationSlot_t m_Slots[MAX_EVENTS];
	uint32_t m_nCount;
};

#endif // __SIRENGINE_SIREVENTREGISTRY_H__
