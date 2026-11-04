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

#include <Scripting/Core/SIRScriptBindingRegistry.h>

#include <stdint.h>

SIRScriptBindingRegistry::SIRScriptBindingRegistry( void ) noexcept
    : m_nTypes( 0 ), m_nFunctions( 0 ), m_nEvents( 0 ), m_nSchemaFingerprint( 0 ), m_bPublished( false )
{
}

bool SIRScriptBindingRegistry::isValidAlignment( uint32_t nAlignment ) noexcept
{
	return nAlignment != 0 && ( nAlignment & ( nAlignment - 1 ) ) == 0;
}

bool SIRScriptBindingRegistry::checkReadFrame( sirScriptReadFrame_t frame, uint32_t nBytes, uint32_t nAlignment ) noexcept
{
	if ( frame.nBytes != nBytes || frame.nAlignment != nAlignment ) {
		return false;
	}
	return nBytes == 0 ? frame.pData == nullptr : frame.pData != nullptr && ( ( uintptr_t )frame.pData % nAlignment ) == 0;
}

bool SIRScriptBindingRegistry::checkWriteFrame( sirScriptWriteFrame_t frame, uint32_t nBytes, uint32_t nAlignment ) noexcept
{
	if ( frame.nBytes != nBytes || frame.nAlignment != nAlignment ) {
		return false;
	}
	return nBytes == 0 ? frame.pData == nullptr : frame.pData != nullptr && ( ( uintptr_t )frame.pData % nAlignment ) == 0;
}

const sirScriptTypeDescriptor_t *SIRScriptBindingRegistry::findType( uint64_t nTypeId ) const noexcept
{
	for ( uint32_t iType = 0; iType < m_nTypes; ++iType ) {
		if ( m_szTypes[iType].nTypeId == nTypeId ) {
			return &m_szTypes[iType];
		}
	}
	return nullptr;
}

sirScriptResult_t SIRScriptBindingRegistry::registerType( const sirScriptTypeDescriptor_t &descriptor ) noexcept
{
	if ( m_bPublished ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( descriptor.nTypeId == 0 || !isValidAlignment( descriptor.nAlignment ) || descriptor.nBytes == 0 ||
	     descriptor.nBytes % descriptor.nAlignment != 0 ||
	     ( descriptor.nFlags & ~( SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS ) ) != 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( findType( descriptor.nTypeId ) != nullptr ) {
		return SIR_SCRIPT_DUPLICATE;
	}
	if ( m_nTypes == MAX_TYPES ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	m_szTypes[m_nTypes++] = descriptor;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptBindingRegistry::registerFunction( const sirScriptFunctionDescriptor_t &descriptor,
                                                              sirScriptNativeThunk_t pInvoke, void *pUser ) noexcept
{
	if ( m_bPublished ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( descriptor.nFunctionId == 0 || descriptor.nSignatureId == 0 || descriptor.nBindingRevision == 0 || pInvoke == nullptr ||
	     ( descriptor.nEffects & ~UINT32_C( 127 ) ) != 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( descriptor.nArgumentsTypeId != 0 && findType( descriptor.nArgumentsTypeId ) == nullptr ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	if ( descriptor.nReturnTypeId != 0 && findType( descriptor.nReturnTypeId ) == nullptr ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	for ( uint32_t iFunction = 0; iFunction < m_nFunctions; ++iFunction ) {
		if ( m_szFunctions[iFunction].descriptor.nFunctionId == descriptor.nFunctionId ) {
			return SIR_SCRIPT_DUPLICATE;
		}
	}
	if ( m_nFunctions == MAX_FUNCTIONS ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	m_szFunctions[m_nFunctions++] = { descriptor, pInvoke, pUser };
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptBindingRegistry::registerEvent( const sirScriptEventDescriptor_t &descriptor ) noexcept
{
	if ( m_bPublished ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( descriptor.nEventTypeId == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	if ( descriptor.nPayloadTypeId != 0 && findType( descriptor.nPayloadTypeId ) == nullptr ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEvents; ++iEvent ) {
		if ( m_szEvents[iEvent].nEventTypeId == descriptor.nEventTypeId ) {
			return SIR_SCRIPT_DUPLICATE;
		}
	}
	if ( m_nEvents == MAX_EVENTS ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	m_szEvents[m_nEvents++] = descriptor;
	return SIR_SCRIPT_OK;
}

// Hash the named ABI fields in canonical little-endian byte order. Do not
// include struct padding, callback addresses or registration order.
static uint64_t hashSchemaWord( uint64_t nHash, uint64_t nValue ) noexcept
{
	constexpr uint64_t FNV_PRIME = 1099511628211ULL;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nHash ^= ( nValue >> ( iByte * 8 ) ) & 0xffU;
		nHash *= FNV_PRIME;
	}
	return nHash;
}

sirScriptResult_t SIRScriptBindingRegistry::publish( void ) noexcept
{
	if ( m_bPublished ) {
		return SIR_SCRIPT_WRONG_STATE;
	}

	// Canonical ordering makes equivalent schemas hash identically even if
	// independently registered by backends in a different order.
	for ( uint32_t iType = 0; iType < m_nTypes; ++iType ) {
		uint32_t iSmallest = iType;
		for ( uint32_t iNext = iType + 1; iNext < m_nTypes; ++iNext ) {
			if ( m_szTypes[iNext].nTypeId < m_szTypes[iSmallest].nTypeId ) {
				iSmallest = iNext;
			}
		}
		const sirScriptTypeDescriptor_t temporary = m_szTypes[iType];
		m_szTypes[iType] = m_szTypes[iSmallest];
		m_szTypes[iSmallest] = temporary;
	}
	for ( uint32_t iFunction = 0; iFunction < m_nFunctions; ++iFunction ) {
		uint32_t iSmallest = iFunction;
		for ( uint32_t iNext = iFunction + 1; iNext < m_nFunctions; ++iNext ) {
			if ( m_szFunctions[iNext].descriptor.nFunctionId < m_szFunctions[iSmallest].descriptor.nFunctionId ) {
				iSmallest = iNext;
			}
		}
		const sirScriptFunctionEntry_t temporary = m_szFunctions[iFunction];
		m_szFunctions[iFunction] = m_szFunctions[iSmallest];
		m_szFunctions[iSmallest] = temporary;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEvents; ++iEvent ) {
		uint32_t iSmallest = iEvent;
		for ( uint32_t iNext = iEvent + 1; iNext < m_nEvents; ++iNext ) {
			if ( m_szEvents[iNext].nEventTypeId < m_szEvents[iSmallest].nEventTypeId ) {
				iSmallest = iNext;
			}
		}
		const sirScriptEventDescriptor_t temporary = m_szEvents[iEvent];
		m_szEvents[iEvent] = m_szEvents[iSmallest];
		m_szEvents[iSmallest] = temporary;
	}

	constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
	uint64_t nHash = FNV_OFFSET_BASIS;
	nHash = hashSchemaWord( nHash, SIR_SCRIPT_BACKEND_ABI_REVISION );
	nHash = hashSchemaWord( nHash, m_nTypes );
	nHash = hashSchemaWord( nHash, m_nFunctions );
	nHash = hashSchemaWord( nHash, m_nEvents );
	for ( uint32_t iType = 0; iType < m_nTypes; ++iType ) {
		const sirScriptTypeDescriptor_t &descriptor = m_szTypes[iType];
		nHash = hashSchemaWord( nHash, descriptor.nTypeId );
		nHash = hashSchemaWord( nHash, descriptor.nBytes );
		nHash = hashSchemaWord( nHash, descriptor.nAlignment );
		nHash = hashSchemaWord( nHash, descriptor.nFlags );
	}
	for ( uint32_t iFunction = 0; iFunction < m_nFunctions; ++iFunction ) {
		const sirScriptFunctionDescriptor_t &descriptor = m_szFunctions[iFunction].descriptor;
		nHash = hashSchemaWord( nHash, descriptor.nFunctionId );
		nHash = hashSchemaWord( nHash, descriptor.nSignatureId );
		nHash = hashSchemaWord( nHash, descriptor.nArgumentsTypeId );
		nHash = hashSchemaWord( nHash, descriptor.nReturnTypeId );
		nHash = hashSchemaWord( nHash, descriptor.nOwnerLane );
		nHash = hashSchemaWord( nHash, descriptor.nBindingRevision );
		nHash = hashSchemaWord( nHash, descriptor.nEffects );
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEvents; ++iEvent ) {
		const sirScriptEventDescriptor_t &descriptor = m_szEvents[iEvent];
		nHash = hashSchemaWord( nHash, descriptor.nEventTypeId );
		nHash = hashSchemaWord( nHash, descriptor.nPayloadTypeId );
	}
	m_nSchemaFingerprint = nHash == 0 ? 1 : nHash;
	m_bPublished = true;
	return SIR_SCRIPT_OK;
}

bool SIRScriptBindingRegistry::describeFunction( uint64_t nFunctionId, sirScriptFunctionDescriptor_t *pDescriptor ) const noexcept
{
	if ( !m_bPublished || nFunctionId == 0 || pDescriptor == nullptr ) {
		return false;
	}
	for ( uint32_t iFunction = 0; iFunction < m_nFunctions; ++iFunction ) {
		const sirScriptFunctionDescriptor_t& descriptor = m_szFunctions[iFunction].descriptor;
		if ( descriptor.nFunctionId == nFunctionId ) {
			*pDescriptor = descriptor;
			return true;
		}
	}
	return false;
}

uint64_t SIRScriptBindingRegistry::schemaFingerprint( void ) const noexcept
{
	return m_bPublished ? m_nSchemaFingerprint : 0;
}

bool SIRScriptBindingRegistry::isPublished( void ) const noexcept
{
	return m_bPublished;
}

bool SIRScriptBindingRegistry::hasEvent( uint64_t nEventTypeId ) const noexcept
{
	if ( !m_bPublished ) {
		return false;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEvents; ++iEvent ) {
		if ( m_szEvents[iEvent].nEventTypeId == nEventTypeId ) {
			return true;
		}
	}
	return false;
}

sirScriptResult_t SIRScriptBindingRegistry::validateEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload ) const noexcept
{
	if ( !m_bPublished ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEvents; ++iEvent ) {
		const sirScriptEventDescriptor_t &descriptor = m_szEvents[iEvent];
		if ( descriptor.nEventTypeId != nEventTypeId ) {
			continue;
		}
		const sirScriptTypeDescriptor_t *pType = findType( descriptor.nPayloadTypeId );
		const uint32_t nBytes = pType != nullptr ? pType->nBytes : 0;
		const uint32_t nAlignment = pType != nullptr ? pType->nAlignment : 1;
		return checkReadFrame( payload, nBytes, nAlignment ) ? SIR_SCRIPT_OK : SIR_SCRIPT_TYPE_MISMATCH;
	}
	return SIR_SCRIPT_NOT_FOUND;
}

sirScriptResult_t SIRScriptBindingRegistry::validateQueuedFrame( uint64_t nTypeId, sirScriptReadFrame_t frame ) const noexcept
{
	if ( !m_bPublished ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	const sirScriptTypeDescriptor_t *pType = findType( nTypeId );
	if ( nTypeId != 0 && pType == nullptr ) {
		return SIR_SCRIPT_NOT_FOUND;
	}
	const uint32_t nBytes = pType != nullptr ? pType->nBytes : 0;
	const uint32_t nAlignment = pType != nullptr ? pType->nAlignment : 1;
	if ( !checkReadFrame( frame, nBytes, nAlignment ) ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	if ( pType != nullptr && ( pType->nFlags & ( SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS ) ) !=
	                             ( SIR_SCRIPT_TYPE_PLAIN_BYTES | SIR_SCRIPT_TYPE_NO_POINTERS ) ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptBindingRegistry::validateQueuedEvent( uint64_t nEventTypeId, sirScriptReadFrame_t payload ) const noexcept
{
	const sirScriptResult_t eValidation = validateEvent( nEventTypeId, payload );
	if ( eValidation != SIR_SCRIPT_OK ) {
		return eValidation;
	}
	for ( uint32_t iEvent = 0; iEvent < m_nEvents; ++iEvent ) {
		if ( m_szEvents[iEvent].nEventTypeId == nEventTypeId ) {
			return validateQueuedFrame( m_szEvents[iEvent].nPayloadTypeId, payload );
		}
	}
	return SIR_SCRIPT_NOT_FOUND;
}

sirScriptResult_t SIRScriptBindingRegistry::invokeNative( const sirScriptNativeInvocation_t& request ) const noexcept
{
	if ( !m_bPublished ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	for ( uint32_t iFunction = 0; iFunction < m_nFunctions; ++iFunction ) {
		const sirScriptFunctionEntry_t &entry = m_szFunctions[iFunction];
		if ( entry.descriptor.nFunctionId != request.nFunctionId ) {
			continue;
		}
		if ( entry.descriptor.nSignatureId != request.nSignatureId || entry.descriptor.nBindingRevision != request.nBindingRevision ) {
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
		const sirScriptTypeDescriptor_t *pArguments = findType( entry.descriptor.nArgumentsTypeId );
		const sirScriptTypeDescriptor_t *pReturn = findType( entry.descriptor.nReturnTypeId );
		const uint32_t nArgumentBytes = pArguments != nullptr ? pArguments->nBytes : 0;
		const uint32_t nArgumentAlignment = pArguments != nullptr ? pArguments->nAlignment : 1;
		const uint32_t nReturnBytes = pReturn != nullptr ? pReturn->nBytes : 0;
		const uint32_t nReturnAlignment = pReturn != nullptr ? pReturn->nAlignment : 1;
		if ( !checkReadFrame( request.arguments, nArgumentBytes, nArgumentAlignment ) ) {
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
		if ( !checkWriteFrame( request.result, nReturnBytes, nReturnAlignment ) ) {
			return SIR_SCRIPT_TYPE_MISMATCH;
		}
		return entry.pInvoke( entry.pUser, request.arguments, request.result );
	}
	return SIR_SCRIPT_NOT_FOUND;
}
