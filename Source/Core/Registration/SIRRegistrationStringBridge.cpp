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

#include <Core/Registration/SIRRegistrationStringBridge.h>
#include <string.h>

static bool sirRegistrationModuleNameAllowed( const char *pszName, size_t nLength ) noexcept
{
	const char *pszDot = static_cast<const char *>(memchr( pszName, '.', nLength ));
	if ( pszDot == nullptr ) {
		return false;
	}
	const size_t nPrefixBytes = static_cast<size_t>( pszDot - pszName );
	if ( nPrefixBytes == 3 && memcmp( pszName, "sir", 3 ) == 0 ) {
		return false;
	}
	if ( nPrefixBytes == 7 && memcmp( pszName, "console", 7 ) == 0 ) {
		return false;
	}
	return true;
}

SIRRegistrationResult sirRegistrationInternName( SIRStringPool& pool, const sirRegistrationNameRequest_t& request,
                                                   SIRStringAtom *pAtom ) noexcept
{
	if ( pAtom == nullptr ) {
		return SIRRegistrationResult::SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	pAtom->reset();
	if ( !pool.isInitialized() ) {
		return SIRRegistrationResult::SIR_REGISTRATION_NOT_INITIALIZED;
	}
	char szCanonical[SIR_REGISTRATION_MAX_NAME_BYTES + 1];
	memset( szCanonical, 0, sizeof( szCanonical ) );
	if ( !sirNormalizeConsoleName( request.pszName, request.nNameBytes, szCanonical, sizeof( szCanonical ) ) ) {
		return SIRRegistrationResult::SIR_REGISTRATION_INVALID_NAME;
	}
	if ( request.nOwnerId != 0 && !sirRegistrationModuleNameAllowed( szCanonical, request.nNameBytes ) ) {
		return SIRRegistrationResult::SIR_REGISTRATION_DENIED;
	}
	SIRStringAtom atom = pool.find( szCanonical, request.nNameBytes );
	if ( !atom ) {
		atom = pool.intern( szCanonical, request.nNameBytes );
	}
	if ( !atom ) {
		return SIRRegistrationResult::SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	*pAtom = atom;
	return SIRRegistrationResult::SIR_REGISTRATION_OK;
}

SIRRegistrationResult sirRegistrationFindName( const SIRStringPool& pool, const char *pszName, size_t nLength,
                                                 SIRStringAtom *pAtom ) noexcept
{
	if ( pAtom == nullptr ) {
		return SIRRegistrationResult::SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	pAtom->reset();
	if ( !pool.isInitialized() ) {
		return SIRRegistrationResult::SIR_REGISTRATION_NOT_INITIALIZED;
	}
	char szCanonical[SIR_REGISTRATION_MAX_NAME_BYTES + 1];
	memset( szCanonical, 0, sizeof( szCanonical ) );
	if ( !sirNormalizeConsoleName( pszName, nLength, szCanonical, sizeof( szCanonical ) ) ) {
		return SIRRegistrationResult::SIR_REGISTRATION_INVALID_NAME;
	}
	const SIRStringAtom atom = pool.find( szCanonical, nLength );
	if ( !atom ) {
		return SIRRegistrationResult::SIR_REGISTRATION_NOT_FOUND;
	}
	*pAtom = atom;
	return SIRRegistrationResult::SIR_REGISTRATION_OK;
}

SIRRegistrationResult sirRegistrationCookedName( const SIRStringPool& pool, uint32_t nOrdinal, uint64_t nCatalogHash,
                                                   SIRStringAtom *pAtom ) noexcept
{
	if ( pAtom == nullptr ) {
		return SIRRegistrationResult::SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	pAtom->reset();
	if ( !pool.isInitialized() ) {
		return SIRRegistrationResult::SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( nCatalogHash == 0 ) {
		return SIRRegistrationResult::SIR_REGISTRATION_BAD_SCHEMA;
	}
	const SIRStringAtom atom = pool.atomFromCookedOrdinal( nOrdinal, nCatalogHash );
	if ( !atom ) {
		return SIRRegistrationResult::SIR_REGISTRATION_BAD_SCHEMA;
	}
	const uint32_t nLength = pool.getDecodedByteLength( atom );
	if ( nLength == 0 || nLength > SIR_REGISTRATION_MAX_NAME_BYTES ) {
		return SIRRegistrationResult::SIR_REGISTRATION_INVALID_NAME;
	}
	char szCanonical[SIR_REGISTRATION_MAX_NAME_BYTES + 1];
	memset( szCanonical, 0, sizeof( szCanonical ) );
	uint32_t nWritten = 0;
	if ( pool.decodeInto( atom, szCanonical, sizeof( szCanonical ), nWritten ) != SIRStringStatus::SIR_STRING_OK || nWritten != nLength ) {
		return SIRRegistrationResult::SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( !sirValidateConsoleName( szCanonical, nWritten ) ) {
		return SIRRegistrationResult::SIR_REGISTRATION_INVALID_NAME;
	}
	for ( uint32_t iByte = 0; iByte < nWritten; ++iByte ) {
		if ( szCanonical[iByte] >= 'A' && szCanonical[iByte] <= 'Z' ) {
			return SIRRegistrationResult::SIR_REGISTRATION_INVALID_NAME;
		}
	}
	*pAtom = atom;
	return SIRRegistrationResult::SIR_REGISTRATION_OK;
}
