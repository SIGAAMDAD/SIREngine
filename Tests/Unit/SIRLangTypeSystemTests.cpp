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
#include <assert.h>
#include <stdio.h>

static void testTypeIdentitiesAndLayout( void ) noexcept
{
	sirLangTypeDescriptor_t szTypes[64] = {};
	sirLangTypeTable_t table = { szTypes, 64, 0, 1234 };
	uint32_t iU16 = UINT32_MAX;
	uint32_t iU32 = UINT32_MAX;
	uint32_t iType = UINT32_MAX;
	assert( SIRLangRegisterScalar( &table, SIR_LANG_VALUE_U16, &iU16 ) == SIR_LANG_TYPE_OK );
	assert( SIRLangRegisterScalar( &table, SIR_LANG_VALUE_U32, &iU32 ) == SIR_LANG_TYPE_OK );
	assert( SIRLangRegisterScalar( &table, SIR_LANG_VALUE_U16, &iType ) == SIR_LANG_TYPE_OK );
	assert( iType == iU16 );
	assert( table.nCount == 2 );
	assert( !SIRLangSameType( &table, iU16, iU32 ) );
	assert( SIRLangGetType( &table, iU16 )->nAlignment == 2 );
	assert( SIRLangRegisterNominal( &table, { 77, 32, 8 }, &iType ) == SIR_LANG_TYPE_OK );
	const uint64_t nRecordType = table.pTypes[iType].nTypeId;
	assert( nRecordType == SIRLangStableTypeId( 1234, 77, SIR_LANG_ABI_REVISION ) );
	assert( SIRLangRegisterNominal( &table, { 77, 16, 8 }, &iType ) == SIR_LANG_TYPE_DUPLICATE_ID );
	assert( SIRLangRegisterNominal( &table, { 77, 20, 8 }, &iType ) == SIR_LANG_TYPE_INVALID );
	const uint32_t szArgs[2] = { iU16, iU32 };
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_RESULT, { szArgs, 2, 0 }, &iType ) == SIR_LANG_TYPE_OK );
	assert( table.pTypes[iType].nBytes >= 8 );
	uint32_t iSecond = UINT32_MAX;
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_RESULT, { szArgs, 2, 0 }, &iSecond ) == SIR_LANG_TYPE_OK );
	assert( iSecond == iType );
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_STACK_LIST, { szArgs, 1, 10 }, &iSecond ) == SIR_LANG_TYPE_OK );
	assert( table.pTypes[iSecond].nBytes >= 24 );
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_SLICE, { szArgs, 1, 0 }, &iSecond ) == SIR_LANG_TYPE_OK );
	assert( table.pTypes[iSecond].nFlags & SIR_LANG_TYPE_FLAG_BORROWED );
	const uint32_t iBorrowed = iSecond;
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_OPTION, { &iBorrowed, 1, 0 }, &iSecond ) == SIR_LANG_TYPE_INVALID_ARGUMENT );
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_ARRAY, { szArgs, 1, UINT32_MAX }, &iSecond ) == SIR_LANG_TYPE_OVERFLOW );
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_OPTION, { szArgs, 2, 0 }, &iSecond ) == SIR_LANG_TYPE_INVALID_ARGUMENT );
	assert( SIRLangConstructType( &table, SIR_LANG_TYPE_OPTION, { szArgs, 1, 0 }, &iSecond ) == SIR_LANG_TYPE_OK );
	assert( SIRLangRegisterScalar( nullptr, SIR_LANG_VALUE_I8, &iSecond ) == SIR_LANG_TYPE_INVALID );
	assert( SIRLangRegisterScalar( &table, SIR_LANG_VALUE_UNKNOWN, &iSecond ) == SIR_LANG_TYPE_INVALID );
	assert( SIRLangGetType( &table, UINT32_MAX ) == nullptr );
}

int main( void )
{
	testTypeIdentitiesAndLayout();
	puts( "SIR-Lang canonical type system tests passed" );
	return 0;
}
