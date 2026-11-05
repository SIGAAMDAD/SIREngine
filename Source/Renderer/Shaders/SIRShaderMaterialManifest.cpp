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

#include <Renderer/Shaders/SIRShaderMaterialManifest.h>

#include <string.h>

sirGraphicsResult_t sirShaderBuildMaterialManifest( const sirShaderMaterialManifestRequest_t& request, void *pBytes, uint32_t nCapacity,
                                                    uint32_t *pRequiredBytes ) noexcept
{
	if ( request.pPasses == nullptr || request.nAssetId == 0 || request.nSourceRevision == 0 || request.nPassCount == 0 ||
	     request.nPassCount > SIR_SHADER_MATERIAL_MAX_PASSES || request.nDependencyCount > SIR_SHADER_MATERIAL_MAX_DEPENDENCIES ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	// Pass count is bounded, so canonical ordering requires no allocation.
	sirShaderMaterialPassRecord_t szSorted[SIR_SHADER_MATERIAL_MAX_PASSES] = {};
	for ( uint32_t iPass = 0; iPass < request.nPassCount; ++iPass ) {
		const sirShaderMaterialPassRecord_t& pass = request.pPasses[iPass];
		uint32_t iInsert = iPass;
		while ( iInsert > 0 && szSorted[iInsert - 1].nPass > pass.nPass ) {
			szSorted[iInsert] = szSorted[iInsert - 1];
			--iInsert;
		}
		szSorted[iInsert] = pass;
	}

	constexpr uint32_t MAX_MATERIAL_BYTES =
	    SIR_SHADER_MATERIAL_HEADER_BYTES + SIR_SHADER_MATERIAL_MAX_PASSES * SIR_SHADER_MATERIAL_PASS_BYTES;
	uint8_t szEncoded[MAX_MATERIAL_BYTES] = {};
	const uint32_t nPassBytes = request.nPassCount * SIR_SHADER_MATERIAL_PASS_BYTES;
	const uint32_t nTotalBytes = SIR_SHADER_MATERIAL_HEADER_BYTES + nPassBytes;
	for ( uint32_t iPass = 0; iPass < request.nPassCount; ++iPass ) {
		const sirShaderMaterialPassRecord_t& pass = szSorted[iPass];
		if ( ( iPass > 0 && pass.nPass == szSorted[iPass - 1].nPass ) ||
		     ( pass.iShaderDependency != SIR_SHADER_MATERIAL_NO_SHADER && pass.iShaderDependency >= request.nDependencyCount ) ||
		     sirShaderEncodeMaterialPass( pass, szEncoded + SIR_SHADER_MATERIAL_HEADER_BYTES + iPass * SIR_SHADER_MATERIAL_PASS_BYTES,
		                                  SIR_SHADER_MATERIAL_PASS_BYTES ) != SIR_GRAPHICS_SUCCESS ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	sirShaderMaterialHeader_t header = {};
	header.nMagic = SIR_SHADER_MATERIAL_MAGIC;
	header.nSchema = SIR_SHADER_MATERIAL_SCHEMA;
	header.nHeaderBytes = SIR_SHADER_MATERIAL_HEADER_BYTES;
	header.nTotalBytes = nTotalBytes;
	header.nDependencyCount = request.nDependencyCount;
	header.nPassCount = request.nPassCount;
	header.nAssetId = request.nAssetId;
	header.nSourceRevision = request.nSourceRevision;
	if ( sirShaderBuildMaterialHash( header, szEncoded + SIR_SHADER_MATERIAL_HEADER_BYTES, nPassBytes, &header.nManifestHash ) !=
	         SIR_GRAPHICS_SUCCESS ||
	     sirShaderEncodeMaterialHeader( header, szEncoded, SIR_SHADER_MATERIAL_HEADER_BYTES ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderValidateMaterial( szEncoded, nTotalBytes ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pRequiredBytes != nullptr ) {
		*pRequiredBytes = nTotalBytes;
	}
	if ( pBytes == nullptr || nCapacity < nTotalBytes ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	memcpy( pBytes, szEncoded, nTotalBytes );
	return SIR_GRAPHICS_SUCCESS;
}
