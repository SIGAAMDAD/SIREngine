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

#include <Renderer/Shaders/SIRShaderGRDG.h>

#include <string.h>

// Identity hashing is explicitly byte-wise and deterministic across hosts.
// GRDG independently compares complete source bytes before deduplication.
static uint64_t sirShaderGRDGHashScalar( uint64_t nHash, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		nHash ^= ( uint8_t )( nValue >> ( iByte * 8 ) );
		nHash *= 1099511628211ULL;
	}
	return nHash;
}

static bool sirShaderGRDGFormatsValid( const sirShaderGRDGFormats_t& formats ) noexcept
{
	return formats.nShaderTypeId != 0 && formats.nShaderFormatId != 0 && formats.nMaterialTypeId != 0 &&
	       formats.nMaterialFormatId != 0 && formats.nShaderTypeId != formats.nMaterialTypeId;
}

static SIRResourceResult sirShaderGRDGCheckView( const SIRResourceView& view, SIRResourceKind eKind, uint32_t nTypeId,
                                                 uint32_t nFormatId, SIRResourceInfo *pInfo ) noexcept
{
	if ( !view.isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	const SIRResourceResult eResult = view.getInfo( pInfo );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	if ( pInfo->m_Key.m_nTypeId != nTypeId || pInfo->m_eKind != eKind ) {
		return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}
	if ( pInfo->m_nFormatId != nFormatId || pInfo->m_nSchemaVersion != 1 || pInfo->m_nChunkCount != 1 ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult sirShaderProjectPackage( const SIRResourceView& view, const sirShaderGRDGFormats_t& formats,
                                            const sirShaderPackageValidation_t& validation, sirShaderPackageProjection_t *pOut ) noexcept
{
	if ( pOut == nullptr || !sirShaderGRDGFormatsValid( formats ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRResourceInfo info = {};
	SIRResourceResult eResult = sirShaderGRDGCheckView( view, SIR_RESOURCE_KIND_BLOB, formats.nShaderTypeId, formats.nShaderFormatId, &info );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	SIRResourceConstBytes bytes = {};
	eResult = view.getChunk( { 0 }, &bytes );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	sirShaderPackageHeader_t header = {};
	if ( sirShaderDecodePackageHeader( bytes.m_pData, bytes.m_nSize, &header ) != SIR_GRAPHICS_SUCCESS ||
	     header.nAssetId != info.m_Key.m_nAssetId ||
	     sirShaderValidatePackage( bytes.m_pData, bytes.m_nSize, validation.eFormat, validation.nAvailableFeatures ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	*pOut = { view, bytes, header, info.m_nRevision };
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult sirShaderProjectStage( const sirShaderPackageProjection_t& package, uint32_t iStage,
                                          sirShaderStageProjection_t *pOut ) noexcept
{
	if ( pOut == nullptr || !package.view.isValid() || package.bytes.m_pData == nullptr ||
	     iStage >= package.header.nStageCount ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const uint64_t nRecordOffset = ( uint64_t )package.header.nStageOffset + iStage * SIR_SHADER_PACKAGE_STAGE_BYTES;
	if ( nRecordOffset > package.bytes.m_nSize || SIR_SHADER_PACKAGE_STAGE_BYTES > package.bytes.m_nSize - nRecordOffset ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	sirShaderStageRecord_t stage = {};
	if ( sirShaderDecodeStage( package.bytes.m_pData + nRecordOffset, SIR_SHADER_PACKAGE_STAGE_BYTES, &stage ) !=
	     SIR_GRAPHICS_SUCCESS ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	if ( stage.nPayloadOffset > package.bytes.m_nSize ||
	     stage.nPayloadBytes > package.bytes.m_nSize - stage.nPayloadOffset ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	*pOut = { stage, { package.bytes.m_pData + stage.nPayloadOffset, stage.nPayloadBytes } };
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult sirShaderProjectMaterial( const SIRResourceView& view, const sirShaderGRDGFormats_t& formats,
                                             sirShaderMaterialProjection_t *pOut ) noexcept
{
	if ( pOut == nullptr || !sirShaderGRDGFormatsValid( formats ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRResourceInfo info = {};
	SIRResourceResult eResult =
	    sirShaderGRDGCheckView( view, SIR_RESOURCE_KIND_COMPOSITION, formats.nMaterialTypeId, formats.nMaterialFormatId, &info );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	SIRResourceConstBytes bytes = {};
	eResult = view.getChunk( { 0 }, &bytes );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	sirShaderMaterialHeader_t header = {};
	if ( sirShaderDecodeMaterialHeader( bytes.m_pData, bytes.m_nSize, &header ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderValidateMaterial( bytes.m_pData, bytes.m_nSize ) != SIR_GRAPHICS_SUCCESS ||
	     header.nAssetId != info.m_Key.m_nAssetId || header.nDependencyCount != info.m_nDependencyCount ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	for ( uint32_t iPass = 0; iPass < header.nPassCount; ++iPass ) {
		sirShaderMaterialPassRecord_t pass = {};
		const uint8_t *pRecord = bytes.m_pData + SIR_SHADER_MATERIAL_HEADER_BYTES + iPass * SIR_SHADER_MATERIAL_PASS_BYTES;
		if ( sirShaderDecodeMaterialPass( pRecord, SIR_SHADER_MATERIAL_PASS_BYTES, &pass ) != SIR_GRAPHICS_SUCCESS ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( pass.iShaderDependency != SIR_SHADER_MATERIAL_NO_SHADER ) {
			SIRResourceDependency dependency = {};
			if ( view.getDependency( pass.iShaderDependency, &dependency ) != SIR_RESOURCE_RESULT_SUCCESS ||
			     dependency.m_Target.m_nTypeId != formats.nShaderTypeId ) {
				return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
			}
		}
	}
	*pOut = { view, bytes, header, info.m_nRevision };
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult sirShaderProjectMaterialPass( const sirShaderMaterialProjection_t& material, uint32_t nPass,
                                                 sirShaderMaterialPassRecord_t *pOut ) noexcept
{
	if ( pOut == nullptr || !material.view.isValid() || material.bytes.m_pData == nullptr ||
	     nPass < SIR_SHADER_PASS_COLOR || nPass > SIR_SHADER_PASS_COMPUTE ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	for ( uint32_t iPass = 0; iPass < material.header.nPassCount; ++iPass ) {
		sirShaderMaterialPassRecord_t pass = {};
		const uint8_t *pRecord =
		    material.bytes.m_pData + SIR_SHADER_MATERIAL_HEADER_BYTES + iPass * SIR_SHADER_MATERIAL_PASS_BYTES;
		if ( sirShaderDecodeMaterialPass( pRecord, SIR_SHADER_MATERIAL_PASS_BYTES, &pass ) != SIR_GRAPHICS_SUCCESS ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( pass.nPass == nPass ) {
			*pOut = pass;
			return SIR_RESOURCE_RESULT_SUCCESS;
		}
	}
	return SIR_RESOURCE_RESULT_DEPENDENCY_MISSING;
}

SIRResourceResult sirShaderSnapshotMaterial( const SIRResourceGraph& graph, const SIRResourceReadScope& scope,
                                              const sirShaderMaterialProjection_t& material, sirShaderMaterialSnapshot_t *pOut ) noexcept
{
	if ( pOut == nullptr || !scope.isActive() || !material.view.isValid() || material.header.nDependencyCount >
	     SIR_SHADER_MATERIAL_MAX_DEPENDENCIES ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const uint32_t nCount = material.header.nDependencyCount;
	pOut->nRequiredCount = nCount;
	if ( nCount > pOut->nCapacity || ( nCount != 0 && pOut->pDependencies == nullptr ) ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	uint64_t nHash = sirShaderGRDGHashScalar( 14695981039346656037ULL, material.header.nManifestHash );
	nHash = sirShaderGRDGHashScalar( nHash, material.nGraphRevision );
	nHash = sirShaderGRDGHashScalar( nHash, material.header.nSourceRevision );
	nHash = sirShaderGRDGHashScalar( nHash, nCount );

	for ( uint32_t iEdge = 0; iEdge < nCount; ++iEdge ) {
		SIRResourceDependency edge = {};
		SIRResourceResult eResult = material.view.getDependency( iEdge, &edge );
		if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		SIRResourceView child;
		eResult = graph.resolveDependency( scope, material.view, iEdge, &child );
		const bool bResident = eResult == SIR_RESOURCE_RESULT_SUCCESS;
		if ( !bResident && edge.m_eKind == SIR_RESOURCE_EDGE_KIND_REQUIRED ) {
			return SIR_RESOURCE_RESULT_DEPENDENCY_MISSING;
		}
		if ( !bResident && eResult != SIR_RESOURCE_RESULT_DEPENDENCY_MISSING &&
		     eResult != SIR_RESOURCE_RESULT_NOT_RESIDENT && eResult != SIR_RESOURCE_RESULT_INVALID_HANDLE ) {
			return eResult;
		}
		sirShaderDependencyRevision_t item = {};
		item.key = edge.m_Target;
		item.eEdgeKind = edge.m_eKind;
		item.bResident = bResident;
		if ( bResident ) {
			item.eKind = child.getKind();
			item.nGraphRevision = child.getRevision();
		}
		nHash = sirShaderGRDGHashScalar( nHash, item.key.m_nAssetId );
		nHash = sirShaderGRDGHashScalar( nHash, item.key.m_nTypeId );
		nHash = sirShaderGRDGHashScalar( nHash, item.key.m_nVariantId );
		nHash = sirShaderGRDGHashScalar( nHash, item.nGraphRevision );
		nHash = sirShaderGRDGHashScalar( nHash, ( uint64_t )item.eKind | ( ( uint64_t )item.eEdgeKind << 8 ) |
		                                                  ( ( uint64_t )item.bResident << 16 ) );
		pOut->pDependencies[iEdge] = item;
	}
	pOut->nFingerprint = nHash;
	pOut->nInvalidationSequence = 0;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult sirShaderStampMaterialInvalidation( SIRResourceGraphWriter& writer, const SIRResourceView& view,
                                                       sirShaderMaterialSnapshot_t *pSnapshot ) noexcept
{
	if ( pSnapshot == nullptr || !view.isValid() || view.getKind() != SIR_RESOURCE_KIND_COMPOSITION ||
	     pSnapshot->nFingerprint == 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	uint64_t nSequence = 0;
	const SIRResourceResult eResult = writer.getInvalidationSequence( view.getHandle(), &nSequence );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	pSnapshot->nInvalidationSequence = nSequence;
	pSnapshot->nFingerprint = sirShaderGRDGHashScalar( pSnapshot->nFingerprint, nSequence );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

static SIRResourceContentKey sirShaderGRDGContentKey( const void *pBytes, uint64_t nBytes, uint32_t nFormatId ) noexcept
{
	const uint8_t *pData = ( const uint8_t * )pBytes;
	uint64_t nLow = 14695981039346656037ULL;
	uint64_t nHigh = 7809847782465536322ULL;
	for ( uint64_t iByte = 0; iByte < nBytes; ++iByte ) {
		nLow ^= pData[iByte];
		nLow *= 1099511628211ULL;
		nHigh ^= ( uint8_t )( pData[iByte] ^ 0xa5U );
		nHigh *= 1099511628211ULL;
	}
	nLow = sirShaderGRDGHashScalar( nLow, nBytes );
	nHigh = sirShaderGRDGHashScalar( nHigh, nBytes );
	return { nLow, nHigh, nFormatId, 0 };
}

SIRResourceResult sirShaderPublishPackage( SIRResourceGraphWriter& writer, const sirShaderGRDGFormats_t& formats,
                                            const sirShaderPublicationRequest_t& request, SIRResourceHandle *pHandle ) noexcept
{
	if ( pHandle == nullptr || !sirShaderGRDGFormatsValid( formats ) || request.key.m_nTypeId != formats.nShaderTypeId ||
	     request.pBytes == nullptr || request.nBytes > SIR_SHADER_PACKAGE_MAX_BYTES ||
	     ( request.nDependencyCount != 0 && request.pDependencies == nullptr ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirShaderPackageHeader_t header = {};
	if ( sirShaderDecodePackageHeader( request.pBytes, request.nBytes, &header ) != SIR_GRAPHICS_SUCCESS ||
	     header.nAssetId != request.key.m_nAssetId ||
	     sirShaderValidatePackage( request.pBytes, request.nBytes, request.eShaderFormat, request.nAvailableFeatures ) !=
	         SIR_GRAPHICS_SUCCESS ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	SIRResourceStagingHandle staging = {};
	SIRResourceMutableBytes writable = {};
	SIRResourceResult eResult = writer.allocateStaging( request.nBytes, 8, request.nStagingDomain, &staging, &writable );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	memcpy( writable.m_pData, request.pBytes, ( size_t )request.nBytes );
	SIRResourceChunkPublication chunk = { { 0 }, sirShaderGRDGContentKey( writable.m_pData, request.nBytes, formats.nShaderFormatId ), staging, false };
	SIRResourcePublication publication = { request.key, SIR_RESOURCE_KIND_BLOB, 1, formats.nShaderFormatId, &chunk, 1,
		                                    request.pDependencies, request.nDependencyCount };
	eResult = writer.publishExpected( publication, request.nExpectedRevision, pHandle );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		( void )writer.cancelStaging( staging );
	}
	return eResult;
}

SIRResourceResult sirShaderPublishMaterial( SIRResourceGraphWriter& writer, const sirShaderGRDGFormats_t& formats,
                                             const sirShaderPublicationRequest_t& request, SIRResourceHandle *pHandle ) noexcept
{
	if ( pHandle == nullptr || !sirShaderGRDGFormatsValid( formats ) || request.key.m_nTypeId != formats.nMaterialTypeId ||
	     request.pBytes == nullptr || ( request.nDependencyCount != 0 && request.pDependencies == nullptr ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	sirShaderMaterialHeader_t header = {};
	if ( sirShaderDecodeMaterialHeader( request.pBytes, request.nBytes, &header ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderValidateMaterial( request.pBytes, request.nBytes ) != SIR_GRAPHICS_SUCCESS ||
	     header.nAssetId != request.key.m_nAssetId || header.nDependencyCount != request.nDependencyCount ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	for ( uint32_t iPass = 0; iPass < header.nPassCount; ++iPass ) {
		sirShaderMaterialPassRecord_t pass = {};
		const uint8_t *pRecord =
		    ( const uint8_t * )request.pBytes + SIR_SHADER_MATERIAL_HEADER_BYTES + iPass * SIR_SHADER_MATERIAL_PASS_BYTES;
		if ( sirShaderDecodeMaterialPass( pRecord, SIR_SHADER_MATERIAL_PASS_BYTES, &pass ) != SIR_GRAPHICS_SUCCESS ) {
			return SIR_RESOURCE_RESULT_INVALID_FORMAT;
		}
		if ( pass.iShaderDependency != SIR_SHADER_MATERIAL_NO_SHADER &&
		     request.pDependencies[pass.iShaderDependency].m_Target.m_nTypeId != formats.nShaderTypeId ) {
			return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
		}
	}
	SIRResourceStagingHandle staging = {};
	SIRResourceMutableBytes writable = {};
	SIRResourceResult eResult = writer.allocateStaging( request.nBytes, 8, request.nStagingDomain, &staging, &writable );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	memcpy( writable.m_pData, request.pBytes, ( size_t )request.nBytes );
	SIRResourceChunkPublication chunk = { { 0 }, sirShaderGRDGContentKey( writable.m_pData, request.nBytes, formats.nMaterialFormatId ), staging, false };
	SIRResourcePublication publication = { request.key, SIR_RESOURCE_KIND_COMPOSITION, 1, formats.nMaterialFormatId, &chunk, 1,
		                                    request.pDependencies, request.nDependencyCount };
	eResult = writer.publishExpected( publication, request.nExpectedRevision, pHandle );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		( void )writer.cancelStaging( staging );
	}
	return eResult;
}
