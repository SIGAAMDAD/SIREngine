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

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <malloc.h>
#endif

static constexpr const char *SIR_VT_IO_PATH = "sir_vt_resource_io_test.bin";

static void sirVTCheck( bool bResult, const char *pszCase ) noexcept
{
	if ( !bResult ) {
		fprintf( stderr, "VT resource IO: %s\n", pszCase );
		abort();
	}
}

typedef struct sirVirtualTextureTestAllocator_s
{
	uint32_t nOutstanding;
} sirVirtualTextureTestAllocator_t;

static void *sirVTTestAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pResult = nullptr;
#ifdef _WIN32
	pResult = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nAligned = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pResult, nAligned, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pResult != nullptr ) {
		++( ( sirVirtualTextureTestAllocator_t * )pContext )->nOutstanding;
	}
	return pResult;
}

static void sirVTTestFree( void *pContext, void *pData, uint64_t, uint32_t, uint32_t ) noexcept
{
	--( ( sirVirtualTextureTestAllocator_t * )pContext )->nOutstanding;
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static sirVirtualTextureLayout_t sirVTMakeLayout( uint64_t nRevision ) noexcept
{
	sirVirtualTextureLayout_t layout = {};
	layout.source.m_nAssetId = 551;
	layout.source.m_nTypeId = 14;
	layout.source.m_nVariantId = 1;
	layout.nRevision = nRevision;
	layout.nWidth = 2;
	layout.nHeight = 2;
	layout.nInteriorSize = 2;
	layout.nMipCount = 2;
	layout.nLayerCount = 1;
	layout.eCodec = SIR_VT_CODEC_RGBA8;
	return layout;
}

static uint64_t sirVTCookTest( uint64_t nRevision, uint8_t *pDestination, uint64_t nCapacity ) noexcept
{
	const sirVirtualTextureLayout_t layout = sirVTMakeLayout( nRevision );
	uint8_t szTexel[16] = {};
	for ( uint32_t iByte = 0; iByte < sizeof( szTexel ); ++iByte ) {
		szTexel[iByte] = ( uint8_t )( iByte * 7U + nRevision );
	}
	const sirVirtualTextureCookPage_t pages[2] = { { szTexel, 16, 0, 0, 0, 0 }, { szTexel, 16, 0, 0, 1, 0 } };
	uint32_t nCount = 0;
	uint64_t nBytes = 0;
	sirVTCheck( sirVTMeasureManifest( layout, &nCount, &nBytes ) == SIR_VT_MANIFEST_SUCCESS && nCount == 2, "measure" );
	sirVTCheck( sirVTCookManifest( layout, pages, 2, { pDestination, nCapacity } ) == SIR_VT_MANIFEST_SUCCESS, "cook" );
	return nBytes;
}

static void sirVTWriteFile( const uint8_t *pBytes, uint64_t nBytes, uint32_t nPrefix ) noexcept
{
	SIRPlatformFile file;
	sirVTCheck( file.open( SIR_VT_IO_PATH, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE | SIR_FILE_OPEN_WRITE ) == SIR_FILE_OK,
	            "open write" );
	uint8_t szPrefix[11] = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5 };
	uint64_t nWritten = 0;
	sirVTCheck( file.write( szPrefix, nPrefix, &nWritten ) == SIR_FILE_OK && nWritten == nPrefix, "prefix write" );
	sirVTCheck( file.write( pBytes, nBytes, &nWritten ) == SIR_FILE_OK && nWritten == nBytes, "manifest write" );
	sirVTCheck( file.close() == SIR_FILE_OK, "file close" );
}

static void sirVTTestImports( SIRResourceGraph& graph ) noexcept
{
	uint8_t szCooked[512] = {};
	const uint64_t nBytes = sirVTCookTest( 1, szCooked, sizeof( szCooked ) );
	sirVTWriteFile( szCooked, nBytes, 11 );
	sirVirtualTextureFileRequest_t request = {};
	request.pszUtf8Path = SIR_VT_IO_PATH;
	request.source = sirVTMakeLayout( 1 ).source;
	request.nOffset = 11;
	request.nBytes = nBytes;
	request.nAlignment = 8;

	SIRResourceHandle handle = {};
	sirVTCheck( sirVTImportFile( graph, request, &handle ) == SIR_RESOURCE_RESULT_SUCCESS, "first import" );
	{
		SIRResourceReadScope scope( graph );
		sirVirtualTextureManifestView_t view = {};
		sirVTCheck( sirVTGetResourceManifest( graph, scope, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS, "published view" );
		sirVTCheck( view.layout.nRevision == 1 && view.nPageCount == 2, "source revision and pages" );
		sirVirtualTexturePageKey_t key = {};
		key.source = view.layout.source;
		key.nRevision = view.layout.nRevision;
		sirVirtualTexturePageView_t page = {};
		sirVTCheck( sirVTGetManifestPage( view, key, &page ) == SIR_VT_MANIFEST_SUCCESS, "valid borrowed page" );
		sirVTCheck( page.nByteCount == 16 && page.pBytes[0] == 1, "cooked payload" );
	}

	// A valid but obsolete package cannot overwrite the current revision.
	sirVTCheck( sirVTImportFile( graph, request, &handle ) == SIR_RESOURCE_RESULT_STALE_REVISION, "replay rejected" );

	request.nExpectedRevision = 1;
	sirVTCheck( sirVTImportFile( graph, request, &handle ) == SIR_RESOURCE_RESULT_STALE_REVISION, "manifest revision rejected" );

	szCooked[100] ^= 0xA6;
	sirVTWriteFile( szCooked, nBytes, 11 );
	sirVTCheck( sirVTImportFile( graph, request, &handle ) == SIR_RESOURCE_RESULT_INVALID_FORMAT, "corrupt payload rejected" );
	{
		SIRResourceReadScope scope( graph );
		sirVirtualTextureManifestView_t view = {};
		sirVTCheck( sirVTGetResourceManifest( graph, scope, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS && view.layout.nRevision == 1,
		            "failed import leaves published record intact" );
	}

	request.nBytes = nBytes + 10;
	sirVTCheck( sirVTImportFile( graph, request, &handle ) == SIR_RESOURCE_RESULT_INVALID_FORMAT, "out of range rejected" );
	request.nBytes = nBytes;
	request.nOffset = UINT64_MAX;
	sirVTCheck( sirVTImportFile( graph, request, &handle ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT, "offset overflow rejected" );
	request.nOffset = 11;

	const uint64_t nNextBytes = sirVTCookTest( 2, szCooked, sizeof( szCooked ) );
	sirVTWriteFile( szCooked, nNextBytes, 11 );
	request.nBytes = nNextBytes;
	sirVTCheck( sirVTImportFile( graph, request, &handle ) == SIR_RESOURCE_RESULT_SUCCESS, "revision update" );
	{
		SIRResourceReadScope scope( graph );
		sirVirtualTextureManifestView_t view = {};
		sirVTCheck( sirVTGetResourceManifest( graph, scope, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS && view.layout.nRevision == 2,
		            "updated manifest" );
	}
}

int main( void )
{
	sirVirtualTextureTestAllocator_t allocator = {};
	{
		SIRResourceGraphConfig config = {};
		config.m_Allocator = { &allocator, sirVTTestAllocate, sirVTTestFree };
		config.m_nMaxResources = 4;
		config.m_nMaxStaging = 2;
		config.m_nMaxProviderPins = 2;
		config.m_nMaxCommittedBytes = 1024 * 1024;
		SIRResourceGraph graph( config );
		sirVTCheck( graph.isReady(), "graph ready" );
		sirVTTestImports( graph );
	}
	sirVTCheck( allocator.nOutstanding == 0, "all NMD-routed staging and graph allocations released" );
	sirVTCheck( SIRPlatformFile::remove( SIR_VT_IO_PATH ) == SIR_FILE_OK, "test file cleanup" );
	return 0;
}
