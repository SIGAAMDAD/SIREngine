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

#include <Core/Console/SIRConsoleService.h>
#include <Core/CVars/SIRCVarValueCodec.h>
#include <cmath>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool sirConsoleSpace( char nCharacter ) noexcept
{
	return nCharacter == ' ' || nCharacter == '\t' || nCharacter == '\r';
}

static bool sirConsoleCaseEqual( const char *pszValue, const char *pszExpected ) noexcept
{
	while ( *pszValue != '\0' && *pszExpected != '\0' ) {
		char nValue = *pszValue;
		if ( nValue >= 'A' && nValue <= 'Z' ) {
			nValue = ( char )( nValue + ( 'a' - 'A' ) );
		}
		if ( nValue != *pszExpected ) {
			return false;
		}
		++pszValue;
		++pszExpected;
	}
	return *pszValue == '\0' && *pszExpected == '\0';
}

static bool sirConsolePrefix( const char *pszName, const char *pszPrefix, uint32_t nBytes ) noexcept
{
	for ( uint32_t iByte = 0; iByte < nBytes; ++iByte ) {
		char nCharacter = pszPrefix[iByte];
		if ( nCharacter >= 'A' && nCharacter <= 'Z' ) {
			nCharacter = ( char )( nCharacter + ( 'a' - 'A' ) );
		}
		if ( pszName[iByte] == '\0' || pszName[iByte] != nCharacter ) {
			return false;
		}
	}
	return true;
}

SIRConsoleService::SIRConsoleService( void ) noexcept
    : m_pRegistration( nullptr ), m_pCVars( nullptr ), m_pPool( nullptr ), m_Storage{}, m_nWriteTicket( 0 ), m_bAccepting( false ),
      m_OwnerThread{}, m_nReadTicket( 0 ), m_nHistoryCount( 0 ), m_nHistoryNext( 0 ), m_nOutputsCurrent( 0 ), m_nOutputOverflowCurrent( 0 ),
      m_nCurrentRequestId( 0 ), m_bPumping( false )
{
}

SIRConsoleService::~SIRConsoleService( void ) noexcept
{
	shutdown();
}

bool SIRConsoleService::isInitialized( void ) const noexcept
{
	return m_bAccepting.load( std::memory_order_acquire );
}

uint32_t SIRConsoleService::getHistoryCount( void ) const noexcept
{
	return ownerThread() && isInitialized() ? m_nHistoryCount : 0;
}

bool SIRConsoleService::ownerThread( void ) const noexcept
{
	return m_OwnerThread == std::this_thread::get_id();
}

sirRegistrationResult_t SIRConsoleService::initialize( SIRRegistrationCatalog& registration, SIRCVarRegistry& cvars, SIRStringPool& pool,
                                                       const sirConsoleStorage_t& storage ) noexcept
{
	if ( isInitialized() ) {
		return SIR_REGISTRATION_BAD_SCHEMA;
	}
	if ( !registration.isInitialized() || !cvars.isInitialized() || !pool.isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( storage.pRequests == nullptr || storage.pszRequestText == nullptr || storage.nRequestCapacity == 0 || storage.nInputStride < 2 ||
	     storage.nInputStride > 4096 || storage.nMaxCommandsPerPump == 0 || storage.nMaxOutputsPerCommand == 0 ||
	     storage.nMaxOutputsPerCommand > 4096 ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( storage.nRequestCapacity > SIZE_MAX / storage.nInputStride ||
	     storage.nRequestTextBytes < ( size_t )storage.nRequestCapacity * storage.nInputStride ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( storage.nHistoryCapacity > SIZE_MAX / storage.nInputStride ||
	     storage.nHistoryTextBytes < ( size_t )storage.nHistoryCapacity * storage.nInputStride ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( storage.nHistoryCapacity != 0 && ( storage.pHistory == nullptr || storage.pszHistoryText == nullptr ) ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( storage.nCompletionCapacity != 0 && storage.pCompletionScratch == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( storage.nCVarBindings != 0 && storage.pCVarBindings == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( ( storage.pfnAcquireModule == nullptr ) != ( storage.pfnReleaseModule == nullptr ) ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < storage.nCVarBindings; ++iBinding ) {
		const sirConsoleCVarBinding_t& binding = storage.pCVarBindings[iBinding];
		const sirRegistrationEntry_t *pEntry = nullptr;
		if ( registration.getEntry( binding.registration, &pEntry ) != SIR_REGISTRATION_OK || pEntry->eKind != SIR_REGISTRATION_CVAR ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iBinding; ++iPrevious ) {
			const sirConsoleCVarBinding_t& previous = storage.pCVarBindings[iPrevious];
			if ( previous.registration.nSlot == binding.registration.nSlot ) {
				return SIR_REGISTRATION_DUPLICATE_NAME;
			}
		}
	}
	m_pRegistration = &registration;
	m_pCVars = &cvars;
	m_pPool = &pool;
	m_Storage = storage;
	m_OwnerThread = std::this_thread::get_id();
	m_nReadTicket = 0;
	m_nWriteTicket.store( 0, std::memory_order_relaxed );
	m_nHistoryCount = 0;
	m_nHistoryNext = 0;
	for ( uint32_t iSlot = 0; iSlot < storage.nRequestCapacity; ++iSlot ) {
		storage.pRequests[iSlot].nSequence.store( iSlot, std::memory_order_relaxed );
		storage.pRequests[iSlot].nBytes = 0;
	}
	m_bAccepting.store( true, std::memory_order_release );
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRConsoleService::submit( const char *pszInput, uint32_t nBytes,
                                                   const sirConsoleSubmission_t& submission ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( pszInput == nullptr || nBytes == 0 || nBytes >= m_Storage.nInputStride || submission.nRequestId == 0 ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	for ( uint32_t iByte = 0; iByte < nBytes; ++iByte ) {
		if ( pszInput[iByte] == '\0' || pszInput[iByte] == '\n' ) {
			return SIR_REGISTRATION_INVALID_ARGUMENT;
		}
	}

	uint64_t nTicket = m_nWriteTicket.load( std::memory_order_relaxed );
	while ( true ) {
		if ( nTicket >= UINT64_MAX - m_Storage.nRequestCapacity ) {
			return SIR_REGISTRATION_CAPACITY_EXCEEDED;
		}
		sirConsoleRequestSlot_t& slot = m_Storage.pRequests[nTicket % m_Storage.nRequestCapacity];
		const uint64_t nSequence = slot.nSequence.load( std::memory_order_acquire );
		if ( nSequence < nTicket ) {
			return SIR_REGISTRATION_QUEUE_FULL;
		}
		if ( nSequence != nTicket ) {
			nTicket = m_nWriteTicket.load( std::memory_order_relaxed );
			continue;
		}
		if ( m_nWriteTicket.compare_exchange_weak( nTicket, nTicket + 1, std::memory_order_relaxed, std::memory_order_relaxed ) ) {
			char *pszStored = m_Storage.pszRequestText + ( size_t )( nTicket % m_Storage.nRequestCapacity ) * m_Storage.nInputStride;
			memcpy( pszStored, pszInput, nBytes );
			pszStored[nBytes] = '\0';
			slot.submission = submission;
			slot.nBytes = nBytes;
			slot.nSequence.store( nTicket + 1, std::memory_order_release );
			return SIR_REGISTRATION_OK;
		}
	}
}

sirRegistrationResult_t SIRConsoleService::tokenize( char *pszInput, uint32_t nBytes, sirConsoleArguments_t *pArgs ) const noexcept
{
	pArgs->nCount = 0;
	uint32_t iRead = 0;
	uint32_t iWrite = 0;
	while ( iRead < nBytes ) {
		while ( iRead < nBytes && sirConsoleSpace( pszInput[iRead] ) ) {
			++iRead;
		}
		if ( iRead == nBytes ) {
			break;
		}
		if ( pArgs->nCount == SIR_CONSOLE_MAX_ARGS ) {
			return SIR_REGISTRATION_CAPACITY_EXCEEDED;
		}
		const uint32_t nStart = iWrite;
		bool bQuoted = false;
		bool bOpened = false;
		while ( iRead < nBytes ) {
			const char nCharacter = pszInput[iRead];
			if ( !bQuoted && sirConsoleSpace( nCharacter ) ) {
				break;
			}
			if ( nCharacter == ';' || nCharacter == '\n' ) {
				return SIR_REGISTRATION_BAD_SCHEMA;
			}
			if ( nCharacter == '"' ) {
				bQuoted = !bQuoted;
				bOpened = true;
				++iRead;
				continue;
			}
			if ( nCharacter == '\\' ) {
				if ( ++iRead == nBytes ) {
					return SIR_REGISTRATION_BAD_SCHEMA;
				}
				const char nEscaped = pszInput[iRead];
				if ( nEscaped != '"' && nEscaped != '\\' && nEscaped != ' ' && nEscaped != 't' ) {
					return SIR_REGISTRATION_BAD_SCHEMA;
				}
				pszInput[iWrite++] = nEscaped == 't' ? '\t' : nEscaped;
				++iRead;
				continue;
			}
			pszInput[iWrite++] = nCharacter;
			++iRead;
		}
		if ( bQuoted || ( iWrite == nStart && !bOpened ) ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		pArgs->szArgs[pArgs->nCount].pszText = pszInput + nStart;
		pArgs->szArgs[pArgs->nCount].nBytes = iWrite - nStart;
		++pArgs->nCount;
		while ( iRead < nBytes && sirConsoleSpace( pszInput[iRead] ) ) {
			++iRead;
		}
		pszInput[iWrite++] = '\0';
	}
	return pArgs->nCount == 0 ? SIR_REGISTRATION_INVALID_ARGUMENT : SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRConsoleService::writeOutput( const sirConsoleOutput_t& output ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( !m_bPumping || output.nRequestId != m_nCurrentRequestId || ( output.pszText == nullptr && output.nBytes != 0 ) ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( m_nOutputsCurrent >= m_Storage.nMaxOutputsPerCommand ) {
		++m_nOutputOverflowCurrent;
		return SIR_REGISTRATION_QUEUE_FULL;
	}
	++m_nOutputsCurrent;
	if ( m_Storage.pfnOutput != nullptr ) {
		m_Storage.pfnOutput( m_Storage.pOutputContext, output );
	}
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRConsoleService::outputText( uint64_t nRequestId, sirRegistrationResult_t eResult, const char *pszText ) noexcept
{
	const uint32_t nBytes = pszText == nullptr ? 0 : ( uint32_t )strlen( pszText );
	const sirConsoleOutput_t output = { nRequestId, eResult, pszText, nBytes };
	return writeOutput( output );
}

void SIRConsoleService::addHistory( const sirConsoleRequestSlot_t& slot, const char *pszInput ) noexcept
{
	if ( m_Storage.nHistoryCapacity == 0 ) {
		return;
	}
	const uint32_t iHistory = m_nHistoryNext;
	char *pszDestination = m_Storage.pszHistoryText + ( size_t )iHistory * m_Storage.nInputStride;
	memcpy( pszDestination, pszInput, slot.nBytes );
	pszDestination[slot.nBytes] = '\0';
	m_Storage.pHistory[iHistory] = { slot.submission.nRequestId, slot.nBytes };
	m_nHistoryNext = ( m_nHistoryNext + 1 ) % m_Storage.nHistoryCapacity;
	if ( m_nHistoryCount < m_Storage.nHistoryCapacity ) {
		++m_nHistoryCount;
	}
}

sirRegistrationResult_t SIRConsoleService::readHistory( uint32_t iNewest, sirConsoleHistoryView_t *pResult ) const noexcept
{
	if ( pResult == nullptr || pResult->pszOutput == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	pResult->nWritten = 0;
	pResult->nRequestId = 0;
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( iNewest >= m_nHistoryCount ) {
		return SIR_REGISTRATION_NOT_FOUND;
	}
	const uint32_t iHistory = ( m_nHistoryNext + m_Storage.nHistoryCapacity - 1 - iNewest ) % m_Storage.nHistoryCapacity;
	const sirConsoleHistoryEntry_t& entry = m_Storage.pHistory[iHistory];
	if ( pResult->nCapacity <= entry.nBytes ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	const char *pszSource = m_Storage.pszHistoryText + ( size_t )iHistory * m_Storage.nInputStride;
	memcpy( pResult->pszOutput, pszSource, entry.nBytes + 1 );
	pResult->nWritten = entry.nBytes;
	pResult->nRequestId = entry.nRequestId;
	return SIR_REGISTRATION_OK;
}

const sirConsoleCVarBinding_t *SIRConsoleService::findBoundCVar( sirRegistrationHandle_t registration ) const noexcept
{
	for ( uint32_t iBinding = 0; iBinding < m_Storage.nCVarBindings; ++iBinding ) {
		const sirConsoleCVarBinding_t& binding = m_Storage.pCVarBindings[iBinding];
		if ( binding.registration.nSlot == registration.nSlot && binding.registration.nGeneration == registration.nGeneration ) {
			return &binding;
		}
	}
	return nullptr;
}

sirRegistrationResult_t SIRConsoleService::parseValue( sirCVarType_t eType, const sirConsoleArgument_t& arg,
                                                       sirCVarValue_t *pValue ) const noexcept
{
	return sirCVarParseText( eType, arg.pszText, arg.nBytes, pValue );
}

sirRegistrationResult_t SIRConsoleService::printValue( const sirConsoleSubmission_t& submission, sirCVarType_t eType,
                                                       const sirCVarValue_t& value ) noexcept
{
	if ( eType == SIR_CVAR_UTF8_STRING ) {
		const sirConsoleOutput_t output = { submission.nRequestId, SIR_REGISTRATION_OK, value.pszText, value.nTextBytes };
		return writeOutput( output );
	}
	char szValue[128] = {};
	sirCVarTextBuffer_t output = { szValue, sizeof( szValue ), 0 };
	const sirRegistrationResult_t eFormat = sirCVarFormatText( eType, value, &output );
	if ( eFormat != SIR_REGISTRATION_OK ) {
		return eFormat;
	}
	return outputText( submission.nRequestId, SIR_REGISTRATION_OK, szValue );
}

sirRegistrationResult_t SIRConsoleService::executeCVar( const sirConsoleSubmission_t& submission, sirConsoleArguments_t& args,
                                                        const char *pszOperation ) noexcept
{
	const bool bGet = strcmp( pszOperation, "get" ) == 0;
	const bool bSet = strcmp( pszOperation, "set" ) == 0;
	const bool bReset = strcmp( pszOperation, "reset" ) == 0;
	const bool bToggle = strcmp( pszOperation, "toggle" ) == 0;
	const uint32_t nExpected = bSet ? 3 : 2;
	if ( args.nCount != nExpected ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( ( bGet && ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_READ ) == 0 ) ||
	     ( !bGet && ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_WRITE ) == 0 ) ) {
		return SIR_REGISTRATION_DENIED;
	}
	sirRegistrationHandle_t registration = {};
	sirRegistrationResult_t eResult = m_pRegistration->find( args.szArgs[1].pszText, args.szArgs[1].nBytes, &registration );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	const sirRegistrationEntry_t *pEntry = nullptr;
	eResult = m_pRegistration->getEntry( registration, &pEntry );
	if ( eResult != SIR_REGISTRATION_OK || pEntry->eKind != SIR_REGISTRATION_CVAR ) {
		return eResult == SIR_REGISTRATION_OK ? SIR_REGISTRATION_TYPE_MISMATCH : eResult;
	}
	const sirConsoleCVarBinding_t *pBinding = findBoundCVar( registration );
	if ( pBinding == nullptr ) {
		return SIR_REGISTRATION_NOT_FOUND;
	}
	const sirCVarType_t eType = ( sirCVarType_t )pEntry->nType;
	if ( bGet ) {
		sirCVarValue_t value = {};
		eResult = m_pCVars->getValue( pBinding->cvar, submission.origin.nCapabilities, &value );
		return eResult == SIR_REGISTRATION_OK ? printValue( submission, eType, value ) : eResult;
	}
	sirCVarWrite_t write = {};
	write.handle = pBinding->cvar;
	write.origin = submission.origin;
	write.bRemove = bReset;
	if ( bSet ) {
		eResult = parseValue( eType, args.szArgs[2], &write.value );
		if ( eResult != SIR_REGISTRATION_OK ) {
			return eResult;
		}
	}
	if ( bToggle ) {
		if ( eType != SIR_CVAR_BOOL ) {
			return SIR_REGISTRATION_TYPE_MISMATCH;
		}
		sirCVarValue_t current = {};
		eResult = m_pCVars->getValue( pBinding->cvar, submission.origin.nCapabilities, &current );
		if ( eResult != SIR_REGISTRATION_OK ) {
			return eResult;
		}
		write.value.nBits = current.nBits == 0 ? 1 : 0;
	}
	uint64_t nToken = 0;
	eResult = m_pCVars->prepareBatch( &write, 1, &nToken );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	eResult = m_pCVars->commitBatch( nToken );
	return eResult == SIR_REGISTRATION_OK ? outputText( submission.nRequestId, SIR_REGISTRATION_OK, "ok" ) : eResult;
}

sirRegistrationResult_t SIRConsoleService::enumerate( const sirConsoleSubmission_t& submission, bool bCommandsOnly ) noexcept
{
	if ( ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_READ ) == 0 ) {
		return SIR_REGISTRATION_DENIED;
	}
	for ( uint32_t iEntry = 0; iEntry < m_pRegistration->getCapacity(); ++iEntry ) {
		const sirRegistrationEntry_t *pEntry = nullptr;
		sirRegistrationHandle_t handle = {};
		if ( m_pRegistration->getAtIndex( iEntry, &pEntry, &handle ) != SIR_REGISTRATION_OK ) {
			continue;
		}
		if ( bCommandsOnly && pEntry->eKind != SIR_REGISTRATION_COMMAND ) {
			continue;
		}
		if ( ( pEntry->nFlags & SIR_CVAR_FLAG_HIDDEN ) != 0 && ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_ADMIN ) == 0 ) {
			continue;
		}
		if ( ( pEntry->nFlags & SIR_CVAR_FLAG_SENSITIVE ) != 0 && ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_SENSITIVE ) == 0 ) {
			continue;
		}
		if ( pEntry->eKind == SIR_REGISTRATION_COMMAND &&
		     ( submission.origin.nCapabilities & pEntry->nCapabilities ) != pEntry->nCapabilities ) {
			continue;
		}
		char szName[SIR_REGISTRATION_MAX_NAME_BYTES + 1] = {};
		uint32_t nWritten = 0;
		if ( m_pPool->decodeInto( pEntry->atom, szName, sizeof( szName ), nWritten ) != SIR_STRING_OK ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		const sirRegistrationResult_t eOutput = outputText( submission.nRequestId, SIR_REGISTRATION_OK, szName );
		if ( eOutput != SIR_REGISTRATION_OK ) {
			return eOutput;
		}
	}
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRConsoleService::executeBuiltin( const sirConsoleSubmission_t& submission, sirConsoleArguments_t& args ) noexcept
{
	const char *pszName = args.szArgs[0].pszText;
	if ( sirConsoleCaseEqual( pszName, "get" ) ) {
		return executeCVar( submission, args, "get" );
	}
	if ( sirConsoleCaseEqual( pszName, "set" ) ) {
		return executeCVar( submission, args, "set" );
	}
	if ( sirConsoleCaseEqual( pszName, "reset" ) ) {
		return executeCVar( submission, args, "reset" );
	}
	if ( sirConsoleCaseEqual( pszName, "toggle" ) ) {
		return executeCVar( submission, args, "toggle" );
	}
	if ( sirConsoleCaseEqual( pszName, "list" ) ) {
		return args.nCount == 1 ? enumerate( submission, false ) : SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	if ( sirConsoleCaseEqual( pszName, "help" ) ) {
		if ( args.nCount == 1 ) {
			return outputText( submission.nRequestId, SIR_REGISTRATION_OK, "help list get set reset toggle exec (INI Config adapter)" );
		}
		if ( args.nCount != 2 ) {
			return SIR_REGISTRATION_INVALID_ARGUMENT;
		}
		sirRegistrationHandle_t handle = {};
		sirRegistrationResult_t eResult = m_pRegistration->find( args.szArgs[1].pszText, args.szArgs[1].nBytes, &handle );
		if ( eResult != SIR_REGISTRATION_OK ) {
			return eResult;
		}
		const sirRegistrationEntry_t *pEntry = nullptr;
		eResult = m_pRegistration->getEntry( handle, &pEntry );
		if ( eResult != SIR_REGISTRATION_OK ) {
			return eResult;
		}
		if ( ( pEntry->nFlags & SIR_CVAR_FLAG_SENSITIVE ) != 0 && ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_SENSITIVE ) == 0 ) {
			return SIR_REGISTRATION_DENIED;
		}
		if ( ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_READ ) == 0 ) {
			return SIR_REGISTRATION_DENIED;
		}
		return outputText( submission.nRequestId, SIR_REGISTRATION_OK, pEntry->eKind == SIR_REGISTRATION_COMMAND ? "command" : "cvar" );
	}
	if ( sirConsoleCaseEqual( pszName, "exec" ) ) {
		if ( args.nCount != 2 ) {
			return SIR_REGISTRATION_INVALID_ARGUMENT;
		}
		if ( m_Storage.pfnExec == nullptr ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		return m_Storage.pfnExec( m_Storage.pExecContext, args.szArgs[1].pszText, args.szArgs[1].nBytes, submission );
	}
	return SIR_REGISTRATION_NOT_FOUND;
}

sirRegistrationResult_t SIRConsoleService::execute( const sirConsoleSubmission_t& submission, sirConsoleArguments_t& args ) noexcept
{
	if ( !sirValidateConsoleName( args.szArgs[0].pszText, args.szArgs[0].nBytes ) ) {
		return SIR_REGISTRATION_INVALID_NAME;
	}
	if ( ( submission.origin.nCapabilities & SIR_CONSOLE_CAP_EXECUTE ) == 0 ) {
		return SIR_REGISTRATION_DENIED;
	}
	const sirRegistrationResult_t eBuiltin = executeBuiltin( submission, args );
	if ( eBuiltin != SIR_REGISTRATION_NOT_FOUND ) {
		return eBuiltin;
	}
	sirRegistrationHandle_t handle = {};
	sirRegistrationResult_t eResult = m_pRegistration->find( args.szArgs[0].pszText, args.szArgs[0].nBytes, &handle );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	const sirRegistrationEntry_t *pEntry = nullptr;
	eResult = m_pRegistration->getEntry( handle, &pEntry );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	if ( pEntry->eKind != SIR_REGISTRATION_COMMAND || pEntry->pfnCallback == nullptr ) {
		return SIR_REGISTRATION_TYPE_MISMATCH;
	}
	if ( ( submission.origin.nCapabilities & pEntry->nCapabilities ) != pEntry->nCapabilities ) {
		return SIR_REGISTRATION_DENIED;
	}
	bool bAcquired = false;
	if ( pEntry->nOwnerId != 0 ) {
		if ( m_Storage.pfnAcquireModule == nullptr || !m_Storage.pfnAcquireModule( m_Storage.pModuleContext, pEntry->nOwnerId ) ) {
			return SIR_REGISTRATION_DENIED;
		}
		bAcquired = true;
	}
	const sirConsoleInvocation_t invocation = { this, submission };
	pEntry->pfnCallback( pEntry->pContext, &invocation, &args );
	if ( bAcquired ) {
		m_Storage.pfnReleaseModule( m_Storage.pModuleContext, pEntry->nOwnerId );
	}
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRConsoleService::pump( uint32_t nBudget, sirConsolePumpReport_t *pReport ) noexcept
{
	if ( pReport == nullptr ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pReport = {};
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	m_bPumping = true;
	const uint32_t nLimit = nBudget == 0 || nBudget > m_Storage.nMaxCommandsPerPump ? m_Storage.nMaxCommandsPerPump : nBudget;
	const uint64_t nFence = m_nWriteTicket.load( std::memory_order_acquire );
	while ( m_nReadTicket < nFence && pReport->nProcessed < nLimit ) {
		const uint64_t nTicket = m_nReadTicket;
		const uint32_t iSlot = ( uint32_t )( nTicket % m_Storage.nRequestCapacity );
		sirConsoleRequestSlot_t& slot = m_Storage.pRequests[iSlot];
		if ( slot.nSequence.load( std::memory_order_acquire ) != nTicket + 1 ) {
			break;
		}
		char *pszText = m_Storage.pszRequestText + ( size_t )iSlot * m_Storage.nInputStride;
		addHistory( slot, pszText );
		m_nCurrentRequestId = slot.submission.nRequestId;
		m_nOutputsCurrent = 0;
		m_nOutputOverflowCurrent = 0;
		sirConsoleArguments_t args = {};
		sirRegistrationResult_t eResult = tokenize( pszText, slot.nBytes, &args );
		if ( eResult == SIR_REGISTRATION_OK ) {
			eResult = execute( slot.submission, args );
		}
		if ( eResult != SIR_REGISTRATION_OK ) {
			const sirConsoleOutput_t failure = { slot.submission.nRequestId, eResult, "command rejected", 16 };
			writeOutput( failure );
			++pReport->nRejected;
		} else {
			++pReport->nSucceeded;
		}
		pReport->nOutputs += m_nOutputsCurrent;
		pReport->nOutputOverflow += m_nOutputOverflowCurrent;
		++pReport->nProcessed;
		// Keep the input slab owned until the callback returns; a recursive submit
		// cannot overwrite it.
		slot.nSequence.store( nTicket + m_Storage.nRequestCapacity, std::memory_order_release );
		++m_nReadTicket;
	}
	m_bPumping = false;
	m_nCurrentRequestId = 0;
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRConsoleService::complete( const char *pszPrefix, uint32_t nBytes,
                                                     sirConsoleCompletionResult_t *pResult ) noexcept
{
	if ( pResult == nullptr || pResult->pszOutput == nullptr || ( pszPrefix == nullptr && nBytes != 0 ) ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	pResult->nWritten = 0;
	pResult->nCount = 0;
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPumping || nBytes > SIR_REGISTRATION_MAX_NAME_BYTES ) {
		return SIR_REGISTRATION_DENIED;
	}
	for ( uint32_t iByte = 0; iByte < nBytes; ++iByte ) {
		const char nCharacter = pszPrefix[iByte];
		const bool bLetter = ( nCharacter >= 'a' && nCharacter <= 'z' ) || ( nCharacter >= 'A' && nCharacter <= 'Z' );
		const bool bNumber = nCharacter >= '0' && nCharacter <= '9';
		if ( !bLetter && !bNumber && nCharacter != '.' && nCharacter != '_' ) {
			return SIR_REGISTRATION_INVALID_NAME;
		}
	}
	for ( uint32_t iEntry = 0; iEntry < m_pRegistration->getCapacity(); ++iEntry ) {
		const sirRegistrationEntry_t *pEntry = nullptr;
		sirRegistrationHandle_t handle = {};
		if ( m_pRegistration->getAtIndex( iEntry, &pEntry, &handle ) != SIR_REGISTRATION_OK ) {
			continue;
		}
		if ( ( pEntry->nFlags & SIR_CVAR_FLAG_HIDDEN ) != 0 && ( pResult->nCapabilities & SIR_CONSOLE_CAP_ADMIN ) == 0 ) {
			continue;
		}
		if ( ( pEntry->nFlags & SIR_CVAR_FLAG_SENSITIVE ) != 0 && ( pResult->nCapabilities & SIR_CONSOLE_CAP_SENSITIVE ) == 0 ) {
			continue;
		}
		if ( pEntry->eKind == SIR_REGISTRATION_COMMAND && ( pResult->nCapabilities & pEntry->nCapabilities ) != pEntry->nCapabilities ) {
			continue;
		}
		char szName[SIR_REGISTRATION_MAX_NAME_BYTES + 1] = {};
		uint32_t nWritten = 0;
		if ( m_pPool->decodeInto( pEntry->atom, szName, sizeof( szName ), nWritten ) != SIR_STRING_OK ) {
			return SIR_REGISTRATION_BAD_SCHEMA;
		}
		if ( !sirConsolePrefix( szName, pszPrefix, nBytes ) ) {
			continue;
		}
		if ( pResult->nCount == m_Storage.nCompletionCapacity ) {
			return SIR_REGISTRATION_CAPACITY_EXCEEDED;
		}
		uint32_t iInsert = pResult->nCount;
		while ( iInsert != 0 && strcmp( m_Storage.pCompletionScratch[iInsert - 1].szName, szName ) > 0 ) {
			m_Storage.pCompletionScratch[iInsert] = m_Storage.pCompletionScratch[iInsert - 1];
			--iInsert;
		}
		memcpy( m_Storage.pCompletionScratch[iInsert].szName, szName, nWritten + 1 );
		++pResult->nCount;
	}
	uint32_t nRequired = 1;
	for ( uint32_t iItem = 0; iItem < pResult->nCount; ++iItem ) {
		nRequired += ( uint32_t )strlen( m_Storage.pCompletionScratch[iItem].szName ) + 1;
	}
	if ( nRequired > pResult->nCapacity ) {
		pResult->nCount = 0;
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	for ( uint32_t iItem = 0; iItem < pResult->nCount; ++iItem ) {
		const char *pszName = m_Storage.pCompletionScratch[iItem].szName;
		const size_t nLength = strlen( pszName );
		memcpy( pResult->pszOutput + pResult->nWritten, pszName, nLength );
		pResult->nWritten += ( uint32_t )nLength;
		pResult->pszOutput[pResult->nWritten++] = '\n';
	}
	pResult->pszOutput[pResult->nWritten] = '\0';
	return SIR_REGISTRATION_OK;
}

sirRegistrationResult_t SIRConsoleService::registerDynamicCommand( const sirRegistrationDynamicDesc_t& descriptor,
                                                                   sirRegistrationHandle_t *pHandle ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPumping || descriptor.eKind != SIR_REGISTRATION_COMMAND ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( descriptor.name.nOwnerId != 0 && m_Storage.pfnAcquireModule == nullptr ) {
		return SIR_REGISTRATION_DENIED;
	}
	return m_pRegistration->registerDynamic( descriptor, pHandle );
}

sirRegistrationResult_t SIRConsoleService::unregisterDynamicCommand( sirRegistrationHandle_t handle ) noexcept
{
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	const sirRegistrationEntry_t *pEntry = nullptr;
	sirRegistrationResult_t eResult = m_pRegistration->getEntry( handle, &pEntry );
	if ( eResult != SIR_REGISTRATION_OK ) {
		return eResult;
	}
	return pEntry->eKind == SIR_REGISTRATION_COMMAND ? m_pRegistration->unregisterDynamic( handle ) : SIR_REGISTRATION_TYPE_MISMATCH;
}

sirRegistrationResult_t SIRConsoleService::unloadModule( uint32_t nOwnerId, uint32_t *pRemoved ) noexcept
{
	if ( pRemoved == nullptr || nOwnerId == 0 ) {
		return SIR_REGISTRATION_INVALID_ARGUMENT;
	}
	*pRemoved = 0;
	if ( !isInitialized() ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( !ownerThread() ) {
		return SIR_REGISTRATION_WRONG_THREAD;
	}
	if ( m_bPumping ) {
		return SIR_REGISTRATION_DENIED;
	}
	*pRemoved = m_pRegistration->unregisterModule( nOwnerId );
	return SIR_REGISTRATION_OK;
}

void SIRConsoleService::shutdown( void ) noexcept
{
	// All producers and module callbacks must be joined before shutdown.
	m_bAccepting.store( false, std::memory_order_release );
	m_pRegistration = nullptr;
	m_pCVars = nullptr;
	m_pPool = nullptr;
	m_Storage = {};
	m_nReadTicket = 0;
	m_nWriteTicket.store( 0, std::memory_order_relaxed );
	m_nHistoryCount = 0;
	m_nHistoryNext = 0;
	m_nOutputsCurrent = 0;
	m_nOutputOverflowCurrent = 0;
	m_nCurrentRequestId = 0;
	m_bPumping = false;
	m_OwnerThread = std::thread::id{};
}
