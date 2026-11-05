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

#include <Core/Console/SIRConsoleConfigAdapter.h>
#include <string.h>

SIRConsoleConfigAdapter::SIRConsoleConfigAdapter( void ) noexcept
    : m_pAdapter( nullptr ), m_Storage{}, m_LastDiagnostic{}, m_bExecuting( false )
{
}

SIRConsoleConfigAdapter::~SIRConsoleConfigAdapter( void ) noexcept
{
	shutdown();
}

const sirConfigDiagnostic_t& SIRConsoleConfigAdapter::getLastDiagnostic( void ) const noexcept
{
	return m_LastDiagnostic;
}

sirConfigResult_t SIRConsoleConfigAdapter::initialize( SIRCVarConfigAdapter& adapter, const sirConsoleConfigStorage_t& storage ) noexcept
{
	if ( m_pAdapter != nullptr || !adapter.isInitialized() || storage.pFileBytes == nullptr || storage.nFileCapacity == 0 ||
	     storage.pfnRead == nullptr ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	m_pAdapter = &adapter;
	m_Storage = storage;
	m_LastDiagnostic = {};
	m_bExecuting = false;
	return SIR_CONFIG_OK;
}

sirRegistrationResult_t SIRConsoleConfigAdapter::execute( void *pContext, const char *pszFileName, uint32_t nBytes,
                                                          const sirConsoleSubmission_t& submission ) noexcept
{
	SIRConsoleConfigAdapter *pAdapter = ( SIRConsoleConfigAdapter * )pContext;
	if ( pAdapter == nullptr || pAdapter->m_pAdapter == nullptr ) {
		return SIR_REGISTRATION_NOT_INITIALIZED;
	}
	if ( pAdapter->m_bExecuting ) {
		return SIR_REGISTRATION_DENIED;
	}
	if ( pszFileName == nullptr || nBytes < 5 || nBytes > 128 ||
	     ( submission.origin.nCapabilities & ( SIR_CONSOLE_CAP_EXECUTE | SIR_CONSOLE_CAP_WRITE ) ) !=
	         ( SIR_CONSOLE_CAP_EXECUTE | SIR_CONSOLE_CAP_WRITE ) ) {
		return SIR_REGISTRATION_DENIED;
	}
	for ( uint32_t iByte = 0; iByte < nBytes; ++iByte ) {
		const char nByte = pszFileName[iByte];
		const bool bLetter = ( nByte >= 'a' && nByte <= 'z' ) || ( nByte >= 'A' && nByte <= 'Z' );
		const bool bNumber = nByte >= '0' && nByte <= '9';
		if ( !bLetter && !bNumber && nByte != '_' && nByte != '-' && nByte != '.' && nByte != '/' ) {
			return SIR_REGISTRATION_INVALID_NAME;
		}
		if ( nByte == '.' && iByte + 1 < nBytes && pszFileName[iByte + 1] == '.' ) {
			return SIR_REGISTRATION_INVALID_NAME;
		}
	}
	if ( pszFileName[0] == '/' || memcmp( pszFileName + nBytes - 4, ".ini", 4 ) != 0 ) {
		return SIR_REGISTRATION_INVALID_NAME;
	}
	pAdapter->m_bExecuting = true;
	pAdapter->m_LastDiagnostic = {};
	sirConsoleConfigReadRequest_t request = { pszFileName, nBytes, pAdapter->m_Storage.pFileBytes, pAdapter->m_Storage.nFileCapacity, 0 };
	const sirConfigResult_t eRead = pAdapter->m_Storage.pfnRead( pAdapter->m_Storage.pContext, &request );
	sirConfigResult_t eResult = eRead;
	if ( eRead == SIR_CONFIG_OK ) {
		if ( request.pszName != pszFileName || request.nNameBytes != nBytes || request.pBytes != pAdapter->m_Storage.pFileBytes ||
		     request.nCapacity != pAdapter->m_Storage.nFileCapacity ) {
			eResult = SIR_CONFIG_INVALID_ARGUMENT;
		} else if ( request.nRead > request.nCapacity ) {
			eResult = SIR_CONFIG_LIMIT_EXCEEDED;
		} else {
			const sirConfigInput_t input = { request.pBytes, request.nRead, {} };
			eResult = pAdapter->m_pAdapter->applyIni( input, submission.origin, &pAdapter->m_LastDiagnostic );
		}
	}
	pAdapter->m_bExecuting = false;
	if ( eResult == SIR_CONFIG_OK ) {
		return SIR_REGISTRATION_OK;
	}
	if ( eResult == SIR_CONFIG_NOT_FOUND ) {
		return SIR_REGISTRATION_NOT_FOUND;
	}
	if ( eResult == SIR_CONFIG_TYPE_MISMATCH ) {
		return SIR_REGISTRATION_TYPE_MISMATCH;
	}
	if ( eResult == SIR_CONFIG_LIMIT_EXCEEDED ) {
		return SIR_REGISTRATION_CAPACITY_EXCEEDED;
	}
	return SIR_REGISTRATION_BAD_SCHEMA;
}

void SIRConsoleConfigAdapter::shutdown( void ) noexcept
{
	m_pAdapter = nullptr;
	m_Storage = {};
	m_LastDiagnostic = {};
	m_bExecuting = false;
}
