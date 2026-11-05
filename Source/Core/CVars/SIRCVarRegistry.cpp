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

#include <Core/CVars/SIRCVarRegistry.h>
#include <string.h>

static bool sirCVarNarrow( sirCVarType_t eType ) noexcept
{
	return eType == SIR_CVAR_BOOL || eType == SIR_CVAR_I32 || eType == SIR_CVAR_U32 || eType == SIR_CVAR_F32 || eType == SIR_CVAR_ENUM_U32;
}

static bool sirCVarTextValid( const char *pszText, uint32_t nBytes ) noexcept
{
	if ( pszText == nullptr ) {
		return nBytes == 0;
	}
	for ( uint32_t iByte = 0; iByte < nBytes; ) {
		const uint8_t nFirst = ( uint8_t )pszText[iByte];
		if ( nFirst == 0 ) {
			return false;
		}
		if ( nFirst < 0x80 ) {
			++iByte;
			continue;
		}
		uint32_t nCodePoint = 0;
		uint32_t nMore = 0;
		if ( nFirst >= 0xc2 && nFirst <= 0xdf ) {
			nCodePoint = nFirst & 0x1f;
			nMore = 1;
		} else if ( nFirst >= 0xe0 && nFirst <= 0xef ) {
			nCodePoint = nFirst & 0x0f;
			nMore = 2;
		} else if ( nFirst >= 0xf0 && nFirst <= 0xf4 ) {
			nCodePoint = nFirst & 0x07;
			nMore = 3;
		} else {
			return false;
		}
		if ( nMore > nBytes - iByte - 1 ) {
			return false;
		}
		for ( uint32_t iPart = 0; iPart < nMore; ++iPart ) {
			const uint8_t nPart = ( uint8_t )pszText[iByte + iPart + 1];
			if ( ( nPart & 0xc0 ) != 0x80 ) {
				return false;
			}
			nCodePoint = ( nCodePoint << 6 ) | ( nPart & 0x3f );
		}
		if ( ( nMore == 2 && nCodePoint < 0x800 ) || ( nMore == 3 && nCodePoint < 0x10000 ) ) {
			return false;
		}
		if ( ( nCodePoint >= 0xd800 && nCodePoint <= 0xdfff ) || nCodePoint > 0x10ffff ) {
			return false;
		}
		iByte += nMore + 1;
	}
	return true;
}

static bool sirCVarValidValue( sirCVarType_t eType, const sirCVarValue_t& value, uint32_t nStride ) noexcept
{
	if ( eType == SIR_CVAR_UTF8_STRING ) {
		return value.nTextBytes < nStride && sirCVarTextValid( value.pszText, value.nTextBytes );
	}
	if ( eType == SIR_CVAR_BOOL ) {
		return value.nBits <= 1 && value.pszText == nullptr && value.nTextBytes == 0;
	}
	if ( sirCVarNarrow( eType ) ) {
		return ( value.nBits >> 32 ) == 0 && value.pszText == nullptr && value.nTextBytes == 0;
	}
	return eType < SIR_CVAR_TYPE_COUNT && value.pszText == nullptr && value.nTextBytes == 0;
}

static uint32_t sirCVarNext( uint32_t iIndex, uint32_t nCapacity ) noexcept
{
	return iIndex + 1 == nCapacity ? 0 : iIndex + 1;
}

static bool sirCVarValidStorage( const sirCVarStorage_t& storage ) noexcept
{
	if ( !storage.pSlots || !storage.pOverrides || !storage.pShadow ) {
		return false;
	}
	if ( !storage.pEvents || !storage.pEventScratch || !storage.pStagedWrites ) {
		return false;
	}
	if ( !storage.pszActiveText || !storage.pszPendingText || !storage.pszStagedText || !storage.pszEventText ) {
		return false;
	}
	if ( !storage.nSlots || !storage.nOverrides || !storage.nEvents || !storage.nStagedWrites || storage.nStringStride < 2 ) {
		return false;
	}
	if ( ( storage.nBank32 != 0 && !storage.pBank32 ) || ( storage.nBank64 != 0 && !storage.pBank64 ) ||
	     storage.nStagedWrites < storage.nSlots ) {
		return false;
	}
	const size_t nStride = storage.nStringStride;
	if ( nStride > SIZE_MAX / storage.nOverrides || nStride > SIZE_MAX / storage.nStagedWrites ) {
		return false;
	}
	if ( storage.nEvents > SIZE_MAX / nStride / 2 ) {
		return false;
	}
	const size_t nOverrideBytes = ( size_t )storage.nOverrides * nStride;
	const size_t nStageBytes = ( size_t )storage.nStagedWrites * nStride;
	const size_t nEventBytes = ( size_t )storage.nEvents * nStride * 2;
	return storage.nActiveTextBytes >= nOverrideBytes && storage.nPendingTextBytes >= nOverrideBytes &&
	       storage.nStagedTextBytes >= nStageBytes && storage.nEventTextBytes >= nEventBytes;
}

static uint32_t sirCVarRead32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8 ) | ( ( uint32_t )pBytes[2] << 16 ) | ( ( uint32_t )pBytes[3] << 24 );
}

sirRegistrationResult_t sirCVarDefinitionFromCooked( const SIRRegistrationCatalog& registration, sirRegistrationHandle_t handle,
                                                     const sirCVarCookedSource_t& source, sirCVarDefinition_t *pOutput ) noexcept
{
	if ( pOutput == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pOutput = {};
	if ( source.pBytes == nullptr || source.nBytes < SIR_REGISTRATION_CATALOG_HEADER_BYTES || source.ePhase > SIR_CVAR_APPLY_RESTART ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	const sirRegistrationEntry_t *pEntry = nullptr;
	const sirRegistrationResult_t eLookup = registration.getEntry( handle, &pEntry );
	if ( eLookup != SIR_REGISTRATION_OK ) {
		return eLookup;
	}
	if ( pEntry->eKind != SIR_REGISTRATION_CVAR || !pEntry->bCooked || pEntry->nType >= SIR_CVAR_TYPE_COUNT ) {
		return SIR_REGISTRATION_TYPE_MISMATCH;
	}
	if ( sirCVarRead32( source.pBytes ) != SIR_REGISTRATION_CATALOG_MAGIC || source.pBytes[4] != SIR_REGISTRATION_CATALOG_VERSION ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	const size_t nOffset = pEntry->nDefaultOffset;
	if ( nOffset < SIR_REGISTRATION_CATALOG_HEADER_BYTES || nOffset > source.nBytes || source.nBytes - nOffset < 8 ) {
		return SIR_REGISTRATION_BAD_BOUNDS;
	}
	const uint8_t *pDefault = source.pBytes + nOffset;
	sirCVarValue_t value = {};
	if ( pEntry->nType == SIR_CVAR_UTF8_STRING ) {
		const uint32_t nTextBytes = sirCVarRead32( pDefault );
		if ( nTextBytes > source.nBytes - nOffset - 4 ) {
			return SIR_REGISTRATION_BAD_BOUNDS;
		}
		value.pszText = ( const char * )pDefault + 4;
		value.nTextBytes = nTextBytes;
	} else {
		value.nBits = ( uint64_t )sirCVarRead32( pDefault ) | ( ( uint64_t )sirCVarRead32( pDefault + 4 ) << 32 );
	}
	pOutput->registration = handle;
	pOutput->defaultValue = value;
	pOutput->nSinkGroup = source.nSinkGroup;
	pOutput->ePhase = source.ePhase;
	return SIR_REGISTRATION_OK;
}

SIRCVarRegistry::SIRCVarRegistry( void ) noexcept
    : m_pRegistration( nullptr ), m_Storage( {} ), m_OwnerThread(), m_nRevision( 0 ), m_nSerial( 0 ), m_nTokenSeed( 0 ),
      m_nPreparedToken( 0 ), m_nPreparedWrites( 0 ), m_nPreparedEvents( 0 ), m_nEventHead( 0 ), m_nEventTail( 0 ), m_nEventCount( 0 ),
      m_nActiveSlots( 0 ), m_nHandleSeed( 0 ), m_bPrepared( false ), m_bPumping( false )
{
}

SIRCVarRegistry::~SIRCVarRegistry( void ) noexcept
{
	shutdown();
}

bool SIRCVarRegistry::ownerThread( void ) const noexcept
{
	return m_OwnerThread == std::this_thread::get_id();
}

bool SIRCVarRegistry::isInitialized( void ) const noexcept
{
	return m_pRegistration != nullptr;
}

uint32_t SIRCVarRegistry::getCount( void ) const noexcept
{
	return m_nActiveSlots;
}

uint32_t SIRCVarRegistry::getPendingEvents( void ) const noexcept
{
	return m_nEventCount;
}

uint64_t SIRCVarRegistry::getRevision( void ) const noexcept
{
	return m_nRevision;
}

sirRegistrationResult_t SIRCVarRegistry::initialize( SIRRegistrationCatalog& registration, const sirCVarStorage_t& storage ) noexcept
{
	if ( isInitialized() || !registration.isInitialized() || !sirCVarValidStorage( storage ) ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	m_Storage = storage;
	for ( uint32_t iSlot = 0; iSlot < storage.nSlots; ++iSlot ) {
		storage.pSlots[iSlot] = {};
	}
	for ( uint32_t iOverride = 0; iOverride < storage.nOverrides; ++iOverride ) {
		storage.pOverrides[iOverride] = {};
		storage.pShadow[iOverride] = {};
	}
	m_pRegistration = &registration;
	m_OwnerThread = std::this_thread::get_id();
	m_nRevision = 0;
	m_nSerial = 0;
	m_nEventHead = 0;
	m_nEventTail = 0;
	m_nEventCount = 0;
	m_nActiveSlots = 0;
	m_bPrepared = false;
	m_bPumping = false;
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::bind( const sirCVarDefinition_t& definition, sirCVarHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPrepared || m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	const sirRegistrationEntry_t *pEntry = nullptr;
	sirRegistrationResult_t eResult = m_pRegistration->getEntry( definition.registration, &pEntry );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	if ( pEntry->eKind != SIR_REGISTRATION_CVAR || pEntry->nType >= SIR_CVAR_TYPE_COUNT ) {
		return SIR_REGISTRATION_TYPE_MISMATCH;
	}
	const sirCVarType_t eType = ( sirCVarType_t )pEntry->nType;
	if ( !sirCVarValidValue( eType, definition.defaultValue, m_Storage.nStringStride ) ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( definition.ePhase > SIR_CVAR_APPLY_RESTART || !sirValidateCVarFlags( pEntry->nFlags ) ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	uint32_t iFree = m_Storage.nSlots;
	for ( uint32_t iSlot = 0; iSlot < m_Storage.nSlots; ++iSlot ) {
		const sirCVarSlot_t& slot = m_Storage.pSlots[iSlot];
		if ( slot.bActive && slot.registration.nSlot == definition.registration.nSlot &&
		     slot.registration.nGeneration == definition.registration.nGeneration ) {
			return SIR_REGISTRATION_DUPLICATE_NAME;
		}
		if ( !slot.bActive && iFree == m_Storage.nSlots ) {
			iFree = iSlot;
		}
	}
	if ( iFree == m_Storage.nSlots || m_nHandleSeed == UINT32_MAX ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	const bool bString = eType == SIR_CVAR_UTF8_STRING;
	const bool bNarrow = sirCVarNarrow( eType );
	const uint32_t nCapacity = bString ? 0 : ( bNarrow ? m_Storage.nBank32 : m_Storage.nBank64 );
	uint32_t iBank = UINT32_MAX;
	if ( !bString ) {
		for ( uint32_t iCandidate = 0; iCandidate < nCapacity; ++iCandidate ) {
			bool bUsed = false;
			for ( uint32_t iSlot = 0; iSlot < m_Storage.nSlots; ++iSlot ) {
				const sirCVarSlot_t& slot = m_Storage.pSlots[iSlot];
				if ( slot.bActive && slot.eType != SIR_CVAR_UTF8_STRING && sirCVarNarrow( slot.eType ) == bNarrow &&
				     slot.nBankIndex == iCandidate ) {
					bUsed = true;
					break;
				}
			}
			if ( !bUsed ) {
				iBank = iCandidate;
				break;
			}
		}
		if ( iBank == UINT32_MAX ) {
			return SIR_REGISTRATION_CAPACITY_EXCEEDED;
		}
	}
	sirCVarSlot_t& slot = m_Storage.pSlots[iFree];
	slot = {};
	slot.registration = definition.registration;
	slot.baseValue = definition.defaultValue;
	slot.effective = definition.defaultValue;
	slot.nGeneration = ++m_nHandleSeed;
	slot.nFlags = pEntry->nFlags;
	slot.nSinkGroup = definition.nSinkGroup;
	slot.nBankIndex = iBank;
	slot.eType = eType;
	slot.ePhase = definition.ePhase;
	slot.bActive = true;
	if ( bString ) {
		// Strings reside only in the dedicated text arenas.
	} else if ( bNarrow ) {
		m_Storage.pBank32[iBank] = ( uint32_t )definition.defaultValue.nBits;
	} else {
		m_Storage.pBank64[iBank] = definition.defaultValue.nBits;
	}
	++m_nActiveSlots;
	*pHandle = { iFree + 1, slot.nGeneration };
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::validateHandle( sirCVarHandle_t handle, const sirCVarSlot_t **ppSlot ) const noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( handle.nSlot == 0 || handle.nSlot > m_Storage.nSlots ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	const sirCVarSlot_t& slot = m_Storage.pSlots[handle.nSlot - 1];
	if ( !slot.bActive || slot.nGeneration != handle.nGeneration ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	const sirRegistrationEntry_t *pEntry = nullptr;
	const sirRegistrationResult_t eResult = m_pRegistration->getEntry( slot.registration, &pEntry );
	if ( eResult != SIR_REGISTRATION_OK || pEntry->eKind != SIR_REGISTRATION_CVAR || pEntry->nType != slot.eType ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	*ppSlot = &slot;
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::getValue( sirCVarHandle_t handle, uint32_t nCapabilities, sirCVarValue_t *pValue ) const noexcept
{
	if ( pValue == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pValue = {};
	const sirCVarSlot_t *pSlot = nullptr;
	const sirRegistrationResult_t eStatus = validateHandle( handle, &pSlot );
	if ( eStatus != SIR_REGISTRATION_OK ) {
		return eStatus;
	}
	if ( ( nCapabilities & SIR_CONSOLE_CAP_READ ) == 0 ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ( pSlot->nFlags & SIR_CVAR_FLAG_SENSITIVE ) != 0 && ( nCapabilities & SIR_CONSOLE_CAP_SENSITIVE ) == 0 ) {
		return SIR_REGISTRATION_DENIED;
	}
	*pValue = pSlot->effective;
	if ( pSlot->eType != SIR_CVAR_UTF8_STRING ) {
		pValue->nBits = sirCVarNarrow( pSlot->eType ) ? m_Storage.pBank32[pSlot->nBankIndex] : m_Storage.pBank64[pSlot->nBankIndex];
	}
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::unbind( sirCVarHandle_t handle ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPrepared || m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( handle.nSlot == 0 || handle.nSlot > m_Storage.nSlots ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	sirCVarSlot_t& slot = m_Storage.pSlots[handle.nSlot - 1];
	if ( !slot.bActive || slot.nGeneration != handle.nGeneration ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	for ( uint32_t iOverride = 0; iOverride < m_Storage.nOverrides; ++iOverride ) {
		if ( m_Storage.pOverrides[iOverride].bActive && m_Storage.pOverrides[iOverride].nSlot == handle.nSlot ) {
			m_Storage.pOverrides[iOverride] = {};
		}
	}
	slot = {};
	--m_nActiveSlots;
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::validateWrite( const sirCVarWrite_t& write, const sirCVarSlot_t **ppSlot ) const noexcept
{
	sirRegistrationResult_t eResult = validateHandle( write.handle, ppSlot );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	const sirCVarSlot_t& slot = **ppSlot;
	if ( write.origin.eSource >= SIR_CVAR_SOURCE_COUNT || ( write.origin.nCapabilities & SIR_CONSOLE_CAP_WRITE ) == 0 ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ( slot.nFlags & SIR_CVAR_FLAG_SENSITIVE ) != 0 && ( write.origin.nCapabilities & SIR_CONSOLE_CAP_SENSITIVE ) == 0 ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ( slot.nFlags & SIR_CVAR_FLAG_DEVELOPMENT ) != 0 &&
	     ( !write.origin.bDevelopmentAllowed || ( write.origin.nCapabilities & SIR_CONSOLE_CAP_DEVELOPMENT ) == 0 ) ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ( slot.nFlags & SIR_CVAR_FLAG_CHEAT ) != 0 &&
	     ( !write.origin.bCheatsAllowed || ( write.origin.nCapabilities & SIR_CONSOLE_CAP_CHEAT ) == 0 ) ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ( slot.nFlags & SIR_CVAR_FLAG_REPLICATED ) != 0 && ( write.origin.nCapabilities & SIR_CONSOLE_CAP_SERVER ) == 0 ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ( slot.nFlags & SIR_CVAR_FLAG_INIT_ONLY ) != 0 &&
	     ( !write.origin.bInitializing || ( write.origin.nCapabilities & SIR_CONSOLE_CAP_INIT ) == 0 ) ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ( slot.nFlags & SIR_CVAR_FLAG_READ_ONLY ) != 0 &&
	     ( !write.origin.bEngineInternal || !write.origin.bInitializing || ( write.origin.nCapabilities & SIR_CONSOLE_CAP_INIT ) == 0 ) ) {
		return SIR_REGISTRATION_DENIED;
	}
	return write.bRemove || sirCVarValidValue( slot.eType, write.value, m_Storage.nStringStride ) ? SIR_REGISTRATION_OK
	                                                                                              : SIR_REGISTRATION_TYPE_MISMATCH;
}

sirRegistrationResult_t SIRCVarRegistry::applyStagedWrite( const sirCVarWrite_t& write, uint32_t iWrite ) noexcept
{
	const uint32_t iSlot = write.handle.nSlot - 1;
	uint32_t iTarget = m_Storage.nOverrides;
	uint32_t iFree = m_Storage.nOverrides;
	for ( uint32_t iOverride = 0; iOverride < m_Storage.nOverrides; ++iOverride ) {
		const sirCVarOverride_t& override = m_Storage.pShadow[iOverride];
		if ( override.bActive && override.nSlot == write.handle.nSlot && override.eSource == write.origin.eSource &&
		     override.nIssuerId == write.origin.nIssuerId ) {
			iTarget = iOverride;
			break;
		}
		if ( !override.bActive && iFree == m_Storage.nOverrides ) {
			iFree = iOverride;
		}
	}
	if ( write.bRemove ) {
		if ( iTarget != m_Storage.nOverrides ) {
			m_Storage.pShadow[iTarget] = {};
		}
		return SIR_REGISTRATION_OK;
	}
	if ( iTarget == m_Storage.nOverrides ) {
		iTarget = iFree;
	}
	if ( iTarget == m_Storage.nOverrides ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	sirCVarOverride_t& entry = m_Storage.pShadow[iTarget];
	if ( !entry.bActive ) {
		entry = {};
	}
	entry.bActive = true;
	entry.nSlot = write.handle.nSlot;
	entry.nIssuerId = write.origin.nIssuerId;
	entry.eSource = write.origin.eSource;
	const sirCVarSlot_t& slot = m_Storage.pSlots[iSlot];
	sirCVarValue_t value = write.value;
	if ( slot.eType == SIR_CVAR_UTF8_STRING ) {
		char *pszDestination = m_Storage.pszStagedText + ( size_t )iWrite * m_Storage.nStringStride;
		if ( value.nTextBytes != 0 ) {
			memcpy( pszDestination, value.pszText, value.nTextBytes );
		}
		pszDestination[value.nTextBytes] = '\0';
		value.pszText = pszDestination;
	}
	if ( ( slot.nFlags & SIR_CVAR_FLAG_LATCHED ) != 0 ) {
		entry.pending = value;
		entry.nPendingSerial = m_nSerial + iWrite + 1;
		entry.bPending = true;
	} else {
		entry.value = value;
		entry.nSerial = m_nSerial + iWrite + 1;
		entry.bApplied = true;
		entry.bPending = false;
	}
	return SIR_REGISTRATION_OK;
}

int32_t SIRCVarRegistry::selectWinner( uint32_t iSlot, const sirCVarOverride_t *pOverrides ) const noexcept
{
	int32_t iBest = -1;
	for ( uint32_t iOverride = 0; iOverride < m_Storage.nOverrides; ++iOverride ) {
		const sirCVarOverride_t& candidate = pOverrides[iOverride];
		if ( !candidate.bActive || !candidate.bApplied || candidate.nSlot != iSlot + 1 ) {
			continue;
		}
		if ( iBest < 0 ) {
			iBest = ( int32_t )iOverride;
			continue;
		}
		const sirCVarOverride_t& best = pOverrides[iBest];
		if ( candidate.eSource > best.eSource || ( candidate.eSource == best.eSource && candidate.nIssuerId > best.nIssuerId ) ||
		     ( candidate.eSource == best.eSource && candidate.nIssuerId == best.nIssuerId && candidate.nSerial > best.nSerial ) ) {
			iBest = ( int32_t )iOverride;
		}
	}
	return iBest;
}

sirCVarValue_t SIRCVarRegistry::selectValue( uint32_t iSlot, const sirCVarOverride_t *pOverrides ) const noexcept
{
	const int32_t iWinner = selectWinner( iSlot, pOverrides );
	return iWinner < 0 ? m_Storage.pSlots[iSlot].baseValue : pOverrides[iWinner].value;
}

bool SIRCVarRegistry::sameValue( const sirCVarValue_t& a, const sirCVarValue_t& b, sirCVarType_t eType ) const noexcept
{
	if ( eType != SIR_CVAR_UTF8_STRING ) {
		return a.nBits == b.nBits;
	}
	return a.nTextBytes == b.nTextBytes && ( a.nTextBytes == 0 || memcmp( a.pszText, b.pszText, a.nTextBytes ) == 0 );
}

sirRegistrationResult_t SIRCVarRegistry::reserveEvents( void ) noexcept
{
	m_nPreparedEvents = 0;
	for ( uint32_t iSlot = 0; iSlot < m_Storage.nSlots; ++iSlot ) {
		const sirCVarSlot_t& slot = m_Storage.pSlots[iSlot];
		if ( !slot.bActive ) {
			continue;
		}
		const sirCVarValue_t candidate = selectValue( iSlot, m_Storage.pShadow );
		if ( sameValue( candidate, slot.effective, slot.eType ) ) {
			continue;
		}
		if ( m_nPreparedEvents == m_Storage.nEvents || m_nPreparedEvents >= m_Storage.nEvents - m_nEventCount ) {
			return SIR_REGISTRATION_QUEUE_FULL;
		}
		sirCVarEvent_t& event = m_Storage.pEventScratch[m_nPreparedEvents++];
		event.handle = { iSlot + 1, slot.nGeneration };
		event.oldValue = slot.effective;
		event.newValue = candidate;
		event.nRevision = m_nRevision + 1;
		event.nSinkGroup = slot.nSinkGroup;
	}
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::prepareBatch( const sirCVarWrite_t *pWrites, uint32_t nCount, uint64_t *pToken ) noexcept
{
	if ( pToken == nullptr || pWrites == nullptr || nCount == 0 ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pToken = 0;
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPrepared || m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( nCount > m_Storage.nStagedWrites || nCount > UINT64_MAX - m_nSerial || m_nRevision == UINT64_MAX || m_nTokenSeed == UINT64_MAX ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	memcpy( m_Storage.pShadow, m_Storage.pOverrides, ( size_t )m_Storage.nOverrides * sizeof( sirCVarOverride_t ) );
	for ( uint32_t iWrite = 0; iWrite < nCount; ++iWrite ) {
		const sirCVarSlot_t *pSlot = nullptr;
		const sirRegistrationResult_t eResult = validateWrite( pWrites[iWrite], &pSlot );
		if ( eResult != SIR_REGISTRATION_OK ) {
			return eResult;
		}
		m_Storage.pStagedWrites[iWrite] = pWrites[iWrite];
		const sirRegistrationResult_t eApply = applyStagedWrite( pWrites[iWrite], iWrite );
		if ( eApply != SIR_REGISTRATION_OK ) {
			return eApply;
		}
	}
	const sirRegistrationResult_t eReserve = reserveEvents();
	if ( eReserve != SIR_REGISTRATION_OK ) {
		return eReserve;
	}
	m_nPreparedWrites = nCount;
	m_nPreparedToken = ++m_nTokenSeed;
	m_bPrepared = true;
	*pToken = m_nPreparedToken;
	return SIR_REGISTRATION_OK;
}

void SIRCVarRegistry::copyEvents( void ) noexcept
{
	uint32_t iEvent = m_nEventTail;
	for ( uint32_t iScratch = 0; iScratch < m_nPreparedEvents; ++iScratch ) {
		sirCVarEvent_t event = m_Storage.pEventScratch[iScratch];
		const sirCVarSlot_t& slot = m_Storage.pSlots[event.handle.nSlot - 1];
		if ( slot.eType == SIR_CVAR_UTF8_STRING ) {
			char *pszOld = m_Storage.pszEventText + ( ( size_t )iEvent * 2 ) * m_Storage.nStringStride;
			char *pszNew = pszOld + m_Storage.nStringStride;
			if ( event.oldValue.nTextBytes != 0 ) {
				memcpy( pszOld, event.oldValue.pszText, event.oldValue.nTextBytes );
			}
			pszOld[event.oldValue.nTextBytes] = '\0';
			if ( event.newValue.nTextBytes != 0 ) {
				memcpy( pszNew, event.newValue.pszText, event.newValue.nTextBytes );
			}
			pszNew[event.newValue.nTextBytes] = '\0';
			event.oldValue.pszText = pszOld;
			event.newValue.pszText = pszNew;
		}
		m_Storage.pEvents[iEvent] = event;
		iEvent = sirCVarNext( iEvent, m_Storage.nEvents );
	}
}

void SIRCVarRegistry::publishOverrides( void ) noexcept
{
	for ( uint32_t iOverride = 0; iOverride < m_Storage.nOverrides; ++iOverride ) {
		sirCVarOverride_t entry = m_Storage.pShadow[iOverride];
		if ( !entry.bActive ) {
			m_Storage.pOverrides[iOverride] = {};
			continue;
		}
		const sirCVarSlot_t& slot = m_Storage.pSlots[entry.nSlot - 1];
		if ( slot.eType == SIR_CVAR_UTF8_STRING ) {
			if ( entry.bApplied ) {
				char *pszTarget = m_Storage.pszActiveText + ( size_t )iOverride * m_Storage.nStringStride;
				if ( entry.value.nTextBytes != 0 ) {
					memmove( pszTarget, entry.value.pszText, entry.value.nTextBytes );
				}
				pszTarget[entry.value.nTextBytes] = '\0';
				entry.value.pszText = pszTarget;
			}
			if ( entry.bPending ) {
				char *pszTarget = m_Storage.pszPendingText + ( size_t )iOverride * m_Storage.nStringStride;
				if ( entry.pending.nTextBytes != 0 ) {
					memmove( pszTarget, entry.pending.pszText, entry.pending.nTextBytes );
				}
				pszTarget[entry.pending.nTextBytes] = '\0';
				entry.pending.pszText = pszTarget;
			}
		}
		m_Storage.pOverrides[iOverride] = entry;
	}
}

void SIRCVarRegistry::publishBanks( void ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < m_Storage.nSlots; ++iSlot ) {
		sirCVarSlot_t& slot = m_Storage.pSlots[iSlot];
		if ( !slot.bActive ) {
			continue;
		}
		slot.effective = selectValue( iSlot, m_Storage.pOverrides );
		if ( slot.eType == SIR_CVAR_UTF8_STRING ) {
			continue;
		}
		if ( sirCVarNarrow( slot.eType ) ) {
			m_Storage.pBank32[slot.nBankIndex] = ( uint32_t )slot.effective.nBits;
		} else {
			m_Storage.pBank64[slot.nBankIndex] = slot.effective.nBits;
		}
	}
}

sirRegistrationResult_t SIRCVarRegistry::commitBatch( uint64_t nToken ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( !m_bPrepared || nToken == 0 || nToken != m_nPreparedToken ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	copyEvents();
	publishOverrides();
	publishBanks();
	for ( uint32_t iEvent = 0; iEvent < m_nPreparedEvents; ++iEvent ) {
		m_nEventTail = sirCVarNext( m_nEventTail, m_Storage.nEvents );
	}
	m_nEventCount += m_nPreparedEvents;
	m_nSerial += m_nPreparedWrites;
	++m_nRevision;
	m_nPreparedToken = 0;
	m_nPreparedWrites = 0;
	m_nPreparedEvents = 0;
	m_bPrepared = false;
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::cancelBatch( uint64_t nToken ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( !m_bPrepared || nToken == 0 || nToken != m_nPreparedToken ) {
		return SIR_REGISTRATION_STALE_HANDLE;
	}
	m_bPrepared = false;
	m_nPreparedToken = 0;
	m_nPreparedWrites = 0;
	m_nPreparedEvents = 0;
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::applyLatched( sirCVarApplyPhase_t ePhase ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPrepared || m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( ePhase > SIR_CVAR_APPLY_RESTART || m_nRevision == UINT64_MAX || m_nTokenSeed == UINT64_MAX ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	memcpy( m_Storage.pShadow, m_Storage.pOverrides, ( size_t )m_Storage.nOverrides * sizeof( sirCVarOverride_t ) );
	uint32_t nApplied = 0;
	for ( uint32_t iOverride = 0; iOverride < m_Storage.nOverrides; ++iOverride ) {
		sirCVarOverride_t& entry = m_Storage.pShadow[iOverride];
		if ( !entry.bActive || !entry.bPending ) {
			continue;
		}
		const sirCVarSlot_t& slot = m_Storage.pSlots[entry.nSlot - 1];
		if ( !slot.bActive || slot.ePhase != ePhase ) {
			continue;
		}
		entry.value = entry.pending;
		entry.nSerial = entry.nPendingSerial;
		entry.bApplied = true;
		entry.bPending = false;
		++nApplied;
	}
	if ( nApplied == 0 ) {
		return SIR_REGISTRATION_NO_CHANGE;
	}
	const sirRegistrationResult_t eResult = reserveEvents();
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	m_nPreparedToken = ++m_nTokenSeed;
	m_nPreparedWrites = 0;
	m_bPrepared = true;
	return commitBatch( m_nPreparedToken );
}

sirRegistrationResult_t SIRCVarRegistry::peekEvent( const sirCVarEvent_t **ppEvent ) const noexcept
{
	if ( ppEvent == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*ppEvent = nullptr;
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_nEventCount == 0 ) {
		return SIR_REGISTRATION_NOT_FOUND;
	}
	*ppEvent = &m_Storage.pEvents[m_nEventHead];
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRCVarRegistry::popEvent( void ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_nEventCount == 0 ) {
		return SIR_REGISTRATION_NOT_FOUND;
	}
	if ( m_bPrepared || m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	m_nEventHead = sirCVarNext( m_nEventHead, m_Storage.nEvents );
	--m_nEventCount;
	return SIR_REGISTRATION_OK;
}

uint32_t SIRCVarRegistry::pumpGroups( sirCVarGroupSink_t pfnSink, void *pContext ) noexcept
{
	if ( !isInitialized() || !ownerThread() || m_bPrepared || m_bPumping || pfnSink == nullptr ) {
		return 0;
	}
	m_bPumping = true;
	const uint32_t nEvents = m_nEventCount;
	for ( uint32_t iEvent = 0; iEvent < nEvents; ++iEvent ) {
		const uint32_t iCurrent = ( m_nEventHead + iEvent ) % m_Storage.nEvents;
		const sirCVarEvent_t& event = m_Storage.pEvents[iCurrent];
		bool bSeen = false;
		for ( uint32_t iEarlier = 0; iEarlier < iEvent; ++iEarlier ) {
			const sirCVarEvent_t& prior = m_Storage.pEvents[( m_nEventHead + iEarlier ) % m_Storage.nEvents];
			if ( prior.nRevision == event.nRevision && prior.nSinkGroup == event.nSinkGroup ) {
				bSeen = true;
				break;
			}
		}
		if ( !bSeen ) {
			pfnSink( pContext, event.nSinkGroup, event.nRevision );
		}
	}
	m_nEventHead = m_nEventTail;
	m_nEventCount = 0;
	m_bPumping = false;
	return nEvents;
}

void SIRCVarRegistry::shutdown( void ) noexcept
{
	if ( isInitialized() && ( !ownerThread() || m_bPumping ) ) {
		return;
	}
	m_pRegistration = nullptr;
	m_Storage = {};
	m_nRevision = 0;
	m_nSerial = 0;
	m_nPreparedToken = 0;
	m_nPreparedWrites = 0;
	m_nPreparedEvents = 0;
	m_nEventHead = 0;
	m_nEventTail = 0;
	m_nEventCount = 0;
	m_nActiveSlots = 0;
	m_bPrepared = false;
	m_bPumping = false;
}
