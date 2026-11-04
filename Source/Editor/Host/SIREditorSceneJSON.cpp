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

#include <Editor/Host/SIREditorSceneJSON.h>
#include <Editor/Host/SIREditorSceneWorkspace.h>

#include <string.h>

static void sirEditorWriteJSONString( FILE *pOutput, const char *pData, size_t nLength ) noexcept
{
	static const char szHex[] = "0123456789abcdef";
	fputc( '"', pOutput );
	for ( size_t i = 0; i < nLength; ++i ) {
		const unsigned char nChar = ( unsigned char )pData[i];
		if ( nChar == '"' || nChar == '\\' ) {
			fputc( '\\', pOutput );
			fputc( nChar, pOutput );
		} else if ( nChar < 0x20 ) {
			fputs( "\\u00", pOutput );
			fputc( szHex[nChar >> 4], pOutput );
			fputc( szHex[nChar & 15], pOutput );
		} else {
			fputc( nChar, pOutput );
		}
	}
	fputc( '"', pOutput );
}

static bool sirEditorValidSpan( size_t nSourceBytes, uint32_t nOffset, uint32_t nLength ) noexcept
{
	return nOffset <= nSourceBytes && nLength <= nSourceBytes - nOffset;
}

bool sirEditorWriteSceneJSON( const SIREditorSceneWorkspace &workspace, const char *pszScenePath, FILE *pOutput ) noexcept
{
	if ( !workspace.isOpen() || pszScenePath == nullptr || pOutput == nullptr || workspace.getSource() == nullptr ) {
		return false;
	}

	const sirEditorTextSceneIndex_t &scene = workspace.getSceneIndex();
	const sirEditorTextResourceIndex_t &resources = workspace.getResourceIndex();
	const char *pszSource = workspace.getSource();
	const size_t nSourceBytes = workspace.getSourceBytes();
	if ( scene.nNodeCount > scene.storage.nNodeCapacity || resources.nCount > resources.nCapacity ) {
		return false;
	}
	if ( scene.nNodeCount != 0 && ( scene.storage.pNodes == nullptr || scene.storage.pszPaths == nullptr ) ) {
		return false;
	}
	if ( resources.nCount != 0 && resources.pResources == nullptr ) {
		return false;
	}

	// Prevalidate every span before writing the first byte. Invalid editor data
	// may never produce a superficially successful partial JSON document.
	for ( uint32_t i = 0; i < scene.nNodeCount; ++i ) {
		const sirEditorTextSceneNode_t &node = scene.storage.pNodes[i];
		if ( !sirEditorValidSpan( nSourceBytes, node.nNameOffset, node.nNameLength ) || node.nPathOffset > scene.nPathBytes ||
		     node.nPathLength > scene.nPathBytes - node.nPathOffset ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < resources.nCount; ++i ) {
		const sirEditorTextResource_t &resource = resources.pResources[i];
		if ( !sirEditorValidSpan( nSourceBytes, resource.nIdOffset, resource.nIdLength ) ||
		     !sirEditorValidSpan( nSourceBytes, resource.nTypeOffset, resource.nTypeLength ) ||
		     !sirEditorValidSpan( nSourceBytes, resource.nPathOffset, resource.nPathLength ) ||
		     !sirEditorValidSpan( nSourceBytes, resource.nUidOffset, resource.nUidLength ) ) {
			return false;
		}
	}

	fputs( "{\"schema\":1,\"scene\":", pOutput );
	sirEditorWriteJSONString( pOutput, pszScenePath, strlen( pszScenePath ) );
	fprintf( pOutput, ",\"revision\":\"%llu\",\"nodes\":[", ( unsigned long long )workspace.getDocument().getRevision() );
	for ( uint32_t i = 0; i < scene.nNodeCount; ++i ) {
		const sirEditorTextSceneNode_t &node = scene.storage.pNodes[i];
		if ( i != 0 ) {
			fputc( ',', pOutput );
		}
		fprintf( pOutput, "{\"id\":\"%llu\",\"parent\":\"%llu\",\"name\":", ( unsigned long long )node.node.nId,
		         ( unsigned long long )node.node.nParentId );
		sirEditorWriteJSONString( pOutput, pszSource + node.nNameOffset, node.nNameLength );
		fputs( ",\"path\":", pOutput );
		sirEditorWriteJSONString( pOutput, scene.storage.pszPaths + node.nPathOffset, node.nPathLength );
		fprintf( pOutput, ",\"line\":%u,\"unique_id\":%u}", node.nSourceLine, node.nGodotUniqueId );
	}
	fputs( "],\"resources\":[", pOutput );
	for ( uint32_t i = 0; i < resources.nCount; ++i ) {
		const sirEditorTextResource_t &resource = resources.pResources[i];
		if ( i != 0 ) {
			fputc( ',', pOutput );
		}
		fputs( "{\"kind\":", pOutput );
		const char *pszKind = resource.eKind == SIR_EDITOR_RESOURCE_EXTERNAL ? "external" : "embedded";
		sirEditorWriteJSONString( pOutput, pszKind, strlen( pszKind ) );
		fputs( ",\"id\":", pOutput );
		sirEditorWriteJSONString( pOutput, pszSource + resource.nIdOffset, resource.nIdLength );
		fputs( ",\"type\":", pOutput );
		sirEditorWriteJSONString( pOutput, pszSource + resource.nTypeOffset, resource.nTypeLength );
		fputs( ",\"path\":", pOutput );
		sirEditorWriteJSONString( pOutput, pszSource + resource.nPathOffset, resource.nPathLength );
		fputs( ",\"uid\":", pOutput );
		sirEditorWriteJSONString( pOutput, pszSource + resource.nUidOffset, resource.nUidLength );
		fprintf( pOutput, ",\"line\":%u}", resource.nSourceLine );
	}
	fputs( "]}\n", pOutput );
	return ferror( pOutput ) == 0;
}

bool sirEditorWriteSceneJSONFile( const SIREditorSceneWorkspace &workspace, const char *pszScenePath, const char *pszOutputPath ) noexcept
{
	if ( pszScenePath == nullptr || pszOutputPath == nullptr || pszOutputPath[0] == '\0' || strcmp( pszScenePath, pszOutputPath ) == 0 ) {
		return false;
	}

	constexpr size_t N_MAX_STAGING_PATH = 4096;
	char szStaging[N_MAX_STAGING_PATH];
	const int nNameLength = snprintf( szStaging, sizeof( szStaging ), "%s.tmp", pszOutputPath );
	if ( nNameLength <= 0 || ( size_t )nNameLength >= sizeof( szStaging ) ) {
		return false;
	}

	FILE *pOutput = fopen( szStaging, "wb" );
	if ( pOutput == nullptr ) {
		return false;
	}

	const bool bWritten = sirEditorWriteSceneJSON( workspace, pszScenePath, pOutput );
	const bool bFlushed = fflush( pOutput ) == 0;
	const bool bClosed = fclose( pOutput ) == 0;
	if ( !bWritten || !bFlushed || !bClosed ) {
		remove( szStaging );
		return false;
	}

	if ( rename( szStaging, pszOutputPath ) != 0 ) {
		remove( szStaging );
		return false;
	}
	return true;
}
