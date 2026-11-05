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

#include <Core/Registration/SIRRegistrationCatalog.h>
#include <string.h>

static uint32_t sirCatalogU32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

static uint64_t sirCatalogU64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirCatalogU32( pBytes ) | ( ( uint64_t )sirCatalogU32( pBytes + 4 ) << 32 );
}

static void sirCatalogSort32( uint32_t *pValues, uint32_t nCount ) noexcept
{
	for ( uint32_t nGap = nCount / 2; nGap != 0; nGap /= 2 ) {
		for ( uint32_t iValue = nGap; iValue < nCount; ++iValue ) {
			const uint32_t nValue = pValues[iValue];
			uint32_t iTarget = iValue;
			while ( iTarget >= nGap && pValues[iTarget - nGap] > nValue ) {
				pValues[iTarget] = pValues[iTarget - nGap];
				iTarget -= nGap;
			}
			pValues[iTarget] = nValue;
		}
	}
}

static void sirCatalogSort64( uint64_t *pValues, uint32_t nCount ) noexcept
{
	for ( uint32_t nGap = nCount / 2; nGap != 0; nGap /= 2 ) {
		for ( uint32_t iValue = nGap; iValue < nCount; ++iValue ) {
			const uint64_t nValue = pValues[iValue];
			uint32_t iTarget = iValue;
			while ( iTarget >= nGap && pValues[iTarget - nGap] > nValue ) {
				pValues[iTarget] = pValues[iTarget - nGap];
				iTarget -= nGap;
			}
			pValues[iTarget] = nValue;
		}
	}
}

static bool sirCatalogAllowedModuleName( SIRStringPool& pool, SIRStringAtom atom ) noexcept
{
	const uint32_t nLength = pool.getDecodedByteLength( atom );
	if ( nLength < 3 || nLength > SIR_REGISTRATION_MAX_NAME_BYTES ) {
		return false;
	}
	char szName[SIR_REGISTRATION_MAX_NAME_BYTES + 1] = {};
	uint32_t nWritten = 0;
	if ( pool.decodeInto( atom, szName, sizeof( szName ), nWritten ) != SIR_STRING_OK ) {
		return false;
	}
	const char *pszDot = ( const char * )memchr( szName, '.', nWritten );
	if ( pszDot == nullptr ) {
		return false;
	}
	const size_t nPrefix = ( size_t )( pszDot - szName );
	return !( nPrefix == 3 && memcmp( szName, "sir", 3 ) == 0 ) && !( nPrefix == 7 && memcmp( szName, "console", 7 ) == 0 );
}

static bool sirCatalogUtf8( const uint8_t *pBytes, uint32_t nCount ) noexcept
{
	for ( uint32_t iByte = 0; iByte < nCount; ) {
		const uint8_t nFirst = pBytes[iByte];
		if ( nFirst < 0x80 ) {
			++iByte;
			continue;
		}
		uint32_t nCodePoint = 0;
		uint32_t nContinuation = 0;
		if ( nFirst >= 0xc2 && nFirst <= 0xdf ) {
			nCodePoint = nFirst & 0x1f;
			nContinuation = 1;
		} else if ( nFirst >= 0xe0 && nFirst <= 0xef ) {
			nCodePoint = nFirst & 0x0f;
			nContinuation = 2;
		} else if ( nFirst >= 0xf0 && nFirst <= 0xf4 ) {
			nCodePoint = nFirst & 7;
			nContinuation = 3;
		} else {
			return false;
		}
		if ( nContinuation > nCount - iByte - 1 ) {
			return false;
		}
		for ( uint32_t iPart = 0; iPart < nContinuation; ++iPart ) {
			const uint8_t nPart = pBytes[iByte + iPart + 1];
			if ( ( nPart & 0xc0 ) != 0x80 ) {
				return false;
			}
			nCodePoint = ( nCodePoint << 6 ) | ( nPart & 0x3f );
		}
		if ( ( nContinuation == 2 && nCodePoint < 0x800 ) || ( nContinuation == 3 && nCodePoint < 0x10000 ) ) {
			return false;
		}
		if ( ( nCodePoint >= 0xd800 && nCodePoint <= 0xdfff ) || nCodePoint > 0x10ffff ) {
			return false;
		}
		iByte += nContinuation + 1;
	}
	return true;
}

static bool sirCatalogDefaultValid( const sirRegistrationCatalogInit_t& init, uint32_t nOffset, uint32_t nType,
                                    uint64_t nDefaultsStart ) noexcept
{
	if ( nOffset < nDefaultsStart || ( nOffset & 7u ) != 0 || nOffset > init.nBytes ) {
		return false;
	}
	if ( nType == SIR_CVAR_UTF8_STRING ) {
		if ( init.nBytes - nOffset < 4 ) {
			return false;
		}
		const uint32_t nLength = sirCatalogU32( init.pBytes + nOffset );
		return nLength <= init.nBytes - nOffset - 4 && sirCatalogUtf8( init.pBytes + nOffset + 4, nLength );
	}
	if ( init.nBytes - nOffset < 8 ) {
		return false;
	}
	const uint64_t nBits = sirCatalogU64( init.pBytes + nOffset );
	if ( nType == SIR_CVAR_BOOL ) {
		return nBits <= 1;
	}
	if ( nType == SIR_CVAR_I32 || nType == SIR_CVAR_U32 || nType == SIR_CVAR_F32 || nType == SIR_CVAR_ENUM_U32 ) {
		return ( nBits >> 32 ) == 0;
	}
	return true;
}

SIRRegistrationCatalog::SIRRegistrationCatalog( void ) noexcept
    : m_pPool( nullptr ), m_pEntries( nullptr ), m_pNames( nullptr ), m_nCapacity( 0 ), m_nCookedCount( 0 ), m_nLiveCount( 0 ),
      m_nGenerationSeed( 1 ), m_bInitialized( false )
{
}

SIRRegistrationCatalog::~SIRRegistrationCatalog( void ) noexcept
{
	shutdown();
}

bool SIRRegistrationCatalog::isInitialized( void ) const noexcept
{
	return m_bInitialized;
}

uint32_t SIRRegistrationCatalog::getCount( void ) const noexcept
{
	return m_nLiveCount;
}

uint32_t SIRRegistrationCatalog::getCapacity( void ) const noexcept
{
	return m_nCapacity;
}

uint32_t SIRRegistrationCatalog::getCookedCount( void ) const noexcept
{
	return m_nCookedCount;
}

sirRegistrationResult_t SIRRegistrationCatalog::getEntry( sirRegistrationHandle_t handle,
                                                          const sirRegistrationEntry_t **ppEntry ) const noexcept
{
	if ( ppEntry == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*ppEntry = nullptr;
	if ( !m_bInitialized ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( handle.nSlot == 0 || handle.nSlot > m_nCapacity ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	const sirRegistrationEntry_t& entry = m_pEntries[handle.nSlot - 1];
	if ( !entry.bActive || entry.nGeneration != handle.nGeneration ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	*ppEntry = &entry;
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRRegistrationCatalog::getAtIndex( uint32_t iEntry, const sirRegistrationEntry_t **ppEntry,
                                                            sirRegistrationHandle_t *pHandle ) const noexcept
{
	if ( ppEntry == nullptr || pHandle == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*ppEntry = nullptr;
	*pHandle = {};
	if ( !m_bInitialized ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( iEntry >= m_nCapacity ) {
		return SIR_REGISTRATION_BAD_BOUNDS;
	}
	const sirRegistrationEntry_t& entry = m_pEntries[iEntry];
	if ( !entry.bActive ) {
		return SIR_REGISTRATION_NOT_FOUND;
	}
	*ppEntry = &entry;
	*pHandle = { iEntry + 1, entry.nGeneration };
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRRegistrationCatalog::loadNames( const sirRegistrationCatalogInit_t& init, SIRStringPool& pool,
                                                           uint32_t nNames ) noexcept
{
	const uint64_t nOffset = sirCatalogU64( init.pBytes + 24 );
	uint32_t nPreviousOrdinal = 0;
	for ( uint32_t iName = 0; iName < nNames; ++iName ) {
		const uint8_t *pRecord = init.pBytes + nOffset + ( size_t )iName * SIR_REGISTRATION_NAME_STRIDE;
		const uint32_t nOrdinal = sirCatalogU32( pRecord );
		const uint32_t nStableId = sirCatalogU32( pRecord + 4 );
		if ( nStableId == 0 || ( iName != 0 && nOrdinal <= nPreviousOrdinal ) ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		nPreviousOrdinal = nOrdinal;
		sirRegistrationResult_t eResult = sirRegistrationCookedName( pool, nOrdinal, init.nStringCatalogHash, &init.pNames[iName] );
		if ( eResult != SIR_REGISTRATION_OK ) {
			return eResult;
		}
		init.pNameIds[iName] = nStableId;
	}
	sirCatalogSort32( init.pNameIds, nNames );
	for ( uint32_t iName = 1; iName < nNames; ++iName ) {
		if ( init.pNameIds[iName] == init.pNameIds[iName - 1] ) {
			return SIR_REGISTRATION_DUPLICATE_NAME;
		}
	}
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRRegistrationCatalog::loadDescriptors( const sirRegistrationCatalogInit_t& init, uint32_t nNames ) noexcept
{
	const uint32_t nCommands = sirCatalogU32( init.pBytes + 48 );
	const uint32_t nCVars = sirCatalogU32( init.pBytes + 64 );
	const uint64_t nCommandsOffset = sirCatalogU64( init.pBytes + 40 );
	const uint64_t nCVarsOffset = sirCatalogU64( init.pBytes + 56 );
	const uint64_t nScalabilityOffset = sirCatalogU64( init.pBytes + 72 );
	const uint32_t nScalabilityCount = sirCatalogU32( init.pBytes + 80 );
	const uint64_t nEndCommands = nCommandsOffset + ( uint64_t )nCommands * SIR_REGISTRATION_COMMAND_STRIDE;
	const uint64_t nEndCVars = nCVarsOffset + ( uint64_t )nCVars * SIR_REGISTRATION_CVAR_STRIDE;
	const uint64_t nEndScalability = nScalabilityOffset + ( uint64_t )nScalabilityCount * SIR_REGISTRATION_SCALABILITY_STRIDE;
	uint64_t nDefaultsStart = nEndCommands > nEndCVars ? nEndCommands : nEndCVars;
	if ( nDefaultsStart < nEndScalability ) {
		nDefaultsStart = nEndScalability;
	}
	if ( nDefaultsStart < SIR_REGISTRATION_CATALOG_HEADER_BYTES ) {
		nDefaultsStart = SIR_REGISTRATION_CATALOG_HEADER_BYTES;
	}
	nDefaultsStart = ( nDefaultsStart + 7 ) & ~7ULL;

	uint32_t nPreviousName = 0;
	for ( uint32_t iCommand = 0; iCommand < nCommands; ++iCommand ) {
		const uint8_t *pRecord = init.pBytes + nCommandsOffset + ( size_t )iCommand * SIR_REGISTRATION_COMMAND_STRIDE;
		const uint32_t nNameIndex = sirCatalogU32( pRecord );
		const uint32_t nHelpIndex = sirCatalogU32( pRecord + 4 );
		const uint64_t nBindingId = sirCatalogU64( pRecord + 8 );
		const uint32_t nFlags = sirCatalogU32( pRecord + 16 );
		const uint32_t nCapabilities = sirCatalogU32( pRecord + 20 );
		const uint32_t nOwnerId = sirCatalogU32( pRecord + 24 );
		if ( nNameIndex >= nNames || ( iCommand != 0 && nNameIndex <= nPreviousName ) ) {
			return SIR_REGISTRATION_DUPLICATE_NAME;
		}
		if ( nHelpIndex != UINT32_MAX && nHelpIndex >= nNames ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( nBindingId == 0 || nFlags != 0 || ( nCapabilities & ~511u ) != 0 ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( nOwnerId != 0 && !sirCatalogAllowedModuleName( *m_pPool, init.pNames[nNameIndex] ) ) {
			return SIR_REGISTRATION_DENIED;
		}
		nPreviousName = nNameIndex;
		const sirRegistrationBinding_t *pBinding = nullptr;
		uint32_t nLower = 0;
		uint32_t nUpper = init.nBindingCount;
		while ( nLower < nUpper ) {
			const uint32_t iMid = nLower + ( nUpper - nLower ) / 2;
			if ( init.pBindings[iMid].nBindingId < nBindingId ) {
				nLower = iMid + 1;
			} else {
				nUpper = iMid;
			}
		}
		if ( nLower < init.nBindingCount && init.pBindings[nLower].nBindingId == nBindingId ) {
			pBinding = init.pBindings + nLower;
		}
		if ( pBinding == nullptr || pBinding->pfnCallback == nullptr || pBinding->nOwnerId != nOwnerId ) {
			return SIR_REGISTRATION_UNBOUND_CALLBACK;
		}
		sirRegistrationEntry_t entry = {};
		entry.atom = init.pNames[nNameIndex];
		entry.nBindingId = nBindingId;
		entry.nSchemaId = sirCatalogU32( pRecord + 28 );
		entry.nOwnerId = nOwnerId;
		entry.nCapabilities = nCapabilities;
		entry.nGeneration = m_nGenerationSeed;
		entry.pfnCallback = pBinding->pfnCallback;
		entry.pContext = pBinding->pContext;
		entry.eKind = SIR_REGISTRATION_COMMAND;
		entry.bActive = true;
		entry.bCooked = true;
		init.pEntries[iCommand] = entry;
		init.pBindingIds[iCommand] = nBindingId;
	}
	sirCatalogSort64( init.pBindingIds, nCommands );
	for ( uint32_t iCommand = 1; iCommand < nCommands; ++iCommand ) {
		if ( init.pBindingIds[iCommand] == init.pBindingIds[iCommand - 1] ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
	}

	nPreviousName = 0;
	for ( uint32_t iCVar = 0; iCVar < nCVars; ++iCVar ) {
		const uint8_t *pRecord = init.pBytes + nCVarsOffset + ( size_t )iCVar * SIR_REGISTRATION_CVAR_STRIDE;
		const uint32_t nNameIndex = sirCatalogU32( pRecord );
		const uint32_t nHelpIndex = sirCatalogU32( pRecord + 4 );
		const uint32_t nSchemaId = sirCatalogU32( pRecord + 8 );
		const uint32_t nType = sirCatalogU32( pRecord + 12 );
		const uint32_t nFlags = sirCatalogU32( pRecord + 16 );
		const uint32_t nDefaultOffset = sirCatalogU32( pRecord + 20 );
		const uint32_t nOwnerId = sirCatalogU32( pRecord + 24 );
		if ( nNameIndex >= nNames || ( iCVar != 0 && nNameIndex <= nPreviousName ) ) {
			return SIR_REGISTRATION_DUPLICATE_NAME;
		}
		if ( nSchemaId == 0 || ( nHelpIndex != UINT32_MAX && nHelpIndex >= nNames ) ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( nType >= SIR_CVAR_TYPE_COUNT || !sirValidateCVarFlags( nFlags ) ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( !sirCatalogDefaultValid( init, nDefaultOffset, nType, nDefaultsStart ) ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( nOwnerId != 0 && !sirCatalogAllowedModuleName( *m_pPool, init.pNames[nNameIndex] ) ) {
			return SIR_REGISTRATION_DENIED;
		}
		nPreviousName = nNameIndex;
		sirRegistrationEntry_t entry = {};
		entry.atom = init.pNames[nNameIndex];
		entry.nSchemaId = nSchemaId;
		entry.nType = nType;
		entry.nFlags = nFlags;
		entry.nOwnerId = nOwnerId;
		entry.nDefaultOffset = nDefaultOffset;
		entry.nGeneration = m_nGenerationSeed;
		entry.eKind = SIR_REGISTRATION_CVAR;
		entry.bActive = true;
		entry.bCooked = true;
		init.pEntries[nCommands + iCVar] = entry;
		init.pNameIds[iCVar] = nSchemaId;
	}
	sirCatalogSort32( init.pNameIds, nCVars );
	for ( uint32_t iCVar = 1; iCVar < nCVars; ++iCVar ) {
		if ( init.pNameIds[iCVar] == init.pNameIds[iCVar - 1] ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
	}
	uint32_t iCommand = 0;
	uint32_t iCVar = 0;
	while ( iCommand < nCommands && iCVar < nCVars ) {
		const uint32_t nCommandName = sirCatalogU32( init.pBytes + nCommandsOffset + ( size_t )iCommand * 32 );
		const uint32_t nCVarName = sirCatalogU32( init.pBytes + nCVarsOffset + ( size_t )iCVar * 32 );
		if ( nCommandName == nCVarName ) {
			return SIR_REGISTRATION_DUPLICATE_NAME;
		}
		if ( nCommandName < nCVarName ) {
			++iCommand;
		} else {
			++iCVar;
		}
	}
	return nScalabilityCount == 0 ? SIR_REGISTRATION_OK : SIR_REGISTRATION_BAD_SCHEMA;
}

sirRegistrationResult_t SIRRegistrationCatalog::initialize( SIRStringPool& pool, const sirRegistrationCatalogInit_t& init ) noexcept
{
	if ( m_bInitialized ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( !pool.isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( init.pBytes == nullptr || init.pEntries == nullptr || init.pNames == nullptr || init.pNameIds == nullptr ||
	     init.nStringCatalogHash == 0 ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	const sirRegistrationResult_t eHeader = sirValidateRegistrationCatalog( init.pBytes, init.nBytes, init.nStringCatalogHash );
	if ( eHeader != SIR_REGISTRATION_OK ) {
		return eHeader;
	}
	const uint32_t nNames = sirCatalogU32( init.pBytes + 32 );
	const uint32_t nCommands = sirCatalogU32( init.pBytes + 48 );
	const uint32_t nCVars = sirCatalogU32( init.pBytes + 64 );
	if ( nNames > init.nNameCapacity || nCommands + nCVars > init.nEntryCapacity || nCommands > init.nBindingCapacity ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	if ( nCommands != 0 && ( init.pBindingIds == nullptr || init.pBindings == nullptr ) ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < init.nBindingCount; ++iBinding ) {
		if ( init.pBindings == nullptr || init.pBindings[iBinding].nBindingId == 0 ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( iBinding != 0 && init.pBindings[iBinding].nBindingId <= init.pBindings[iBinding - 1].nBindingId ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
	}
	m_pPool = &pool;
	sirRegistrationResult_t eResult = loadNames( init, pool, nNames );
	if ( eResult == SIR_REGISTRATION_OK ) {
		eResult = loadDescriptors( init, nNames );
	}
	if ( eResult != SIR_REGISTRATION_OK ) {
		m_pPool = nullptr;
		return eResult;
	}
	m_pEntries = init.pEntries;
	m_pNames = init.pNames;
	m_nCapacity = init.nEntryCapacity;
	m_nCookedCount = nCommands + nCVars;
	m_nLiveCount = m_nCookedCount;
	for ( uint32_t iEntry = m_nCookedCount; iEntry < m_nCapacity; ++iEntry ) {
		m_pEntries[iEntry] = {};
		m_pEntries[iEntry].nGeneration = m_nGenerationSeed;
	}
	m_bInitialized = true;
	return SIR_REGISTRATION_OK;
}

bool SIRRegistrationCatalog::isNameOccupied( SIRStringAtom atom ) const noexcept
{
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		if ( m_pEntries[iEntry].bActive && m_pEntries[iEntry].atom == atom ) {
			return true;
		}
	}
	return false;
}

sirRegistrationResult_t SIRRegistrationCatalog::find( const char *pszName, size_t nNameBytes,
                                                      sirRegistrationHandle_t *pHandle ) const noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bInitialized ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	SIRStringAtom atom;
	sirRegistrationResult_t eResult = sirRegistrationFindName( *m_pPool, pszName, nNameBytes, &atom );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		const sirRegistrationEntry_t& entry = m_pEntries[iEntry];
		if ( entry.bActive && entry.atom == atom ) {
			pHandle->nSlot = iEntry + 1;
			pHandle->nGeneration = entry.nGeneration;
			return SIR_REGISTRATION_OK;
		}
	}
	return SIR_REGISTRATION_NOT_FOUND;
}

sirRegistrationResult_t SIRRegistrationCatalog::registerDynamic( const sirRegistrationDynamicDesc_t& desc,
                                                                 sirRegistrationHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !m_bInitialized ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( desc.eKind != SIR_REGISTRATION_COMMAND && desc.eKind != SIR_REGISTRATION_CVAR ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( desc.eKind == SIR_REGISTRATION_COMMAND && ( desc.nBindingId == 0 || desc.pfnCallback == nullptr ) ) {
		return SIR_REGISTRATION_UNBOUND_CALLBACK;
	}
	if ( desc.eKind == SIR_REGISTRATION_CVAR && ( desc.eType >= SIR_CVAR_TYPE_COUNT || !sirValidateCVarFlags( desc.nFlags ) ) ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( desc.eKind == SIR_REGISTRATION_CVAR && desc.nSchemaId == 0 ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( desc.eKind == SIR_REGISTRATION_COMMAND && ( desc.nFlags != 0 || ( desc.nCapabilities & ~511u ) != 0 ) ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	uint32_t iFree = m_nCapacity;
	for ( uint32_t iEntry = m_nCookedCount; iEntry < m_nCapacity; ++iEntry ) {
		if ( !m_pEntries[iEntry].bActive ) {
			iFree = iEntry;
			break;
		}
	}
	if ( iFree == m_nCapacity ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	SIRStringAtom existing;
	const sirRegistrationResult_t eFind = sirRegistrationFindName( *m_pPool, desc.name.pszName, desc.name.nNameBytes, &existing );
	if ( eFind != SIR_REGISTRATION_OK && eFind != SIR_REGISTRATION_NOT_FOUND ) {
		return eFind;
	}
	if ( existing && isNameOccupied( existing ) ) {
		return SIR_REGISTRATION_DUPLICATE_NAME;
	}
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		const sirRegistrationEntry_t& entry = m_pEntries[iEntry];
		if ( !entry.bActive ) {
			continue;
		}
		if ( desc.eKind == SIR_REGISTRATION_COMMAND && entry.eKind == SIR_REGISTRATION_COMMAND && entry.nBindingId == desc.nBindingId ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( desc.eKind == SIR_REGISTRATION_CVAR && entry.eKind == SIR_REGISTRATION_CVAR && entry.nSchemaId == desc.nSchemaId ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
	}
	SIRStringAtom atom;
	const sirRegistrationResult_t eIntern = sirRegistrationInternName( *m_pPool, desc.name, &atom );
	if ( eIntern != SIR_REGISTRATION_OK ) {
		return eIntern;
	}
	sirRegistrationEntry_t& entry = m_pEntries[iFree];
	const uint32_t nGeneration = entry.nGeneration == 0 ? 1 : entry.nGeneration;
	entry = {};
	entry.atom = atom;
	entry.nBindingId = desc.eKind == SIR_REGISTRATION_COMMAND ? desc.nBindingId : 0;
	entry.nSchemaId = desc.nSchemaId;
	entry.nOwnerId = desc.name.nOwnerId;
	entry.nFlags = desc.nFlags;
	entry.nCapabilities = desc.nCapabilities;
	entry.nType = desc.eKind == SIR_REGISTRATION_CVAR ? ( uint32_t )desc.eType : 0;
	entry.nGeneration = nGeneration;
	entry.pfnCallback = desc.pfnCallback;
	entry.pContext = desc.pContext;
	entry.eKind = desc.eKind;
	entry.bActive = true;
	entry.bCooked = false;
	++m_nLiveCount;
	*pHandle = { iFree + 1, nGeneration };
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRRegistrationCatalog::unregisterDynamic( sirRegistrationHandle_t handle ) noexcept
{
	const sirRegistrationEntry_t *pEntry = nullptr;
	sirRegistrationResult_t eResult = getEntry( handle, &pEntry );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	if ( pEntry->bCooked ) {
		return SIR_REGISTRATION_DENIED;
	}
	sirRegistrationEntry_t& entry = m_pEntries[handle.nSlot - 1];
	entry.bActive = false;
	entry.pfnCallback = nullptr;
	entry.pContext = nullptr;
	++entry.nGeneration;
	if ( entry.nGeneration == 0 ) {
		entry.nGeneration = 1;
	}
	--m_nLiveCount;
	return SIR_REGISTRATION_OK;
}

uint32_t SIRRegistrationCatalog::unregisterModule( uint32_t nOwnerId ) noexcept
{
	if ( !m_bInitialized || nOwnerId == 0 ) {
		return 0;
	}
	uint32_t nRemoved = 0;
	for ( uint32_t iEntry = 0; iEntry < m_nCapacity; ++iEntry ) {
		sirRegistrationEntry_t& entry = m_pEntries[iEntry];
		if ( !entry.bActive || entry.nOwnerId != nOwnerId ) {
			continue;
		}
		entry.bActive = false;
		entry.pfnCallback = nullptr;
		entry.pContext = nullptr;
		++entry.nGeneration;
		if ( entry.nGeneration == 0 ) {
			entry.nGeneration = 1;
		}
		--m_nLiveCount;
		++nRemoved;
	}
	return nRemoved;
}

void SIRRegistrationCatalog::shutdown( void ) noexcept
{
	m_pPool = nullptr;
	m_pEntries = nullptr;
	m_pNames = nullptr;
	m_nCapacity = 0;
	m_nCookedCount = 0;
	m_nLiveCount = 0;
	++m_nGenerationSeed;
	if ( m_nGenerationSeed == 0 ) {
		m_nGenerationSeed = 1;
	}
	m_bInitialized = false;
}
