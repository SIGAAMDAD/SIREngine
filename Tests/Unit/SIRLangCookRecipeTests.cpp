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

#include <Scripting/Language/SIRLangCookRecipe.h>

#include <assert.h>
#include <string.h>

static bool sameKey( const sirScriptCacheKey_t& a, const sirScriptCacheKey_t& b ) noexcept
{
	return memcmp( a.szDigest, b.szDigest, sizeof( a.szDigest ) ) == 0;
}

static void testCookKeyIdentity( void ) noexcept
{
	sirLangCookIdentity_t identity = {};
	identity.nModuleId = 100;
	identity.nSchemaId = 200;
	identity.nLayoutDigest = 300;
	identity.nBindingDigest = 400;
	identity.nTargetId = 500;
	identity.nToolchainId = 600;
	identity.szSourceDigest[0] = 1;
	sirScriptCacheKey_t first = {};
	sirScriptCacheKey_t repeated = {};
	assert( SIRLangMakeCookKey( identity, nullptr, 0, &first ) == SIR_SCRIPT_OK );
	assert( SIRLangMakeCookKey( identity, nullptr, 0, &repeated ) == SIR_SCRIPT_OK );
	assert( sameKey( first, repeated ) );

	identity.nBindingDigest = 401;
	assert( SIRLangMakeCookKey( identity, nullptr, 0, &repeated ) == SIR_SCRIPT_OK );
	assert( !sameKey( first, repeated ) );
	identity.nBindingDigest = 400;
	identity.nToolchainId = 601;
	assert( SIRLangMakeCookKey( identity, nullptr, 0, &repeated ) == SIR_SCRIPT_OK );
	assert( !sameKey( first, repeated ) );
	identity.nToolchainId = 600;

	sirLangCookImport_t imports[2] = {};
	imports[0].nModuleId = 10;
	imports[0].szDigest[0] = 7;
	imports[1].nModuleId = 20;
	imports[1].szDigest[0] = 8;
	assert( SIRLangMakeCookKey( identity, imports, 2, &repeated ) == SIR_SCRIPT_OK );
	assert( !sameKey( first, repeated ) );
	sirScriptCacheKey_t withImports = repeated;
	imports[1].szDigest[0] = 9;
	assert( SIRLangMakeCookKey( identity, imports, 2, &repeated ) == SIR_SCRIPT_OK );
	assert( !sameKey( withImports, repeated ) );

	imports[1].nModuleId = 10;
	assert( SIRLangMakeCookKey( identity, imports, 2, &repeated ) == SIR_SCRIPT_INVALID_ARGUMENT );
	imports[1].nModuleId = 5;
	assert( SIRLangMakeCookKey( identity, imports, 2, &repeated ) == SIR_SCRIPT_INVALID_ARGUMENT );
	imports[1].nModuleId = 100;
	assert( SIRLangMakeCookKey( identity, imports, 2, &repeated ) == SIR_SCRIPT_INVALID_ARGUMENT );
	assert( SIRLangMakeCookKey( identity, nullptr, 2, &repeated ) == SIR_SCRIPT_INVALID_ARGUMENT );
	assert( SIRLangMakeCookKey( identity, nullptr, 0, nullptr ) == SIR_SCRIPT_INVALID_ARGUMENT );
}

int main( void )
{
	testCookKeyIdentity();
	return 0;
}
