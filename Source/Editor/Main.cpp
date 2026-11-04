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

#include <Editor/Host/SIREditorSceneWorkspace.h>
#include <Editor/Host/SIREditorSceneJSON.h>

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <stdio.h>
#include <string.h>

static void sirEditorPrintHelp( void ) noexcept
{
	puts( "Usage: SIREditor --inspect-scene <path.tscn>" );
	puts( "       SIREditor --inspect-scene-json <path.tscn>" );
	puts( "       SIREditor --inspect-scene-json-file <path.tscn> <output.json>" );
	puts( "       SIREditor --version" );
	puts( "       SIREditor --help" );
	puts( "Scene inspection is read-only; serialized properties remain owned by Godot." );
}

static void sirEditorPrintSceneNodes( const SIREditorSceneWorkspace &workspace ) noexcept
{
	const sirEditorTextSceneIndex_t &index = workspace.getSceneIndex();
	const char *pszSource = workspace.getSource();
	puts( "Hierarchy:" );
	for ( uint32_t iNode = 0; iNode < index.nNodeCount; ++iNode ) {
		const sirEditorTextSceneNode_t &entry = index.storage.pNodes[iNode];
		const char *pszPath = index.storage.pszPaths + entry.nPathOffset;
		uint32_t nDepth = 0;
		if ( entry.node.nParentId != 0 ) {
			nDepth = 1;
			for ( uint32_t iChar = 0; iChar < entry.nPathLength; ++iChar ) {
				if ( pszPath[iChar] == '/' ) {
					++nDepth;
				}
			}
		}
		const uint32_t nIndent = nDepth < 32 ? nDepth : 32;
		printf( "%*s%.*s (line %u, id %llu)\n", ( int )( nIndent * 2 ), "", ( int )entry.nNameLength, pszSource + entry.nNameOffset,
		        entry.nSourceLine, ( unsigned long long )entry.node.nId );
	}
}

static void sirEditorPrintResources( const SIREditorSceneWorkspace &workspace ) noexcept
{
	const sirEditorTextResourceIndex_t &index = workspace.getResourceIndex();
	const char *pszSource = workspace.getSource();
	puts( "Resources:" );
	for ( uint32_t iResource = 0; iResource < index.nCount; ++iResource ) {
		const sirEditorTextResource_t &resource = index.pResources[iResource];
		const char *pszKind = resource.eKind == SIR_EDITOR_RESOURCE_EXTERNAL ? "external" : "embedded";
		printf( "  [%s] %.*s : %.*s", pszKind, ( int )resource.nIdLength, pszSource + resource.nIdOffset, ( int )resource.nTypeLength,
		        pszSource + resource.nTypeOffset );
		if ( resource.nPathLength != 0 ) {
			printf( " -> %.*s", ( int )resource.nPathLength, pszSource + resource.nPathOffset );
		}
		printf( " (line %u)\n", resource.nSourceLine );
	}
}

static int sirEditorInspectScene( const char *pszPath, bool bJSON, const char *pszOutputPath = nullptr ) noexcept
{
	constexpr uint64_t N_MIB = UINT64_C( 1024 ) * 1024;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 512 * N_MIB, 256 * N_MIB, 4 * N_MIB, 2 };
	if ( !memory.init( config ) ) {
		fputs( "SIREditor: cannot initialize editor memory domain\n", stderr );
		return 3;
	}
	SIRMemorySegment *pSegment = memory.createSegment( "EditorScene", 384 * N_MIB, 240 * N_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	if ( pSegment == nullptr ) {
		fputs( "SIREditor: cannot reserve editor workspace segment\n", stderr );
		return 3;
	}

	SIREditorSceneWorkspace workspace;
	if ( !workspace.initialize( pSegment ) ) {
		fputs( "SIREditor: cannot initialize scene workspace\n", stderr );
		return 3;
	}
	const sirEditorWorkspaceResult_t eResult = workspace.openTextFile( pszPath );
	if ( eResult != SIR_EDITOR_WORKSPACE_OK ) {
		fprintf( stderr, "SIREditor: cannot inspect scene (code %u, line %u)\n", ( unsigned )eResult, workspace.getErrorLine() );
		return 2;
	}

	if ( bJSON ) {
		if ( pszOutputPath != nullptr ) {
			return sirEditorWriteSceneJSONFile( workspace, pszPath, pszOutputPath ) ? 0 : 4;
		}
		return sirEditorWriteSceneJSON( workspace, pszPath, stdout ) ? 0 : 4;
	}

	printf( "SIREditor scene: %s\n", pszPath );
	printf( "Nodes: %u\n", workspace.getSceneIndex().nNodeCount );
	printf( "Resources: %u\n", workspace.getResourceIndex().nCount );
	printf( "Document revision: %llu\n", ( unsigned long long )workspace.getDocument().getRevision() );
	printf( "Visible hierarchy rows: %u\n", workspace.getHierarchy().getVisibleCount() );
	sirEditorPrintSceneNodes( workspace );
	sirEditorPrintResources( workspace );
	return 0;
}

int main( int nArgs, char **pszArgs )
{
	if ( nArgs == 2 && strcmp( pszArgs[1], "--help" ) == 0 ) {
		sirEditorPrintHelp();
		return 0;
	}
	if ( nArgs == 2 && strcmp( pszArgs[1], "--version" ) == 0 ) {
		puts( "SIREditor native scene host " SIR_VERSION );
		return 0;
	}
	if ( nArgs == 3 && strcmp( pszArgs[1], "--inspect-scene" ) == 0 ) {
		return sirEditorInspectScene( pszArgs[2], false );
	}
	if ( nArgs == 3 && strcmp( pszArgs[1], "--inspect-scene-json" ) == 0 ) {
		return sirEditorInspectScene( pszArgs[2], true );
	}
	if ( nArgs == 4 && strcmp( pszArgs[1], "--inspect-scene-json-file" ) == 0 ) {
		return sirEditorInspectScene( pszArgs[2], true, pszArgs[3] );
	}
	sirEditorPrintHelp();
	return 2;
}
