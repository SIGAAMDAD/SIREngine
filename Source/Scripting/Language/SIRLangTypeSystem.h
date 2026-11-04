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

#ifndef __SIRENGINE_SIRLANGTYPESYSTEM_H__
#define __SIRENGINE_SIRLANGTYPESYSTEM_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Scripting/Language/SIRLangSignatures.h>

constexpr uint32_t SIR_LANG_ABI_REVISION = 1;
constexpr uint32_t SIR_LANG_MAX_TYPE_ARGUMENTS = 8;

typedef enum sirLangTypeCategory_e : uint8_t
{
	SIR_LANG_TYPE_SCALAR = 0,
	SIR_LANG_TYPE_NOMINAL,
	SIR_LANG_TYPE_ARRAY,
	SIR_LANG_TYPE_STACK_LIST,
	SIR_LANG_TYPE_SLICE,
	SIR_LANG_TYPE_OPTION,
	SIR_LANG_TYPE_RESULT,
	SIR_LANG_TYPE_VARIANT,
	SIR_LANG_TYPE_HANDLE,
	SIR_LANG_TYPE_VECTOR
} sirLangTypeCategory_t;

typedef enum sirLangTypeError_e : uint8_t
{
	SIR_LANG_TYPE_OK = 0,
	SIR_LANG_TYPE_INVALID,
	SIR_LANG_TYPE_CAPACITY,
	SIR_LANG_TYPE_DUPLICATE_ID,
	SIR_LANG_TYPE_MISSING_ARGUMENT,
	SIR_LANG_TYPE_INVALID_ARGUMENT,
	SIR_LANG_TYPE_OVERFLOW,
	SIR_LANG_TYPE_UNSUPPORTED_LAYOUT
} sirLangTypeError_t;

typedef enum sirLangTypeFlags_e : uint32_t
{
	SIR_LANG_TYPE_FLAG_PLAIN_VALUE = 1U,
	SIR_LANG_TYPE_FLAG_BORROWED = 2U,
	SIR_LANG_TYPE_FLAG_SERIALIZABLE = 4U,
	SIR_LANG_TYPE_FLAG_RUNTIME_OWNED = 8U
} sirLangTypeFlags_t;

typedef struct sirLangTypeDescriptor_s
{
	uint64_t nTypeId;
	uint64_t nSchemaId;
	uint32_t nBytes;
	uint32_t nAlignment;
	uint32_t nFlags;
	uint32_t nCapacity;
	uint32_t szArgumentTypes[SIR_LANG_MAX_TYPE_ARGUMENTS];
	uint8_t nArgumentCount;
	sirLangTypeCategory_t eCategory;
	sirLangValueKind_t eScalar;
} sirLangTypeDescriptor_t;

typedef struct sirLangNominalLayout_s
{
	uint64_t nSchemaId;
	uint32_t nBytes;
	uint32_t nAlignment;
} sirLangNominalLayout_t;

typedef struct sirLangTypeArguments_s
{
	const uint32_t *pTypes;
	uint32_t nCount;
	uint32_t nCapacity;
} sirLangTypeArguments_t;

typedef struct sirLangTypeTable_s
{
	sirLangTypeDescriptor_t *pTypes;
	uint32_t nCapacity;
	uint32_t nCount;
	uint64_t nModuleId;
} sirLangTypeTable_t;

// Identifiers use a versioned, domain-separated stable encoding. Nominal
// type IDs include the host-authored schema identity, never source offsets.
uint64_t SIRLangStableTypeId( uint64_t nModuleId, uint64_t nSchemaId, uint32_t nRevision ) noexcept;
sirLangTypeError_t SIRLangRegisterScalar( sirLangTypeTable_t *pTable, sirLangValueKind_t eScalar, uint32_t *pIndex ) noexcept;
sirLangTypeError_t SIRLangRegisterNominal( sirLangTypeTable_t *pTable, const sirLangNominalLayout_t& layout, uint32_t *pIndex ) noexcept;
sirLangTypeError_t SIRLangConstructType( sirLangTypeTable_t *pTable, sirLangTypeCategory_t eCategory,
                                         const sirLangTypeArguments_t& arguments, uint32_t *pIndex ) noexcept;
bool SIRLangSameType( const sirLangTypeTable_t *pTable, uint32_t iFirst, uint32_t iSecond ) noexcept;
const sirLangTypeDescriptor_t *SIRLangGetType( const sirLangTypeTable_t *pTable, uint32_t iType ) noexcept;

#endif // __SIRENGINE_SIRLANGTYPESYSTEM_H__
