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

#include <Core/CVars/SIRCVarConfigAdapter.h>
#include <Core/CVars/SIRCVarValueCodec.h>
#include <string.h>

static bool sirCVarConfigSame( const char *pszLeft, const char *pszRight ) noexcept
{
	while ( *pszLeft != 0 && *pszRight != 0 ) {
		char nA = *pszLeft++;
		char nB = *pszRight++;
		if ( nA >= 'A' && nA <= 'Z' ) {
			nA = static_cast<char>( nA + 32 );
		}
		if ( nB >= 'A' && nB <= 'Z' ) {
			nB = static_cast<char>( nB + 32 );
		}
		if ( nA != nB ) {
			return false;
		}
	}
	return *pszLeft == *pszRight;
}

static SIRConfigResult sirCVarConfigStatus( SIRRegistrationResult eResult ) noexcept
{
	if ( eResult == SIRRegistrationResult::SIR_REGISTRATION_OK || eResult == SIRRegistrationResult::SIR_REGISTRATION_NO_CHANGE ) {
		return SIRConfigResult::SIR_CONFIG_OK;
	}
	if ( eResult == SIRRegistrationResult::SIR_REGISTRATION_NOT_FOUND ) {
		return SIRConfigResult::SIR_CONFIG_NOT_FOUND;
	}
	if ( eResult == SIRRegistrationResult::SIR_REGISTRATION_TYPE_MISMATCH ) {
		return SIRConfigResult::SIR_CONFIG_TYPE_MISMATCH;
	}
	if ( eResult == SIRRegistrationResult::SIR_REGISTRATION_CAPACITY_EXCEEDED || eResult == SIRRegistrationResult::SIR_REGISTRATION_QUEUE_FULL ) {
		return SIRConfigResult::SIR_CONFIG_LIMIT_EXCEEDED;
	}
	if ( eResult == SIRRegistrationResult::SIR_REGISTRATION_OUT_OF_MEMORY ) {
		return SIRConfigResult::SIR_CONFIG_OUT_OF_MEMORY;
	}
	return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
}

SIRCVarConfigAdapter::SIRCVarConfigAdapter( void ) noexcept
    : m_pRegistration( nullptr ), m_pCVars( nullptr ), m_pPool( nullptr ), m_Storage{}
{
}

SIRCVarConfigAdapter::~SIRCVarConfigAdapter( void ) noexcept
{
	shutdown();
}

bool SIRCVarConfigAdapter::isInitialized( void ) const noexcept
{
	return m_pRegistration != nullptr;
}

SIRConfigResult SIRCVarConfigAdapter::initialize( SIRRegistrationCatalog& registration, SIRCVarRegistry& cvars, SIRStringPool& pool,
                                                    const sirCVarConfigStorage_t& storage ) noexcept
{
	if ( isInitialized() || !registration.isInitialized() || !cvars.isInitialized() || !pool.isInitialized() ||
	     storage.ini.pEntries == nullptr || storage.ini.pszValues == nullptr || storage.pWrites == nullptr || storage.nWriteCapacity == 0 ||
	     storage.nWriteCapacity > storage.ini.nEntryCapacity || ( storage.nBindingCount != 0 && storage.pBindings == nullptr ) ) {
		return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < storage.nBindingCount; ++i ) {
		const sirCVarConfigBinding_t& binding = storage.pBindings[i];
		const sirRegistrationEntry_t *pEntry = nullptr;
		if ( registration.getEntry( binding.registration, &pEntry ) != SIRRegistrationResult::SIR_REGISTRATION_OK || pEntry->eKind != SIRRegistrationKind::SIR_REGISTRATION_CVAR ) {
			return SIRConfigResult::SIR_CONFIG_INVALID_HANDLE;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( storage.pBindings[j].registration.nSlot == binding.registration.nSlot ) {
				return SIRConfigResult::SIR_CONFIG_DUPLICATE_KEY;
			}
		}
	}
	m_pRegistration = &registration;
	m_pCVars = &cvars;
	m_pPool = &pool;
	m_Storage = storage;
	return SIRConfigResult::SIR_CONFIG_OK;
}

const sirCVarConfigBinding_t *SIRCVarConfigAdapter::findBinding( sirRegistrationHandle_t registration ) const noexcept
{
	for ( uint32_t i = 0; i < m_Storage.nBindingCount; ++i ) {
		const sirCVarConfigBinding_t& binding = m_Storage.pBindings[i];
		if ( binding.registration.nSlot == registration.nSlot && binding.registration.nGeneration == registration.nGeneration ) {
			return &binding;
		}
	}
	return nullptr;
}

SIRConfigResult SIRCVarConfigAdapter::applyIni( const sirConfigInput_t& input, const sirCVarOrigin_t& origin,
                                                  sirConfigDiagnostic_t *pDiagnostic ) noexcept
{
	if ( !isInitialized() ) {
		return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
	}
	sirIniParseResult_t parse {};
	SIRConfigResult eResult = SIRIniCodec::parse( input, m_Storage.limits, m_Storage.ini, &parse );
	const uint32_t nEntries = parse.nCount;
	if ( pDiagnostic != nullptr ) {
		*pDiagnostic = parse.diagnostic;
	}
	if ( eResult != SIRConfigResult::SIR_CONFIG_OK ) {
		return eResult;
	}
	if ( nEntries > m_Storage.nWriteCapacity ) {
		return SIRConfigResult::SIR_CONFIG_LIMIT_EXCEEDED;
	}
	uint32_t nFailureLine = 1;
	for ( uint32_t iEntry = 0; iEntry < nEntries; ++iEntry ) {
		const sirIniEntry_t& entry = m_Storage.ini.pEntries[iEntry];
		nFailureLine = entry.nLine;
		if ( !sirCVarConfigSame( entry.szSection, "CVars" ) ) {
			eResult = SIRConfigResult::SIR_CONFIG_UNSUPPORTED_FEATURE;
			break;
		}
		sirRegistrationHandle_t registration {};
		const SIRRegistrationResult eFind = m_pRegistration->find( entry.szKey, strlen( entry.szKey ), &registration );
		if ( eFind != SIRRegistrationResult::SIR_REGISTRATION_OK ) {
			eResult = sirCVarConfigStatus( eFind );
			break;
		}
		const sirRegistrationEntry_t *pDescriptor = nullptr;
		if ( m_pRegistration->getEntry( registration, &pDescriptor ) != SIRRegistrationResult::SIR_REGISTRATION_OK ||
		     pDescriptor->eKind != SIRRegistrationKind::SIR_REGISTRATION_CVAR ) {
			eResult = SIRConfigResult::SIR_CONFIG_TYPE_MISMATCH;
			break;
		}
		const sirCVarConfigBinding_t *pBinding = findBinding( registration );
		if ( pBinding == nullptr ) {
			eResult = SIRConfigResult::SIR_CONFIG_INVALID_HANDLE;
			break;
		}
		sirCVarValue_t value {};
		const SIRRegistrationResult eParsed =
		    sirCVarParseText( static_cast<SIRCVarType>(pDescriptor->nType), entry.pszValue, entry.nValueBytes, &value );
		if ( eParsed != SIRRegistrationResult::SIR_REGISTRATION_OK ) {
			eResult = sirCVarConfigStatus( eParsed );
			break;
		}
		m_Storage.pWrites[iEntry] = {};
		m_Storage.pWrites[iEntry].handle = pBinding->cvar;
		m_Storage.pWrites[iEntry].origin = origin;
		m_Storage.pWrites[iEntry].value = value;
	}
	if ( eResult != SIRConfigResult::SIR_CONFIG_OK ) {
		if ( pDiagnostic != nullptr ) {
			pDiagnostic->eResult = eResult;
			pDiagnostic->nLine = nFailureLine;
		}
		return eResult;
	}
	if ( nEntries == 0 ) {
		return SIRConfigResult::SIR_CONFIG_OK;
	}
	uint64_t nToken = 0;
	const SIRRegistrationResult ePrepare = m_pCVars->prepareBatch( m_Storage.pWrites, nEntries, &nToken );
	if ( ePrepare != SIRRegistrationResult::SIR_REGISTRATION_OK ) {
		return sirCVarConfigStatus( ePrepare );
	}
	return sirCVarConfigStatus( m_pCVars->commitBatch( nToken ) );
}

SIRConfigResult SIRCVarConfigAdapter::writeArchive( const sirConfigSink_t& sink, uint32_t nCapabilities, uint64_t nMaxBytes,
                                                      uint32_t *pWrittenEntries ) noexcept
{
	if ( pWrittenEntries == nullptr || !isInitialized() || sink.pfnWrite == nullptr || nMaxBytes == 0 ) {
		return SIRConfigResult::SIR_CONFIG_INVALID_ARGUMENT;
	}
	*pWrittenEntries = 0;
	uint32_t nCount = 0;
	for ( uint32_t i = 0; i < m_Storage.nBindingCount; ++i ) {
		const sirCVarConfigBinding_t& binding = m_Storage.pBindings[i];
		const sirRegistrationEntry_t *pDescriptor = nullptr;
		if ( m_pRegistration->getEntry( binding.registration, &pDescriptor ) != SIRRegistrationResult::SIR_REGISTRATION_OK ||
		     pDescriptor->eKind != SIRRegistrationKind::SIR_REGISTRATION_CVAR ) {
			return SIRConfigResult::SIR_CONFIG_INVALID_HANDLE;
		}
		if ( ( pDescriptor->nFlags & static_cast<uint32_t>( SIRCVarFlag::SIR_CVAR_FLAG_ARCHIVE ) ) == 0 ||
		     ( pDescriptor->nFlags & ( static_cast<uint32_t>( SIRCVarFlag::SIR_CVAR_FLAG_TRANSIENT ) | static_cast<uint32_t>( SIRCVarFlag::SIR_CVAR_FLAG_READ_ONLY ) ) ) != 0 ||
		     ( ( pDescriptor->nFlags & static_cast<uint32_t>( SIRCVarFlag::SIR_CVAR_FLAG_SENSITIVE ) ) != 0 && ( nCapabilities & static_cast<uint32_t>( SIRConsoleCapability::SIR_CONSOLE_CAP_SENSITIVE ) ) == 0 ) ) {
			continue;
		}
		if ( nCount >= m_Storage.ini.nEntryCapacity ) {
			return SIRConfigResult::SIR_CONFIG_LIMIT_EXCEEDED;
		}
		sirIniEntry_t& entry = m_Storage.ini.pEntries[nCount];
		entry = {};
		memcpy( entry.szSection, "CVars", 6 );
		uint32_t nNameBytes = 0;
		if ( m_pPool->decodeInto( pDescriptor->atom, entry.szKey, sizeof( entry.szKey ), nNameBytes ) != SIRStringStatus::SIR_STRING_OK ) {
			return SIRConfigResult::SIR_CONFIG_INVALID_HANDLE;
		}
		sirCVarValue_t value {};
		const SIRRegistrationResult eRead = m_pCVars->getValue( binding.cvar, nCapabilities, &value );
		if ( eRead != SIRRegistrationResult::SIR_REGISTRATION_OK ) {
			return sirCVarConfigStatus( eRead );
		}
		entry.pszValue = m_Storage.ini.pszValues + static_cast<size_t>(nCount) * m_Storage.ini.nValueStride;
		sirCVarTextBuffer_t output = { entry.pszValue, m_Storage.ini.nValueStride, 0 };
		const SIRRegistrationResult eFormat = sirCVarFormatText( static_cast<SIRCVarType>(pDescriptor->nType), value, &output );
		entry.nValueBytes = output.nLength;
		if ( eFormat != SIRRegistrationResult::SIR_REGISTRATION_OK ) {
			return sirCVarConfigStatus( eFormat );
		}
		entry.bQuoted = true;
		++nCount;
	}
	for ( uint32_t i = 1; i < nCount; ++i ) {
		const sirIniEntry_t saved = m_Storage.ini.pEntries[i];
		uint32_t j = i;
		while ( j != 0 && strcmp( m_Storage.ini.pEntries[j - 1].szKey, saved.szKey ) > 0 ) {
			m_Storage.ini.pEntries[j] = m_Storage.ini.pEntries[j - 1];
			--j;
		}
		m_Storage.ini.pEntries[j] = saved;
	}
	const SIRConfigResult eWrite = SIRIniCodec::write( m_Storage.ini.pEntries, nCount, sink, nMaxBytes );
	if ( eWrite == SIRConfigResult::SIR_CONFIG_OK ) {
		*pWrittenEntries = nCount;
	}
	return eWrite;
}

void SIRCVarConfigAdapter::shutdown( void ) noexcept
{
	m_pRegistration = nullptr;
	m_pCVars = nullptr;
	m_pPool = nullptr;
	m_Storage = {};
}
