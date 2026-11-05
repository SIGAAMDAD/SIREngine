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

#include <Renderer/VirtualTexturing/SIRVirtualTextureResourceIO.h>

#include <Core/Platform/SIRPlatformFile.h>

#include <limits.h>

static bool sirVTEqualSource( const sirResourceKey_t& left, const sirResourceKey_t& right ) noexcept
{
	return left.m_nAssetId == right.m_nAssetId && left.m_nTypeId == right.m_nTypeId && left.m_nVariantId == right.m_nVariantId;
}

static uint64_t sirVTHashChunkBytes( const uint8_t *pBytes, uint64_t nBytes, uint64_t nSeed ) noexcept
{
	uint64_t nHash = nSeed;
	for ( uint64_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash ^= pBytes[iByte];
		nHash *= UINT64_C( 1099511628211 );
	}
	return nHash;
}

static SIRResourceResult sirVTValidateLoadedBytes( const SIRResourceMutableBytes& bytes,
                                                   const sirVirtualTextureFileRequest_t& request ) noexcept
{
	sirVirtualTextureManifestView_t view = {};
	if ( sirVTParseManifest( bytes.m_pData, bytes.m_nSize, &view ) != SIR_VT_MANIFEST_SUCCESS ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	if ( !sirVTEqualSource( view.layout.source, request.source ) || view.layout.nRevision != request.nExpectedRevision + 1U ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult sirVTImportFile( SIRResourceGraph& graph, const sirVirtualTextureFileRequest_t& request,
                                   SIRResourceHandle *pHandle ) noexcept
{
	SIRResourceGraphWriter *pWriter = graph.getWriter();
	if ( pWriter == nullptr ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( pHandle == nullptr || request.pszUtf8Path == nullptr || request.pszUtf8Path[0] == '\0' || request.source.m_nAssetId == 0 ||
	     request.source.m_nTypeId == 0 || request.nBytes < SIR_VT_MANIFEST_HEADER_BYTES || request.nBytes > SIZE_MAX ||
	     request.nOffset > INT64_MAX || request.nBytes > ( uint64_t )INT64_MAX - request.nOffset ||
	     request.nExpectedRevision == UINT64_MAX || request.nAlignment == 0 || ( request.nAlignment & ( request.nAlignment - 1U ) ) != 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}

	SIRPlatformFile file;
	if ( file.open( request.pszUtf8Path, SIR_FILE_OPEN_READ ) != SIR_FILE_OK ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}

	uint64_t nFileSize = 0;
	if ( file.getSize( &nFileSize ) != SIR_FILE_OK ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	if ( request.nOffset > nFileSize || request.nBytes > nFileSize - request.nOffset ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}

	SIRResourceStagingHandle staging = {};
	SIRResourceMutableBytes bytes = {};
	const SIRResourceResult eAllocation =
	    pWriter->allocateStaging( request.nBytes, request.nAlignment, request.nStagingDomain, &staging, &bytes );
	if ( eAllocation != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eAllocation;
	}

	uint64_t nTransferred = 0;
	const sirFileResult_t eRead = file.readAt( request.nOffset, bytes.m_pData, request.nBytes, &nTransferred );
	SIRResourceResult eResult = SIR_RESOURCE_RESULT_FAILED;
	if ( eRead == SIR_FILE_OK && nTransferred == request.nBytes ) {
		eResult = sirVTValidateLoadedBytes( bytes, request );
		if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
			const SIRResourceContentKey content = { sirVTHashChunkBytes( bytes.m_pData, bytes.m_nSize, UINT64_C( 14695981039346656037 ) ),
			                                        sirVTHashChunkBytes( bytes.m_pData, bytes.m_nSize, UINT64_C( 7809847782465536322 ) ),
			                                        SIR_VT_MANIFEST_MAGIC, SIR_VT_MANIFEST_VERSION };
			const SIRResourceChunkPublication chunk = { { 0 }, content, staging, false };
			const SIRResourcePublication publication = {
			    request.source, SIR_RESOURCE_KIND_IMAGE, SIR_VT_MANIFEST_VERSION, SIR_VT_MANIFEST_MAGIC, &chunk, 1, nullptr, 0 };
			eResult = pWriter->publishExpected( publication, request.nExpectedRevision, pHandle );
			if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
				return SIR_RESOURCE_RESULT_SUCCESS;
			}
		}
	}
	pWriter->cancelStaging( staging );
	return eResult;
}

SIRResourceResult sirVTGetResourceManifest( const SIRResourceGraph& graph, const SIRResourceReadScope& scope, SIRResourceHandle handle,
                                            sirVirtualTextureManifestView_t *pView ) noexcept
{
	if ( pView == nullptr ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}

	SIRResourceView resource;
	const SIRResourceResult eLookup = graph.resolve( scope, handle, &resource );
	if ( eLookup != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eLookup;
	}

	SIRResourceInfo info = {};
	const SIRResourceResult eInfo = resource.getInfo( &info );
	if ( eInfo != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eInfo;
	}
	if ( info.m_eKind != SIR_RESOURCE_KIND_IMAGE || info.m_nSchemaVersion != SIR_VT_MANIFEST_VERSION ||
	     info.m_nFormatId != SIR_VT_MANIFEST_MAGIC || info.m_nChunkCount != 1 ) {
		return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}

	SIRResourceConstBytes bytes = {};
	const SIRResourceResult eChunk = resource.getChunk( { 0 }, &bytes );
	if ( eChunk != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eChunk;
	}
	sirVirtualTextureManifestView_t view = {};
	if ( sirVTParseManifest( bytes.m_pData, bytes.m_nSize, &view ) != SIR_VT_MANIFEST_SUCCESS ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	if ( !sirVTEqualSource( view.layout.source, info.m_Key ) || view.layout.nRevision != info.m_nRevision ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	*pView = view;
	return SIR_RESOURCE_RESULT_SUCCESS;
}
