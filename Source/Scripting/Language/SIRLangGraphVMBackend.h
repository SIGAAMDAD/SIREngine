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

#ifndef __SIRENGINE_SIRLANGGRAPHVMBACKEND_H__
#define __SIRENGINE_SIRLANGGRAPHVMBACKEND_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Scripting/Language/SIRLangBytecode.h>
#include <Scripting/Public/SIRScriptTypes.h>

// Every slot has caller-owned bounded decoding storage. The decoder never
// allocates or aliases the source view past the pLoadProgram callback.
typedef struct sirLangGraphVMProgramSlot_s
{
	sirLangCIRStorage_t storage;
	uint64_t nToken;
	uint32_t nInstances;
	bool bLoaded;
} sirLangGraphVMProgramSlot_t;

typedef struct sirLangGraphVMInstanceSlot_s
{
	uint64_t nToken;
	uint64_t nOwnerId;
	uint32_t iProgram;
	bool bActive;
	bool bInitialized;
} sirLangGraphVMInstanceSlot_t;

// The host owns every program and instance slot and the capability manifest.
// Source revisions remain pinned by SIRScriptRuntime until unload. No second
// scheduler, binding registry, VM allocator or resource authority is created.
typedef struct sirLangGraphVMBackend_s
{
	sirLangGraphVMProgramSlot_t *pPrograms;
	uint32_t nProgramCapacity;
	sirLangGraphVMInstanceSlot_t *pInstances;
	uint32_t nInstanceCapacity;
	uint64_t nFormatId;
	uint64_t nSchemaId;
	uint64_t nLayoutDigest;
	uint64_t nBindingDigest;
	uint64_t nFuelPerInvoke;
	uint64_t nTokenSequence;
	uint32_t nOwnerLane;
	sirLangVMNativeCall_t pNativeCall;
	void *pNativeUser;
	bool bStarted;
} sirLangGraphVMBackend_t;

// The returned callbacks implement scripting backend ABI v4. All invocation
// arguments are an array of canonical sirLangVMValue_t records and the return
// frame is one canonical record, or zero bytes for a void function.
sirScriptBackendAPI_t SIRLangGraphVMBackendAPI( sirLangGraphVMBackend_t *pBackend ) noexcept;

#endif // __SIRENGINE_SIRLANGGRAPHVMBACKEND_H__
