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

#ifndef __SIRENGINE_SIRLANGGRAPHVM_H__
#define __SIRENGINE_SIRLANGGRAPHVM_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Scripting/Language/SIRLangTypeSystem.h>

constexpr uint32_t SIR_LANG_VM_VERSION = 1;
constexpr uint32_t SIR_LANG_VM_MAX_REGISTERS = 64;
constexpr uint32_t SIR_LANG_VM_MAX_FRAMES = 32;
constexpr uint32_t SIR_LANG_VM_MAX_PARAMETERS = 32;

typedef enum sirLangOpcode_e : uint8_t
{
	SIR_LANG_OP_CONST = 0,
	SIR_LANG_OP_PARAMETER,
	SIR_LANG_OP_MOVE,
	SIR_LANG_OP_ADD,
	SIR_LANG_OP_SUBTRACT,
	SIR_LANG_OP_MULTIPLY,
	SIR_LANG_OP_DIVIDE,
	SIR_LANG_OP_EQUAL,
	SIR_LANG_OP_LESS,
	SIR_LANG_OP_LESS_EQUAL,
	SIR_LANG_OP_NOT,
	SIR_LANG_OP_AND,
	SIR_LANG_OP_OR,
	SIR_LANG_OP_JUMP,
	SIR_LANG_OP_JUMP_FALSE,
	SIR_LANG_OP_CALL,
	SIR_LANG_OP_CALL_NATIVE,
	SIR_LANG_OP_RETURN,
	SIR_LANG_OP_PHI
} sirLangOpcode_t;

typedef enum sirLangVMError_e : uint8_t
{
	SIR_LANG_VM_OK = 0,
	SIR_LANG_VM_INVALID_ARGUMENT,
	SIR_LANG_VM_INVALID_FORMAT,
	SIR_LANG_VM_INVALID_OPCODE,
	SIR_LANG_VM_INVALID_REGISTER,
	SIR_LANG_VM_INVALID_TYPE,
	SIR_LANG_VM_INVALID_CONTROL_FLOW,
	SIR_LANG_VM_INVALID_CALL,
	SIR_LANG_VM_INVALID_CAPABILITY,
	SIR_LANG_VM_DIVIDE_ZERO,
	SIR_LANG_VM_ARITHMETIC_OVERFLOW,
	SIR_LANG_VM_FUEL_EXHAUSTED,
	SIR_LANG_VM_STACK_OVERFLOW,
	SIR_LANG_VM_CAPACITY,
	SIR_LANG_VM_UNINITIALIZED
} sirLangVMError_t;

typedef enum sirLangCapabilityEffect_e : uint32_t
{
	SIR_LANG_EFFECT_PURE = 0,
	SIR_LANG_EFFECT_STATE_READ = 1U,
	SIR_LANG_EFFECT_STATE_WRITE = 2U,
	SIR_LANG_EFFECT_EMIT = 4U,
	SIR_LANG_EFFECT_ALLOCATE = 8U,
	SIR_LANG_EFFECT_SUSPEND = 16U,
	SIR_LANG_EFFECT_NONDETERMINISTIC = 32U,
	SIR_LANG_EFFECT_REENTRANT = 64U
} sirLangCapabilityEffect_t;

typedef struct sirLangVMValue_s
{
	uint64_t nBits;
	sirLangValueKind_t eType;
} sirLangVMValue_t;

// Canonical typed IR: register assignments are single-definition; branches
// end blocks and all target offsets identify valid instruction boundaries.
// No host pointers are serialized. nImmediate is an integer or exact FP bits.
typedef struct sirLangCIRInstruction_s
{
	sirLangOpcode_t eOpcode;
	sirLangValueKind_t eType;
	uint16_t nFlags;
	uint32_t iDestination;
	uint32_t iFirstSource;
	uint32_t iSecondSource;
	uint32_t nAuxiliary;
	uint64_t nImmediate;
	uint32_t iSourceNode;
} sirLangCIRInstruction_t;

typedef struct sirLangCIRFunction_s
{
	uint64_t nFunctionId;
	uint32_t iFirstInstruction;
	uint32_t nInstructionCount;
	uint32_t nRegisterCount;
	uint32_t nParameterCount;
	uint32_t iFirstParameterType;
	sirLangValueKind_t eReturnType;
} sirLangCIRFunction_t;

typedef struct sirLangCIRCapability_s
{
	uint64_t nFunctionId;
	uint64_t nSignatureId;
	uint32_t nBindingRevision;
	uint32_t nOwnerLane;
	uint32_t nEffects;
	uint32_t iFirstParameterType;
	uint32_t nParameterCount;
	sirLangValueKind_t eReturnType;
} sirLangCIRCapability_t;

typedef struct sirLangCIRProgram_s
{
	const sirLangCIRFunction_t *pFunctions;
	uint32_t nFunctionCount;
	const sirLangCIRInstruction_t *pInstructions;
	uint32_t nInstructionCount;
	const sirLangValueKind_t *pParameterTypes;
	uint32_t nParameterTypeCount;
	const uint32_t *pCallArguments;
	uint32_t nCallArgumentCount;
	const sirLangCIRCapability_t *pCapabilities;
	uint32_t nCapabilityCount;
	uint64_t nSchemaId;
	uint64_t nLayoutDigest;
	uint64_t nBindingDigest;
	uint32_t nRevision;
} sirLangCIRProgram_t;

typedef struct sirLangVMCall_s
{
	uint64_t nFunctionId;
	uint64_t nSignatureId;
	uint32_t nBindingRevision;
	uint32_t nOwnerLane;
	uint32_t nEffects;
	const sirLangVMValue_t *pArguments;
	uint32_t nArgumentCount;
} sirLangVMCall_t;

typedef sirLangVMError_t ( *sirLangVMNativeCall_t )( const sirLangVMCall_t&, sirLangVMValue_t *, void * ) noexcept;

typedef struct sirLangVMInvocation_s
{
	uint64_t nFunctionId;
	const sirLangVMValue_t *pArguments;
	uint32_t nArgumentCount;
	uint64_t nFuel;
	sirLangVMNativeCall_t pInvokeNative;
	void *pUser;
	uint64_t nExpectedLayoutDigest;
	uint64_t nExpectedBindingDigest;
	uint32_t nOwnerLane;
} sirLangVMInvocation_t;

typedef struct sirLangVMRunResult_s
{
	sirLangVMError_t eError;
	sirLangVMValue_t result;
	uint64_t nFuelConsumed;
	uint32_t nInstruction;
	uint32_t nSourceNode;
} sirLangVMRunResult_t;

// Verifies all instructions (including unreachable ones) before execution.
// Interpreter enforces fuel, bounded call depth and typed register lifetimes.
sirLangVMError_t SIRLangVerifyCIR( const sirLangCIRProgram_t *pProgram ) noexcept;
sirLangVMRunResult_t SIRLangRunGraphVM( const sirLangCIRProgram_t *pProgram, const sirLangVMInvocation_t& invocation ) noexcept;

#endif // __SIRENGINE_SIRLANGGRAPHVM_H__
