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

#include <Scripting/Core/SIRScriptOptimizationProtocol.h>
#include <Scripting/Core/SIRScriptSHA256.h>

#include <string.h>

static constexpr uint8_t s_szOptimizationMagic[8] = { 'S', 'I', 'R', 'O', 'P', 'T', '0', '1' };
static constexpr uint32_t s_nOptimizationWireRevision = 1;
static constexpr uint32_t s_nOptimizationRequestKind = 1;
static constexpr uint32_t s_nOptimizationReplyKind = 2;

static void scriptProtocolWrite32( uint8_t *pBytes, uint32_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static void scriptProtocolWrite64( uint8_t *pBytes, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		pBytes[iByte] = ( uint8_t )( nValue >> ( iByte * 8 ) );
	}
}

static uint32_t scriptProtocolRead32( const uint8_t *pBytes ) noexcept
{
	uint32_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 4; ++iByte ) {
		nValue |= ( uint32_t )pBytes[iByte] << ( iByte * 8 );
	}
	return nValue;
}

static uint64_t scriptProtocolRead64( const uint8_t *pBytes ) noexcept
{
	uint64_t nValue = 0;
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nValue |= ( uint64_t )pBytes[iByte] << ( iByte * 8 );
	}
	return nValue;
}

static bool scriptProtocolNonzeroDigest( const uint8_t szDigest[32] ) noexcept
{
	uint8_t nAggregate = 0;
	for ( uint32_t iByte = 0; iByte < 32; ++iByte ) {
		nAggregate |= szDigest[iByte];
	}
	return nAggregate != 0;
}

static void scriptProtocolEncodeArtifact( uint8_t *pBytes, const sirScriptCodeArtifact_t &artifact ) noexcept
{
	const uint64_t szFields[7] = { artifact.nBackendId,         artifact.nProgramId, artifact.nRevision,        artifact.nFormatId,
	                               artifact.nSchemaFingerprint, artifact.nTargetId,  artifact.nBuildFingerprint };
	for ( uint32_t iField = 0; iField < 7; ++iField ) {
		scriptProtocolWrite64( pBytes + iField * 8, szFields[iField] );
	}
}

static void scriptProtocolDecodeArtifact( const uint8_t *pBytes, sirScriptCodeArtifact_t *pArtifact ) noexcept
{
	uint64_t *pszFields[7] = { &pArtifact->nBackendId,         &pArtifact->nProgramId, &pArtifact->nRevision,        &pArtifact->nFormatId,
	                           &pArtifact->nSchemaFingerprint, &pArtifact->nTargetId,  &pArtifact->nBuildFingerprint };
	for ( uint32_t iField = 0; iField < 7; ++iField ) {
		*pszFields[iField] = scriptProtocolRead64( pBytes + iField * 8 );
	}
}

static bool scriptProtocolSameArtifact( const sirScriptCodeArtifact_t &a, const sirScriptCodeArtifact_t &b ) noexcept
{
	return a.nBackendId == b.nBackendId && a.nProgramId == b.nProgramId && a.nRevision == b.nRevision && a.nFormatId == b.nFormatId &&
	       a.nSchemaFingerprint == b.nSchemaFingerprint && a.nTargetId == b.nTargetId && a.nBuildFingerprint == b.nBuildFingerprint;
}

static bool scriptProtocolValidArtifact( const sirScriptCodeArtifact_t &a ) noexcept
{
	return a.nBackendId != 0 && a.nProgramId != 0 && a.nRevision != 0 && a.nFormatId != 0 && a.nSchemaFingerprint != 0 &&
	       a.nTargetId != 0 && a.nBuildFingerprint != 0;
}

static void scriptProtocolHeader( uint8_t *pOutput, uint32_t nKind ) noexcept
{
	memcpy( pOutput, s_szOptimizationMagic, 8 );
	scriptProtocolWrite32( pOutput + 8, s_nOptimizationWireRevision );
	scriptProtocolWrite32( pOutput + 12, nKind );
}

static bool scriptProtocolVerify( const uint8_t *pBytes, uint32_t nLength, uint32_t nKind ) noexcept
{
	if ( memcmp( pBytes, s_szOptimizationMagic, 8 ) != 0 || scriptProtocolRead32( pBytes + 8 ) != s_nOptimizationWireRevision ||
	     scriptProtocolRead32( pBytes + 12 ) != nKind ) {
		return false;
	}
	uint8_t szDigest[32] = {};
	SIRScriptSHA256::digest( pBytes, nLength - 32, szDigest );
	return memcmp( szDigest, pBytes + nLength - 32, 32 ) == 0;
}

bool SIRScriptOptimizationProtocol::validRequest( const sirScriptOptimizationRequest_t &request ) noexcept
{
	return request.nJobId != 0 && scriptProtocolValidArtifact( request.artifact ) && scriptProtocolNonzeroDigest( request.key.szDigest ) &&
	       request.nInputBytes != 0 && request.eTier <= SIR_SCRIPT_OPT_PROFILE_GUIDED && request.nFlags == 0;
}

bool SIRScriptOptimizationProtocol::validReply( const sirScriptOptimizationReply_t &reply ) noexcept
{
	if ( reply.nJobId == 0 || reply.eResult > SIR_SCRIPT_BACKEND_FAILURE ) {
		return false;
	}
	if ( reply.eResult != SIR_SCRIPT_OK ) {
		return true;
	}
	const sirScriptCachedArtifact_t &entry = reply.cached;
	return scriptProtocolValidArtifact( entry.artifact ) && scriptProtocolNonzeroDigest( entry.key.szDigest ) &&
	       scriptProtocolNonzeroDigest( entry.szPayloadDigest ) && entry.nBytes != 0 && entry.nAlignment != 0 && entry.nAlignment <= 4096 &&
	       ( entry.nAlignment & ( entry.nAlignment - 1 ) ) == 0;
}

sirScriptResult_t SIRScriptOptimizationProtocol::encodeRequest( const sirScriptOptimizationRequest_t &request,
                                                                uint8_t szOutput[REQUEST_BYTES] ) noexcept
{
	if ( szOutput == nullptr || !validRequest( request ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	memset( szOutput, 0, REQUEST_BYTES );
	scriptProtocolHeader( szOutput, s_nOptimizationRequestKind );
	scriptProtocolWrite64( szOutput + 16, request.nJobId );
	scriptProtocolEncodeArtifact( szOutput + 24, request.artifact );
	memcpy( szOutput + 80, request.key.szDigest, 32 );
	scriptProtocolWrite64( szOutput + 112, request.nInputBytes );
	scriptProtocolWrite32( szOutput + 120, ( uint32_t )request.eTier );
	scriptProtocolWrite32( szOutput + 124, request.nFlags );
	SIRScriptSHA256::digest( szOutput, REQUEST_BYTES - 32, szOutput + REQUEST_BYTES - 32 );
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationProtocol::decodeRequest( const uint8_t *pInput, uint32_t nBytes,
                                                                sirScriptOptimizationRequest_t *pRequest ) noexcept
{
	if ( pInput == nullptr || pRequest == nullptr || nBytes != REQUEST_BYTES ||
	     !scriptProtocolVerify( pInput, REQUEST_BYTES, s_nOptimizationRequestKind ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	sirScriptOptimizationRequest_t decoded = {};
	decoded.nJobId = scriptProtocolRead64( pInput + 16 );
	scriptProtocolDecodeArtifact( pInput + 24, &decoded.artifact );
	memcpy( decoded.key.szDigest, pInput + 80, 32 );
	decoded.nInputBytes = scriptProtocolRead64( pInput + 112 );
	decoded.eTier = ( sirScriptOptimizationTier_t )scriptProtocolRead32( pInput + 120 );
	decoded.nFlags = scriptProtocolRead32( pInput + 124 );
	if ( !validRequest( decoded ) ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	*pRequest = decoded;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationProtocol::encodeReply( const sirScriptOptimizationReply_t &reply,
                                                              uint8_t szOutput[REPLY_BYTES] ) noexcept
{
	if ( szOutput == nullptr || !validReply( reply ) ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	memset( szOutput, 0, REPLY_BYTES );
	scriptProtocolHeader( szOutput, s_nOptimizationReplyKind );
	scriptProtocolWrite64( szOutput + 16, reply.nJobId );
	scriptProtocolWrite32( szOutput + 24, reply.eResult );
	if ( reply.eResult == SIR_SCRIPT_OK ) {
		const sirScriptCachedArtifact_t &entry = reply.cached;
		scriptProtocolEncodeArtifact( szOutput + 32, entry.artifact );
		memcpy( szOutput + 88, entry.key.szDigest, 32 );
		memcpy( szOutput + 120, entry.szPayloadDigest, 32 );
		scriptProtocolWrite64( szOutput + 152, entry.nBytes );
		scriptProtocolWrite32( szOutput + 160, entry.nAlignment );
	}
	SIRScriptSHA256::digest( szOutput, REPLY_BYTES - 32, szOutput + REPLY_BYTES - 32 );
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationProtocol::decodeReply( const uint8_t *pInput, uint32_t nBytes,
                                                              sirScriptOptimizationReply_t *pReply ) noexcept
{
	if ( pInput == nullptr || pReply == nullptr || nBytes != REPLY_BYTES ||
	     !scriptProtocolVerify( pInput, REPLY_BYTES, s_nOptimizationReplyKind ) || scriptProtocolRead32( pInput + 28 ) != 0 ||
	     scriptProtocolRead32( pInput + 164 ) != 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	sirScriptOptimizationReply_t decoded = {};
	decoded.nJobId = scriptProtocolRead64( pInput + 16 );
	const uint32_t nStatus = scriptProtocolRead32( pInput + 24 );
	if ( nStatus > SIR_SCRIPT_BACKEND_FAILURE ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	decoded.eResult = ( sirScriptResult_t )nStatus;
	if ( decoded.eResult == SIR_SCRIPT_OK ) {
		sirScriptCachedArtifact_t &entry = decoded.cached;
		scriptProtocolDecodeArtifact( pInput + 32, &entry.artifact );
		memcpy( entry.key.szDigest, pInput + 88, 32 );
		memcpy( entry.szPayloadDigest, pInput + 120, 32 );
		entry.nBytes = scriptProtocolRead64( pInput + 152 );
		entry.nAlignment = scriptProtocolRead32( pInput + 160 );
	} else {
		for ( uint32_t iByte = 32; iByte < REPLY_BYTES - 32; ++iByte ) {
			if ( pInput[iByte] != 0 ) {
				return SIR_SCRIPT_TYPE_MISMATCH;
			}
		}
	}
	if ( !validReply( decoded ) ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	*pReply = decoded;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizationProtocol::validateCompletion( const sirScriptOptimizationRequest_t &request,
                                                                     const sirScriptOptimizationReply_t &reply ) noexcept
{
	if ( !validRequest( request ) || !validReply( reply ) || request.nJobId != reply.nJobId ) {
		return SIR_SCRIPT_TYPE_MISMATCH;
	}
	if ( reply.eResult != SIR_SCRIPT_OK ) {
		return reply.eResult;
	}
	return scriptProtocolSameArtifact( request.artifact, reply.cached.artifact ) &&
	               memcmp( request.key.szDigest, reply.cached.key.szDigest, 32 ) == 0
	           ? SIR_SCRIPT_OK
			   : SIR_SCRIPT_TYPE_MISMATCH;
}
