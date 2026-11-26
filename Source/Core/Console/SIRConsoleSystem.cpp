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

#include "Core/Systems/SIREngineSystem.h"
#include <Core/Console/SIRConsoleSystem.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

SIRConsoleSystem::SIRConsoleSystem( const sirConsoleSystemConfig_t& config ) noexcept
    : m_Config( config )
{
}

SIRConsoleSystem::~SIRConsoleSystem() noexcept = default;

bool SIRConsoleSystem::isInitialized( void ) const noexcept
{
	return m_bInitialized;
}

SIRSystemResult SIRConsoleSystem::initialize( void ) noexcept
{
	if ( m_bInitialized ) {
		return SIRSystemResult::SIR_SYSTEM_BUSY;
	}
	if ( m_Config.pPool == nullptr ) {
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}
	if ( m_Config.nCVarCapacity == 0 || m_Config.nCVarCapacity > 256 ) {
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}
	if ( m_Config.nCommandCapacity == 0 || m_Config.nCommandCapacity > 64 ) {
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}
	if ( m_Config.nCommandBytes == 0 || m_Config.nCommandBytes > 512 || m_Config.nOverrideCapacity > 512 ) {
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}

	m_nCVarCapacity = m_Config.nCVarCapacity;
	m_nCommandCapacity = m_Config.nCommandCapacity;
	m_nCommandBytes = m_Config.nCommandBytes;
	m_nOverrideCapacity = m_Config.nOverrideCapacity;
	m_nRevision = 0;
	m_nSerial = 1;
	m_nNextRequest = 1;
	memset( m_bActive, 0, sizeof( m_bActive ) );
	memset( m_bOverrideActive, 0, sizeof( m_bOverrideActive ) );
	m_nCommandRead.store( 0, std::memory_order_relaxed );
	m_nCommandWrite.store( 0, std::memory_order_relaxed );
	if ( m_Config.pLogger != nullptr && !m_Config.bHeadless ) {
		if ( !m_LogSink.initialize( m_Config.nLogCapacity ) || m_Config.pLogger->attachSink( m_LogSink ) != sirLogSinkResult::SIR_LOG_SINK_OK ) {
			m_LogSink.reset();
			return SIRSystemResult::SIR_SYSTEM_ERROR;
		}
	}
	m_bStopping = false;
	m_bInitialized = true;
	return SIRSystemResult::SIR_SYSTEM_OK;
}

SIRSystemResult SIRConsoleSystem::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIRSystemResult::SIR_SYSTEM_OK;
	}
	m_bStopping = true;
	if ( m_Config.pLogger != nullptr && !m_Config.bHeadless && m_Config.pLogger->detachSink( m_LogSink ) == sirLogSinkResult::SIR_LOG_SINK_BUSY ) {
		return SIRSystemResult::SIR_SYSTEM_BUSY;
	}
	m_bInitialized = false;
	m_bStopping = false;
	m_LogSink.reset();
	return SIRSystemResult::SIR_SYSTEM_OK;
}

void SIRConsoleSystem::update( float fDeltaSeconds ) noexcept
{
	static_cast<void>( fDeltaSeconds );
	if ( !m_bInitialized ) {
		return;
	}
	static_cast<void>( pumpCommands( UINT32_MAX ) );
	if ( m_Config.pLogger != nullptr && !m_Config.bHeadless ) {
		sirLogRecord_t records[16]{};
		static_cast<void>( m_LogSink.drain( records, 16 ) );
	}
}

uint32_t SIRConsoleSystem::sourcePriority( SIRCVarSource eSource ) const noexcept
{
	return static_cast<uint32_t>( eSource );
}

SIRConsoleResult SIRConsoleSystem::findEntry( SIRStringAtom name, uint32_t& nSlot ) const noexcept
{
	if ( !m_bInitialized || !name.isValid() ) {
		return m_bInitialized ? SIRConsoleResult::SIR_CONSOLE_INVALID : SIRConsoleResult::SIR_CONSOLE_NOT_READY;
	}
	for ( uint32_t i = 0; i < m_nCVarCapacity; ++i ) {
		if ( m_bActive[i] && m_NameAtoms[i] == name ) {
			nSlot = i;
			return SIRConsoleResult::SIR_CONSOLE_OK;
		}
	}
	return SIRConsoleResult::SIR_CONSOLE_NOT_FOUND;
}

SIRConsoleResult SIRConsoleSystem::validateCVar( sirCVarHandle_t handle, uint32_t& nSlot ) const noexcept
{
	if ( !m_bInitialized ) {
		return SIRConsoleResult::SIR_CONSOLE_NOT_READY;
	}
	if ( handle.nSlot >= m_nCVarCapacity || !m_bActive[handle.nSlot] || m_nKind[handle.nSlot] != 1 || m_nGeneration[handle.nSlot] != handle.nGeneration ) {
		return SIRConsoleResult::SIR_CONSOLE_STALE_HANDLE;
	}
	nSlot = handle.nSlot;
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::registerScalar( SIRStringAtom name, const void *pValue, ScalarType eType, uint32_t nFlags, sirCVarHandle_t& outHandle ) noexcept
{
	if ( !m_bInitialized || !name.isValid() || pValue == nullptr ) {
		return m_bInitialized ? SIRConsoleResult::SIR_CONSOLE_INVALID : SIRConsoleResult::SIR_CONSOLE_NOT_READY;
	}
	uint32_t nExisting = 0;
	if ( findEntry( name, nExisting ) == SIRConsoleResult::SIR_CONSOLE_OK ) {
		return SIRConsoleResult::SIR_CONSOLE_BUSY;
	}
	uint32_t nSlot = UINT32_MAX;
	for ( uint32_t i = 0; i < m_nCVarCapacity; ++i ) {
		if ( !m_bActive[i] ) {
			nSlot = i;
			break;
		}
	}
	if ( nSlot == UINT32_MAX ) {
		return SIRConsoleResult::SIR_CONSOLE_CAPACITY;
	}
	m_bActive[nSlot] = true;
	m_nKind[nSlot] = 1;
	m_nGeneration[nSlot] = m_nGeneration[nSlot] == 0 ? 1 : m_nGeneration[nSlot];
	m_NameAtoms[nSlot] = name;
	m_nFlags[nSlot] = nFlags;
	m_nScalarType[nSlot] = static_cast<uint8_t>( eType );
	if ( eType == ScalarType::SIR_I64 || eType == ScalarType::SIR_U64 || eType == ScalarType::SIR_F64 ) {
		memcpy( &m_WideValues[nSlot], pValue, eType == ScalarType::SIR_F64 ? sizeof( double ) : sizeof( uint64_t ) );
	} else {
		memcpy( &m_NarrowValues[nSlot], pValue, eType == ScalarType::SIR_F32 ? sizeof( float ) : eType == ScalarType::SIR_BOOL ? sizeof( bool ) : sizeof( uint32_t ) );
	}
	outHandle = { nSlot, m_nGeneration[nSlot] };
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::getScalar( sirCVarHandle_t handle, ScalarType eType, void *pValue ) const noexcept
{
	if ( pValue == nullptr ) {
		return SIRConsoleResult::SIR_CONSOLE_INVALID;
	}
	uint32_t nSlot = 0;
	const SIRConsoleResult eResult = validateCVar( handle, nSlot );
	if ( eResult != SIRConsoleResult::SIR_CONSOLE_OK ) {
		return eResult;
	}
	if ( m_nScalarType[nSlot] != static_cast<uint8_t>( eType ) ) {
		return SIRConsoleResult::SIR_CONSOLE_TYPE_MISMATCH;
	}
	if ( eType == ScalarType::SIR_I64 || eType == ScalarType::SIR_U64 || eType == ScalarType::SIR_F64 ) {
		memcpy( pValue, &m_WideValues[nSlot], eType == ScalarType::SIR_F64 ? sizeof( double ) : sizeof( uint64_t ) );
	} else {
		memcpy( pValue, &m_NarrowValues[nSlot], eType == ScalarType::SIR_F32 ? sizeof( float ) : eType == ScalarType::SIR_BOOL ? sizeof( bool ) : sizeof( uint32_t ) );
	}
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::stageValue( uint32_t nSlot, const void *pValue, ScalarType eType, SIRCVarSource eSource, uint32_t nIssuerId, bool bRemove ) noexcept
{
	if ( ( m_nFlags[nSlot] & SIR_CVAR_FLAG_READ_ONLY ) != 0 || ( ( m_nFlags[nSlot] & SIR_CVAR_FLAG_DEVELOPMENT ) != 0 && nIssuerId == 0 ) ||
	     ( ( m_nFlags[nSlot] & SIR_CVAR_FLAG_CHEAT ) != 0 && nIssuerId == 0 ) ) {
		return SIRConsoleResult::SIR_CONSOLE_ACCESS_DENIED;
	}
	uint32_t nOverride = UINT32_MAX;
	for ( uint32_t i = 0; i < m_nOverrideCapacity; ++i ) {
		if ( m_bOverrideActive[i] && m_nOverrideSlots[i] == nSlot && m_nOverrideIssuers[i] == nIssuerId && m_nOverrideSources[i] == static_cast<uint8_t>( eSource ) ) {
			nOverride = i;
			break;
		}
	}
	if ( bRemove ) {
		if ( nOverride != UINT32_MAX ) {
			m_bOverrideActive[nOverride] = false;
		}
	} else {
		if ( nOverride == UINT32_MAX ) {
			for ( uint32_t i = 0; i < m_nOverrideCapacity; ++i ) {
				if ( !m_bOverrideActive[i] ) {
					nOverride = i;
					break;
				}
			}
			if ( nOverride == UINT32_MAX ) {
				return SIRConsoleResult::SIR_CONSOLE_CAPACITY;
			}
		}
		m_bOverrideActive[nOverride] = true;
		m_nOverrideSlots[nOverride] = nSlot;
		m_nOverrideIssuers[nOverride] = nIssuerId;
		m_nOverrideSources[nOverride] = static_cast<uint8_t>( eSource );
		m_nOverrideTypes[nOverride] = static_cast<uint8_t>( eType );
		m_OverrideWide[nOverride] = 0;
		m_OverrideNarrow[nOverride] = 0;
		if ( eType == ScalarType::SIR_I64 || eType == ScalarType::SIR_U64 || eType == ScalarType::SIR_F64 ) {
			memcpy( &m_OverrideWide[nOverride], pValue, eType == ScalarType::SIR_F64 ? sizeof( double ) : sizeof( uint64_t ) );
		} else {
			memcpy( &m_OverrideNarrow[nOverride], pValue, eType == ScalarType::SIR_F32 ? sizeof( float ) : eType == ScalarType::SIR_BOOL ? sizeof( bool ) : sizeof( uint32_t ) );
		}
	}
	applyPending( nSlot );
	++m_nRevision;
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

void SIRConsoleSystem::applyPending( uint32_t nSlot ) noexcept
{
	uint32_t nWinner = UINT32_MAX;
	for ( uint32_t i = 0; i < m_nOverrideCapacity; ++i ) {
		if ( !m_bOverrideActive[i] || m_nOverrideSlots[i] != nSlot ) {
			continue;
		}
		if ( nWinner == UINT32_MAX ||
		     sourcePriority( static_cast<SIRCVarSource>( m_nOverrideSources[i] ) ) >= sourcePriority( static_cast<SIRCVarSource>( m_nOverrideSources[nWinner] ) ) ) {
			nWinner = i;
		}
	}
	if ( nWinner == UINT32_MAX ) {
		return;
	}
	if ( m_nOverrideTypes[nWinner] == static_cast<uint8_t>( ScalarType::SIR_I64 ) || m_nOverrideTypes[nWinner] == static_cast<uint8_t>( ScalarType::SIR_U64 ) ||
	     m_nOverrideTypes[nWinner] == static_cast<uint8_t>( ScalarType::SIR_F64 ) ) {
		m_WideValues[nSlot] = m_OverrideWide[nWinner];
	} else {
		m_NarrowValues[nSlot] = m_OverrideNarrow[nWinner];
	}
}

SIRConsoleResult SIRConsoleSystem::setScalar( sirCVarHandle_t handle, const void *pValue, ScalarType eType, SIRCVarSource eSource, uint32_t nIssuerId ) noexcept
{
	if ( pValue == nullptr ) {
		return SIRConsoleResult::SIR_CONSOLE_INVALID;
	}
	uint32_t nSlot = 0;
	const SIRConsoleResult eResult = validateCVar( handle, nSlot );
	if ( eResult != SIRConsoleResult::SIR_CONSOLE_OK ) {
		return eResult;
	}
	if ( m_nScalarType[nSlot] != static_cast<uint8_t>( eType ) ) {
		return SIRConsoleResult::SIR_CONSOLE_TYPE_MISMATCH;
	}
	return stageValue( nSlot, pValue, eType, eSource, nIssuerId, false );
}

SIRConsoleResult SIRConsoleSystem::getStringCVar( sirCVarHandle_t handle, sirStringView_t& outValue ) const noexcept
{
	uint32_t nSlot = 0;
	const SIRConsoleResult eResult = validateCVar( handle, nSlot );
	if ( eResult != SIRConsoleResult::SIR_CONSOLE_OK ) {
		return eResult;
	}
	if ( m_nScalarType[nSlot] != 7 ) {
		return SIRConsoleResult::SIR_CONSOLE_TYPE_MISMATCH;
	}
	outValue = { m_StringValues[nSlot], static_cast<uint32_t>( strlen( m_StringValues[nSlot] ) ) };
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::registerStringCVar( SIRStringAtom name, sirStringView_t initialValue, uint32_t nFlags, sirCVarHandle_t& outHandle ) noexcept
{
	if ( !m_bInitialized || !name.isValid() || initialValue.pBytes == nullptr || initialValue.nBytes >= 256 ) {
		return m_bInitialized ? SIRConsoleResult::SIR_CONSOLE_INVALID : SIRConsoleResult::SIR_CONSOLE_NOT_READY;
	}
	uint32_t nExisting = 0;
	if ( findEntry( name, nExisting ) == SIRConsoleResult::SIR_CONSOLE_OK ) {
		return SIRConsoleResult::SIR_CONSOLE_BUSY;
	}
	uint32_t nSlot = UINT32_MAX;
	for ( uint32_t i = 0; i < m_nCVarCapacity; ++i ) {
		if ( !m_bActive[i] ) {
			nSlot = i;
			break;
		}
	}
	if ( nSlot == UINT32_MAX ) {
		return SIRConsoleResult::SIR_CONSOLE_CAPACITY;
	}
	m_bActive[nSlot] = true;
	m_nKind[nSlot] = 1;
	m_nGeneration[nSlot] = m_nGeneration[nSlot] == 0 ? 1 : m_nGeneration[nSlot];
	m_NameAtoms[nSlot] = name;
	m_nFlags[nSlot] = nFlags;
	m_nScalarType[nSlot] = 7;
	memcpy( m_StringValues[nSlot], initialValue.pBytes, initialValue.nBytes );
	m_StringValues[nSlot][initialValue.nBytes] = '\0';
	outHandle = { nSlot, m_nGeneration[nSlot] };
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::setStringCVar( sirCVarHandle_t handle, sirStringView_t value, SIRCVarSource eSource, uint32_t nIssuerId ) noexcept
{
	uint32_t nSlot = 0;
	const SIRConsoleResult eResult = validateCVar( handle, nSlot );
	if ( eResult != SIRConsoleResult::SIR_CONSOLE_OK || value.pBytes == nullptr || value.nBytes >= 256 ) {
		return eResult == SIRConsoleResult::SIR_CONSOLE_OK ? SIRConsoleResult::SIR_CONSOLE_INVALID : eResult;
	}
	if ( ( m_nFlags[nSlot] & SIR_CVAR_FLAG_READ_ONLY ) != 0 || ( ( m_nFlags[nSlot] & SIR_CVAR_FLAG_DEVELOPMENT ) != 0 && nIssuerId == 0 ) ||
	     ( ( m_nFlags[nSlot] & SIR_CVAR_FLAG_CHEAT ) != 0 && nIssuerId == 0 ) ) {
		return SIRConsoleResult::SIR_CONSOLE_ACCESS_DENIED;
	}
	memcpy( m_StringValues[nSlot], value.pBytes, value.nBytes );
	m_StringValues[nSlot][value.nBytes] = '\0';
	static_cast<void>( eSource );
	++m_nRevision;
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::unregisterCVar( sirCVarHandle_t handle ) noexcept
{
	uint32_t nSlot = 0;
	const SIRConsoleResult eResult = validateCVar( handle, nSlot );
	if ( eResult != SIRConsoleResult::SIR_CONSOLE_OK ) {
		return eResult;
	}
	m_bActive[nSlot] = false;
	++m_nGeneration[nSlot];
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::unregisterModule( uint32_t nModuleId ) noexcept
{
	if ( nModuleId == 0 ) {
		return SIRConsoleResult::SIR_CONSOLE_INVALID;
	}
	for ( uint32_t i = 0; i < m_nCVarCapacity; ++i ) {
		if ( m_bActive[i] && m_nOwners[i] == nModuleId ) {
			m_bActive[i] = false;
			++m_nGeneration[i];
		}
	}
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::registerCommandBinding( SIRStringAtom name, void *pOwner, CommandInvoker pInvoker, sirCommandHandle_t& outHandle ) noexcept
{
	if ( !m_bInitialized || !name.isValid() || pInvoker == nullptr || pOwner == nullptr ) {
		return SIRConsoleResult::SIR_CONSOLE_INVALID;
	}
	uint32_t nExisting = 0;
	if ( findEntry( name, nExisting ) == SIRConsoleResult::SIR_CONSOLE_OK ) {
		return SIRConsoleResult::SIR_CONSOLE_BUSY;
	}
	uint32_t nSlot = UINT32_MAX;
	for ( uint32_t i = 0; i < m_nCVarCapacity; ++i ) {
		if ( !m_bActive[i] ) {
			nSlot = i;
			break;
		}
	}
	if ( nSlot == UINT32_MAX ) {
		return SIRConsoleResult::SIR_CONSOLE_CAPACITY;
	}
	m_bActive[nSlot] = true;
	m_nKind[nSlot] = 2;
	m_nGeneration[nSlot] = m_nGeneration[nSlot] == 0 ? 1 : m_nGeneration[nSlot];
	m_NameAtoms[nSlot] = name;
	m_pOwners[nSlot] = pOwner;
	m_pCommandInvokers[nSlot] = pInvoker;
	outHandle = { nSlot, m_nGeneration[nSlot] };
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::parseAndSet( SIRStringAtom name, sirStringView_t value, SIRCVarSource eSource, uint32_t nIssuerId ) noexcept
{
	uint32_t nSlot = 0;
	const SIRConsoleResult eFind = findEntry( name, nSlot );
	if ( eFind != SIRConsoleResult::SIR_CONSOLE_OK || m_nKind[nSlot] != 1 ) {
		return eFind == SIRConsoleResult::SIR_CONSOLE_OK ? SIRConsoleResult::SIR_CONSOLE_TYPE_MISMATCH : eFind;
	}
	char szValue[256]{};
	if ( value.pBytes == nullptr || value.nBytes >= sizeof( szValue ) ) {
		return SIRConsoleResult::SIR_CONSOLE_INVALID;
	}
	memcpy( szValue, value.pBytes, value.nBytes );
	szValue[value.nBytes] = '\0';
	const ScalarType eType = static_cast<ScalarType>( m_nScalarType[nSlot] );
	if ( eType == ScalarType::SIR_U32 || eType == ScalarType::SIR_BOOL ) {
		const uint32_t nValue = static_cast<uint32_t>( strtoul( szValue, nullptr, 0 ) );
		return setScalar( { nSlot, m_nGeneration[nSlot] }, &nValue, eType, eSource, nIssuerId );
	}
	if ( eType == ScalarType::SIR_I32 ) {
		const int32_t nValue = static_cast<int32_t>( strtol( szValue, nullptr, 0 ) );
		return setScalar( { nSlot, m_nGeneration[nSlot] }, &nValue, eType, eSource, nIssuerId );
	}
	if ( eType == ScalarType::SIR_F32 ) {
		const float fValue = strtof( szValue, nullptr );
		return setScalar( { nSlot, m_nGeneration[nSlot] }, &fValue, eType, eSource, nIssuerId );
	}
	if ( eType == ScalarType::SIR_I64 ) {
		const int64_t nValue = strtoll( szValue, nullptr, 0 );
		return setScalar( { nSlot, m_nGeneration[nSlot] }, &nValue, eType, eSource, nIssuerId );
	}
	if ( eType == ScalarType::SIR_U64 ) {
		const uint64_t nValue = strtoull( szValue, nullptr, 0 );
		return setScalar( { nSlot, m_nGeneration[nSlot] }, &nValue, eType, eSource, nIssuerId );
	}
	if ( eType == ScalarType::SIR_F64 ) {
		const double fValue = strtod( szValue, nullptr );
		return setScalar( { nSlot, m_nGeneration[nSlot] }, &fValue, eType, eSource, nIssuerId );
	}
	return setStringCVar( { nSlot, m_nGeneration[nSlot] }, value, eSource, nIssuerId );
}

SIRConsoleResult SIRConsoleSystem::executeCommand( sirStringView_t text ) noexcept
{
	if ( !m_bInitialized || text.pBytes == nullptr || text.nBytes == 0 || text.nBytes > m_nCommandBytes ) {
		return m_bInitialized ? SIRConsoleResult::SIR_CONSOLE_INVALID : SIRConsoleResult::SIR_CONSOLE_NOT_READY;
	}
	sirStringView_t tokens[32]{};
	uint32_t nTokenCount = 0;
	uint32_t i = 0;
	while ( i < text.nBytes ) {
		while ( i < text.nBytes && text.pBytes[i] == ' ' )
			++i;
		if ( i == text.nBytes )
			break;
		if ( nTokenCount == 32 )
			return SIRConsoleResult::SIR_CONSOLE_CAPACITY;
		const uint32_t nStart = i;
		while ( i < text.nBytes && text.pBytes[i] != ' ' )
			++i;
		tokens[nTokenCount++] = { text.pBytes + nStart, i - nStart };
	}
	if ( nTokenCount == 0 )
		return SIRConsoleResult::SIR_CONSOLE_INVALID;
	char szName[256]{};
	memcpy( szName, tokens[0].pBytes, tokens[0].nBytes );
	szName[tokens[0].nBytes] = '\0';
	SIRStringAtom name = m_Config.pPool->find( szName, tokens[0].nBytes );
	if ( tokens[0].nBytes == 3 && memcmp( szName, "set", 3 ) == 0 ) {
		if ( nTokenCount < 3 )
			return SIRConsoleResult::SIR_CONSOLE_INVALID;
		memcpy( szName, tokens[1].pBytes, tokens[1].nBytes );
		szName[tokens[1].nBytes] = '\0';
		return parseAndSet( m_Config.pPool->find( szName, tokens[1].nBytes ), tokens[2], SIRCVarSource::SIR_CVAR_SESSION_CONSOLE, 1 );
	}
	uint32_t nSlot = 0;
	const SIRConsoleResult eFind = findEntry( name, nSlot );
	if ( eFind != SIRConsoleResult::SIR_CONSOLE_OK )
		return eFind;
	if ( m_nKind[nSlot] == 2 ) {
		return m_pCommandInvokers[nSlot]( m_pOwners[nSlot], SIRSpan<const sirStringView_t>( tokens + 1, nTokenCount - 1 ) );
	}
	if ( nTokenCount == 1 )
		return SIRConsoleResult::SIR_CONSOLE_OK;
	return parseAndSet( name, tokens[1], SIRCVarSource::SIR_CVAR_SESSION_CONSOLE, 1 );
}

SIRConsoleResult SIRConsoleSystem::submitCommand( sirStringView_t text ) noexcept
{
	if ( !m_bInitialized || m_bStopping || text.pBytes == nullptr || text.nBytes == 0 || text.nBytes > m_nCommandBytes ) {
		return SIRConsoleResult::SIR_CONSOLE_NOT_READY;
	}
	const uint32_t nWrite = m_nCommandWrite.fetch_add( 1, std::memory_order_acq_rel );
	const uint32_t nRead = m_nCommandRead.load( std::memory_order_acquire );
	if ( nWrite - nRead >= m_nCommandCapacity ) {
		m_nCommandWrite.fetch_sub( 1, std::memory_order_release );
		return SIRConsoleResult::SIR_CONSOLE_QUEUE_FULL;
	}
	memcpy( m_CommandText[nWrite % m_nCommandCapacity], text.pBytes, text.nBytes );
	m_CommandText[nWrite % m_nCommandCapacity][text.nBytes] = '\0';
	m_nCommandLengths[nWrite % m_nCommandCapacity] = text.nBytes;
	return SIRConsoleResult::SIR_CONSOLE_OK;
}

SIRConsoleResult SIRConsoleSystem::pumpCommands( uint32_t nBudget ) noexcept
{
	uint32_t nCount = 0;
	while ( nCount < nBudget ) {
		const uint32_t nRead = m_nCommandRead.load( std::memory_order_relaxed );
		if ( nRead == m_nCommandWrite.load( std::memory_order_acquire ) )
			break;
		static_cast<void>( executeCommand( { m_CommandText[nRead % m_nCommandCapacity], m_nCommandLengths[nRead % m_nCommandCapacity] } ) );
		m_nCommandRead.store( nRead + 1, std::memory_order_release );
		++nCount;
	}
	return SIRConsoleResult::SIR_CONSOLE_OK;
}
