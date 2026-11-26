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

#ifndef __SIR_SCRIPTBINDINGREGISTRY_H__
#define __SIR_SCRIPTBINDINGREGISTRY_H__

#include <Core/SIRPCH.h>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Scripting/Public/SIRScriptTypes.h>

enum class SIRScriptTypeFlags : uint32_t {
	SIR_SCRIPT_TYPE_PLAIN_BYTES = 1U << 0,
	SIR_SCRIPT_TYPE_NO_POINTERS = 1U << 1
};

typedef struct sirScriptTypeDescriptor_s
{
	uint64_t nTypeId;
	uint32_t nBytes;
	uint32_t nAlignment;
	uint32_t nFlags;
} sirScriptTypeDescriptor_t;

typedef struct sirScriptFunctionDescriptor_s
{
	uint64_t nFunctionId;
	uint64_t nSignatureId;
	uint64_t nArgumentsTypeId; // zero identifies the empty argument frame
	uint64_t nReturnTypeId;    // zero identifies the void return frame
	uint32_t nOwnerLane;
	uint32_t nBindingRevision;
	uint32_t nEffects;
} sirScriptFunctionDescriptor_t;

typedef struct sirScriptEventDescriptor_s
{
	uint64_t nEventTypeId;
	uint64_t nPayloadTypeId; // zero identifies an empty event payload
} sirScriptEventDescriptor_t;

typedef SIRScriptResult ( *sirScriptNativeThunk_t )( void *, sirScriptReadFrame_t, sirScriptWriteFrame_t ) noexcept;

typedef struct sirScriptFunctionEntry_s
{
	sirScriptFunctionDescriptor_t descriptor;
	sirScriptNativeThunk_t pInvoke;
	void *pUser;
} sirScriptFunctionEntry_t;

// Typed request shared by GraphVM and native host dispatch. Descriptor
// layout, effects and revision remain owned by the published registry.
typedef struct sirScriptNativeInvocation_s
{
	uint64_t nFunctionId;
	uint64_t nSignatureId;
	uint32_t nBindingRevision;
	sirScriptReadFrame_t arguments;
	sirScriptWriteFrame_t result;
} sirScriptNativeInvocation_t;

class SIRScriptBindingRegistry
{
public:
	SIRScriptBindingRegistry( void ) noexcept;

	SIRScriptResult registerType( const sirScriptTypeDescriptor_t& descriptor ) noexcept;
	SIRScriptResult registerFunction( const sirScriptFunctionDescriptor_t& descriptor, sirScriptNativeThunk_t pInvoke,
	                                    void *pUser ) noexcept;
	SIRScriptResult registerEvent( const sirScriptEventDescriptor_t& descriptor ) noexcept;
	SIRScriptResult publish( void ) noexcept;
	bool isPublished( void ) const noexcept;
	uint64_t schemaFingerprint( void ) const noexcept;
	bool describeFunction( uint64_t nFunctionId, sirScriptFunctionDescriptor_t *pDescriptor ) const noexcept;
	bool hasEvent( uint64_t nEventTypeId ) const noexcept;
	SIRScriptResult validateEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload ) const noexcept;
	SIRScriptResult validateQueuedEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload ) const noexcept;
	SIRScriptResult validateQueuedFrame( uint64_t nTypeId, sirScriptReadFrame_t frame ) const noexcept;
	SIRScriptResult invokeNative( const sirScriptNativeInvocation_t& request ) const noexcept;

private:
	static constexpr uint32_t MAX_TYPES = 64;
	static constexpr uint32_t MAX_FUNCTIONS = 128;
	static constexpr uint32_t MAX_EVENTS = 64;

	const sirScriptTypeDescriptor_t *findType( uint64_t nTypeId ) const noexcept;
	static bool isValidAlignment( uint32_t nAlignment ) noexcept;
	static bool checkReadFrame( sirScriptReadFrame_t frame, uint32_t nBytes, uint32_t nAlignment ) noexcept;
	static bool checkWriteFrame( sirScriptWriteFrame_t frame, uint32_t nBytes, uint32_t nAlignment ) noexcept;

	sirScriptTypeDescriptor_t m_szTypes[MAX_TYPES];
	sirScriptFunctionEntry_t m_szFunctions[MAX_FUNCTIONS];
	sirScriptEventDescriptor_t m_szEvents[MAX_EVENTS];
	uint32_t m_nTypes;
	uint32_t m_nFunctions;
	uint32_t m_nEvents;
	uint64_t m_nSchemaFingerprint;
	bool m_bPublished;
};

#endif // __SIR_SCRIPTBINDINGREGISTRY_H__
