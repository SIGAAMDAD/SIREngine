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

#include <Scripting/Core/SIRScriptOptimizerWorker.h>

#include <stdlib.h>
#include <string.h>

SIRScriptOptimizerWorker::SIRScriptOptimizerWorker( void ) noexcept
    : m_pInputs( nullptr ), m_pCache( nullptr ), m_Backend{}, m_nMaximumOutputBytes( 0 ), m_bProcessing( false )
{
}

sirScriptResult_t SIRScriptOptimizerWorker::configure( SIRScriptOptimizerInputStore *pInputs, SIRScriptArtifactCache *pCache,
                                                       const sirScriptOptimizerBackendAPI_t& backend,
                                                       uint64_t nMaximumOutputBytes ) noexcept
{
	if ( m_pInputs != nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pInputs == nullptr || pCache == nullptr || backend.nABIRevision != SIR_SCRIPT_OPT_BACKEND_ABI_REVISION ||
	     backend.pCompile == nullptr || backend.pRelease == nullptr || nMaximumOutputBytes == 0 || nMaximumOutputBytes > UINT32_MAX ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	m_pInputs = pInputs;
	m_pCache = pCache;
	m_Backend = backend;
	m_nMaximumOutputBytes = nMaximumOutputBytes;
	return SIR_SCRIPT_OK;
}

sirScriptResult_t SIRScriptOptimizerWorker::process( const sirScriptOptimizationRequest_t& request,
                                                     sirScriptOptimizationReply_t *pReply ) noexcept
{
	if ( m_pInputs == nullptr || m_pCache == nullptr ) {
		return SIR_SCRIPT_WRONG_STATE;
	}
	if ( pReply == nullptr || m_bProcessing ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
	if ( SIRScriptOptimizationProtocol::encodeRequest( request, szWire ) != SIR_SCRIPT_OK ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	m_bProcessing = true;
	sirScriptOptimizationReply_t reply = {};
	reply.nJobId = request.nJobId;
	reply.eResult = SIR_SCRIPT_BACKEND_FAILURE;
	uint64_t nRecipeBytes = 0;
	sirScriptResult_t eResult = m_pInputs->inspect( request, &nRecipeBytes );
	if ( eResult != SIR_SCRIPT_OK ) {
		reply.eResult = eResult;
		*pReply = reply;
		m_bProcessing = false;
		return SIR_SCRIPT_OK;
	}
	if ( nRecipeBytes > SIZE_MAX || request.nInputBytes > SIZE_MAX ) {
		reply.eResult = SIR_SCRIPT_CAPACITY_EXCEEDED;
		*pReply = reply;
		m_bProcessing = false;
		return SIR_SCRIPT_OK;
	}
	void *pRecipe = malloc( ( size_t )nRecipeBytes );
	void *pInput = malloc( ( size_t )request.nInputBytes );
	if ( pRecipe == nullptr || pInput == nullptr ) {
		free( pRecipe );
		free( pInput );
		reply.eResult = SIR_SCRIPT_CAPACITY_EXCEEDED;
		*pReply = reply;
		m_bProcessing = false;
		return SIR_SCRIPT_OK;
	}
	sirScriptOptimizerInputBuffers_t buffers = { pRecipe, nRecipeBytes, pInput, request.nInputBytes, 0 };
	eResult = m_pInputs->fetch( request, &buffers );
	if ( eResult == SIR_SCRIPT_OK ) {
		const sirScriptOptimizerWorkView_t view = { &request, pRecipe, buffers.nRecipeBytes, pInput, request.nInputBytes };
		sirScriptOptimizerOutput_t output = {};
		eResult = m_Backend.pCompile( m_Backend.pUser, &view, &output );
		if ( eResult == SIR_SCRIPT_OK ) {
			if ( output.pBytes == nullptr || output.nBytes == 0 || output.nBytes > m_nMaximumOutputBytes || output.nAlignment == 0 ||
			     output.nAlignment > 4096 || ( output.nAlignment & ( output.nAlignment - 1 ) ) != 0 ||
			     ( ( uintptr_t )output.pBytes & ( output.nAlignment - 1 ) ) != 0 ) {
				eResult = SIR_SCRIPT_CAPACITY_EXCEEDED;
			} else {
				eResult = m_pCache->store( request.artifact, request.key, output.pBytes, output.nBytes, output.nAlignment, &reply.cached );
				if ( eResult == SIR_SCRIPT_DUPLICATE ) {
					eResult = SIR_SCRIPT_OK;
				}
			}
		}
		// The backend must be able to reclaim partial output after failure.
		m_Backend.pRelease( m_Backend.pUser, &output );
	}
	free( pRecipe );
	free( pInput );
	reply.eResult = eResult;
	if ( eResult != SIR_SCRIPT_OK ) {
		memset( &reply.cached, 0, sizeof( reply.cached ) );
	}
	*pReply = reply;
	m_bProcessing = false;
	return SIR_SCRIPT_OK;
}
