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

#include <Scripting/Language/SIRLangTypeSystem.h>
#include <limits.h>
#include <string.h>

static uint64_t sirLangHashWord( uint64_t nSeed, uint64_t nWord ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nSeed ^= ( nWord >> ( iByte * 8 ) ) & 255U;
		nSeed *= UINT64_C( 1099511628211 );
	}
	return nSeed;
}

uint64_t SIRLangStableTypeId( uint64_t nModuleId, uint64_t nSchemaId, uint32_t nRevision ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	nHash = sirLangHashWord( nHash, UINT64_C( 0x5349524c54595045 ) );
	nHash = sirLangHashWord( nHash, nModuleId );
	nHash = sirLangHashWord( nHash, nSchemaId );
	nHash = sirLangHashWord( nHash, nRevision );
	return nHash == 0 ? 1 : nHash;
}

static bool sirLangValidAlignment( uint32_t nAlignment ) noexcept
{
	return nAlignment > 0 && nAlignment <= 64 && ( nAlignment & ( nAlignment - 1 ) ) == 0;
}

static sirLangTypeError_t sirLangAddType( sirLangTypeTable_t *pTable, const sirLangTypeDescriptor_t& descriptor, uint32_t *pIndex ) noexcept
{
	if ( pTable == nullptr || pIndex == nullptr || pTable->pTypes == nullptr ) {
		return SIR_LANG_TYPE_INVALID;
	}
	for ( uint32_t iType = 0; iType < pTable->nCount; ++iType ) {
		if ( pTable->pTypes[iType].nTypeId == descriptor.nTypeId ) {
			if ( memcmp( &pTable->pTypes[iType], &descriptor, sizeof( descriptor ) ) == 0 ) {
				*pIndex = iType;
				return SIR_LANG_TYPE_OK;
			}
			return SIR_LANG_TYPE_DUPLICATE_ID;
		}
	}
	if ( pTable->nCount >= pTable->nCapacity ) {
		return SIR_LANG_TYPE_CAPACITY;
	}
	*pIndex = pTable->nCount;
	pTable->pTypes[pTable->nCount++] = descriptor;
	return SIR_LANG_TYPE_OK;
}

sirLangTypeError_t SIRLangRegisterScalar( sirLangTypeTable_t *pTable, sirLangValueKind_t eScalar, uint32_t *pIndex ) noexcept
{
	if ( eScalar <= SIR_LANG_VALUE_UNKNOWN || eScalar >= SIR_LANG_VALUE_OTHER ) {
		return SIR_LANG_TYPE_INVALID;
	}
	uint32_t nBytes = 0;
	switch ( eScalar ) {
	case SIR_LANG_VALUE_VOID:
		nBytes = 0;
		break;
	case SIR_LANG_VALUE_BOOL:
	case SIR_LANG_VALUE_I8:
	case SIR_LANG_VALUE_U8:
		nBytes = 1;
		break;
	case SIR_LANG_VALUE_I16:
	case SIR_LANG_VALUE_U16:
	case SIR_LANG_VALUE_F16:
		nBytes = 2;
		break;
	case SIR_LANG_VALUE_I32:
	case SIR_LANG_VALUE_U32:
	case SIR_LANG_VALUE_F32:
		nBytes = 4;
		break;
	case SIR_LANG_VALUE_I64:
	case SIR_LANG_VALUE_U64:
	case SIR_LANG_VALUE_F64:
	case SIR_LANG_VALUE_ATOM:
		nBytes = 8;
		break;
	default:
		return SIR_LANG_TYPE_INVALID;
	}
	sirLangTypeDescriptor_t descriptor = {};
	descriptor.nTypeId = SIRLangStableTypeId( 0, ( uint64_t )eScalar, SIR_LANG_ABI_REVISION );
	descriptor.nBytes = nBytes;
	descriptor.nAlignment = nBytes == 0 ? 1 : nBytes;
	descriptor.nFlags = SIR_LANG_TYPE_FLAG_PLAIN_VALUE | SIR_LANG_TYPE_FLAG_SERIALIZABLE;
	descriptor.eCategory = SIR_LANG_TYPE_SCALAR;
	descriptor.eScalar = eScalar;
	return sirLangAddType( pTable, descriptor, pIndex );
}

sirLangTypeError_t SIRLangRegisterNominal( sirLangTypeTable_t *pTable, const sirLangNominalLayout_t& layout, uint32_t *pIndex ) noexcept
{
	const uint64_t nSchemaId = layout.nSchemaId;
	const uint32_t nBytes = layout.nBytes;
	const uint32_t nAlignment = layout.nAlignment;
	if ( pTable == nullptr || nSchemaId == 0 || !sirLangValidAlignment( nAlignment ) || nBytes % nAlignment != 0 ) {
		return SIR_LANG_TYPE_INVALID;
	}
	sirLangTypeDescriptor_t descriptor = {};
	descriptor.nTypeId = SIRLangStableTypeId( pTable->nModuleId, nSchemaId, SIR_LANG_ABI_REVISION );
	descriptor.nSchemaId = nSchemaId;
	descriptor.nBytes = nBytes;
	descriptor.nAlignment = nAlignment;
	descriptor.nFlags = SIR_LANG_TYPE_FLAG_PLAIN_VALUE | SIR_LANG_TYPE_FLAG_SERIALIZABLE;
	descriptor.eCategory = SIR_LANG_TYPE_NOMINAL;
	descriptor.eScalar = SIR_LANG_VALUE_OTHER;
	return sirLangAddType( pTable, descriptor, pIndex );
}

static bool sirLangValidArity( sirLangTypeCategory_t eCategory, uint32_t nArguments ) noexcept
{
	switch ( eCategory ) {
	case SIR_LANG_TYPE_ARRAY:
	case SIR_LANG_TYPE_STACK_LIST:
	case SIR_LANG_TYPE_SLICE:
	case SIR_LANG_TYPE_OPTION:
	case SIR_LANG_TYPE_HANDLE:
	case SIR_LANG_TYPE_VECTOR:
		return nArguments == 1;
	case SIR_LANG_TYPE_RESULT:
		return nArguments == 2;
	case SIR_LANG_TYPE_VARIANT:
		return nArguments >= 2 && nArguments <= SIR_LANG_MAX_TYPE_ARGUMENTS;
	default:
		return false;
	}
}

static bool sirLangMulFits( uint32_t nFirst, uint32_t nSecond, uint32_t *pResult ) noexcept
{
	if ( nSecond && nFirst > UINT32_MAX / nSecond ) {
		return false;
	}
	*pResult = nFirst * nSecond;
	return true;
}

static bool sirLangRoundUp( uint32_t nBytes, uint32_t nAlignment, uint32_t *pResult ) noexcept
{
	if ( nAlignment == 0 || nBytes > UINT32_MAX - ( nAlignment - 1 ) ) {
		return false;
	}
	*pResult = ( nBytes + nAlignment - 1 ) & ~( nAlignment - 1 );
	return true;
}

sirLangTypeError_t SIRLangConstructType( sirLangTypeTable_t *pTable, sirLangTypeCategory_t eCategory,
                                         const sirLangTypeArguments_t& arguments, uint32_t *pIndex ) noexcept
{
	const uint32_t *pArguments = arguments.pTypes;
	const uint32_t nArguments = arguments.nCount;
	const uint32_t nCapacity = arguments.nCapacity;
	if ( pTable == nullptr || pTable->pTypes == nullptr || pArguments == nullptr || pIndex == nullptr || nArguments == 0 ||
	     nArguments > SIR_LANG_MAX_TYPE_ARGUMENTS || !sirLangValidArity( eCategory, nArguments ) ) {
		return SIR_LANG_TYPE_INVALID_ARGUMENT;
	}
	sirLangTypeDescriptor_t descriptor = {};
	descriptor.eCategory = eCategory;
	descriptor.eScalar = SIR_LANG_VALUE_OTHER;
	descriptor.nArgumentCount = ( uint8_t )nArguments;
	descriptor.nCapacity = nCapacity;
	descriptor.nAlignment = 1;
	uint32_t nMaxSize = 0;
	for ( uint32_t iArgument = 0; iArgument < nArguments; ++iArgument ) {
		if ( pArguments[iArgument] >= pTable->nCount ) {
			return SIR_LANG_TYPE_MISSING_ARGUMENT;
		}
		const sirLangTypeDescriptor_t& argument = pTable->pTypes[pArguments[iArgument]];
		if ( argument.eScalar == SIR_LANG_VALUE_VOID || argument.nBytes == 0 || ( argument.nFlags & SIR_LANG_TYPE_FLAG_BORROWED ) ) {
			return SIR_LANG_TYPE_INVALID_ARGUMENT;
		}
		descriptor.szArgumentTypes[iArgument] = pArguments[iArgument];
		if ( argument.nAlignment > descriptor.nAlignment ) {
			descriptor.nAlignment = argument.nAlignment;
		}
		if ( argument.nBytes > nMaxSize ) {
			nMaxSize = argument.nBytes;
		}
	}
	const sirLangTypeDescriptor_t& first = pTable->pTypes[pArguments[0]];
	switch ( eCategory ) {
	case SIR_LANG_TYPE_ARRAY:
	case SIR_LANG_TYPE_VECTOR:
		if ( nCapacity == 0 || !sirLangMulFits( first.nBytes, nCapacity, &descriptor.nBytes ) ) {
			return SIR_LANG_TYPE_OVERFLOW;
		}
		descriptor.nFlags = SIR_LANG_TYPE_FLAG_PLAIN_VALUE | SIR_LANG_TYPE_FLAG_SERIALIZABLE;
		break;
	case SIR_LANG_TYPE_STACK_LIST:
		if ( nCapacity == 0 || !sirLangMulFits( first.nBytes, nCapacity, &descriptor.nBytes ) || descriptor.nBytes > UINT32_MAX - 4 ) {
			return SIR_LANG_TYPE_OVERFLOW;
		}
		descriptor.nBytes += 4;
		descriptor.nFlags = SIR_LANG_TYPE_FLAG_PLAIN_VALUE;
		break;
	case SIR_LANG_TYPE_SLICE:
		descriptor.nBytes = 16;
		descriptor.nAlignment = 8;
		descriptor.nFlags = SIR_LANG_TYPE_FLAG_BORROWED;
		break;
	case SIR_LANG_TYPE_HANDLE:
		descriptor.nBytes = 16;
		descriptor.nAlignment = 8;
		descriptor.nFlags = SIR_LANG_TYPE_FLAG_PLAIN_VALUE | SIR_LANG_TYPE_FLAG_SERIALIZABLE;
		break;
	case SIR_LANG_TYPE_OPTION:
	case SIR_LANG_TYPE_RESULT:
	case SIR_LANG_TYPE_VARIANT:
		if ( !sirLangRoundUp( nMaxSize, descriptor.nAlignment, &descriptor.nBytes ) ||
		     descriptor.nBytes > UINT32_MAX - descriptor.nAlignment ) {
			return SIR_LANG_TYPE_OVERFLOW;
		}
		descriptor.nBytes += descriptor.nAlignment;
		descriptor.nFlags = SIR_LANG_TYPE_FLAG_PLAIN_VALUE | SIR_LANG_TYPE_FLAG_SERIALIZABLE;
		break;
	default:
		return SIR_LANG_TYPE_INVALID_ARGUMENT;
	}
	uint32_t nAligned = 0;
	if ( !sirLangRoundUp( descriptor.nBytes, descriptor.nAlignment, &nAligned ) ) {
		return SIR_LANG_TYPE_OVERFLOW;
	}
	descriptor.nBytes = nAligned;
	uint64_t nHash = SIRLangStableTypeId( 0, ( uint64_t )eCategory, SIR_LANG_ABI_REVISION );
	nHash = sirLangHashWord( nHash, nCapacity );
	for ( uint32_t iArgument = 0; iArgument < nArguments; ++iArgument ) {
		nHash = sirLangHashWord( nHash, pTable->pTypes[pArguments[iArgument]].nTypeId );
	}
	descriptor.nTypeId = nHash;
	return sirLangAddType( pTable, descriptor, pIndex );
}

bool SIRLangSameType( const sirLangTypeTable_t *pTable, uint32_t iFirst, uint32_t iSecond ) noexcept
{
	if ( pTable == nullptr || pTable->pTypes == nullptr || iFirst >= pTable->nCount || iSecond >= pTable->nCount ) {
		return false;
	}
	return pTable->pTypes[iFirst].nTypeId == pTable->pTypes[iSecond].nTypeId;
}

const sirLangTypeDescriptor_t *SIRLangGetType( const sirLangTypeTable_t *pTable, uint32_t iType ) noexcept
{
	if ( pTable == nullptr || pTable->pTypes == nullptr || iType >= pTable->nCount ) {
		return nullptr;
	}
	return &pTable->pTypes[iType];
}
