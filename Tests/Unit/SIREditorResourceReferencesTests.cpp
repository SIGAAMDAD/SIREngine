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

#include <Editor/Scene/SIREditorResourceReferences.h>

#include <assert.h>
#include <string.h>

// Standalone focused compilation substitutes only the existing ID lookup.
// The native CTest target links sirEditorFindTextResource from the real scanner.
#if defined( SIR_EDITOR_RESOURCE_REFERENCE_STANDALONE )
sirEditorTextResourceResult_t sirEditorFindTextResource( const char *pszSource, const sirEditorTextResourceIndex_t &index,
                                                         sirEditorTextResourceKind_t eKind, const char *pszId, uint32_t nIdLength,
                                                         uint32_t *pOutIndex ) noexcept
{
	for ( uint32_t i = 0; i < index.nCount; ++i ) {
		const sirEditorTextResource_t &item = index.pResources[i];
		if ( item.eKind == eKind && item.nIdLength == nIdLength && memcmp( pszSource + item.nIdOffset, pszId, nIdLength ) == 0 ) {
			*pOutIndex = i;
			return SIR_EDITOR_RESOURCE_OK;
		}
	}
	return SIR_EDITOR_RESOURCE_MISSING_REFERENCE;
}

#endif

static void sirTestResourceReferences( void )
{
	const char szSource[] = "[gd_scene format=3]\n"
	                        "[ext_resource type=\"Script\" path=\"res://file.gd\" id=\"ext_a\"]\n"
	                        "[sub_resource type=\"Material\" id=\"sub_a\"]\n"
	                        "first = ExtResource(\"ext_a\")\n"
	                        "comment = \"SubResource(\\\"sub_b\\\")\" # SubResource(\"sub_b\")\n"
	                        "[sub_resource type=\"Material\" id=\"sub_b\"]\n"
	                        "first = SubResource(\"sub_a\")\n"
	                        "noise = \"text on one line\n"
	                        "ExtResource(\\\"ext_a\\\") in a quoted multiline value\"\n"
	                        "[node name=\"Root\" type=\"Node\"]\n"
	                        "material = SubResource(\"sub_b\")\n";
	sirEditorTextResource_t szResources[3] = {};
	szResources[0].nStableKey = 101;
	szResources[0].eKind = SIR_EDITOR_RESOURCE_EXTERNAL;
	szResources[0].nSourceLine = 2;
	szResources[0].nIdLength = 5;
	szResources[0].nIdOffset = ( uint32_t )( strstr( szSource, "id=\"ext_a\"" ) - szSource + 4 );
	szResources[1].nStableKey = 202;
	szResources[1].eKind = SIR_EDITOR_RESOURCE_EMBEDDED;
	szResources[1].nSourceLine = 3;
	szResources[1].nIdLength = 5;
	szResources[1].nIdOffset = ( uint32_t )( strstr( szSource, "id=\"sub_a\"" ) - szSource + 4 );
	szResources[2].nStableKey = 303;
	szResources[2].eKind = SIR_EDITOR_RESOURCE_EMBEDDED;
	szResources[2].nSourceLine = 6;
	szResources[2].nIdLength = 5;
	szResources[2].nIdOffset = ( uint32_t )( strstr( szSource, "id=\"sub_b\"" ) - szSource + 4 );
	sirEditorTextResourceIndex_t index = { szResources, 3, 3, 0 };
	sirEditorResourceReferenceScan_t request = { szSource, sizeof( szSource ) - 1, &index, nullptr, 0 };
	uint32_t nCount = UINT32_MAX;
	assert( sirEditorScanResourceReferences( request, &nCount ) == SIR_EDITOR_RESOURCE_OK );
	assert( nCount == 3 );
	sirEditorResourceReference_t szOut[4] = {};
	szOut[0].nTargetKey = 999;
	request.pOutput = szOut;
	request.nCapacity = 2;
	assert( sirEditorScanResourceReferences( request, &nCount ) == SIR_EDITOR_RESOURCE_OUT_OF_CAPACITY );
	assert( nCount == 3 && szOut[0].nTargetKey == 999 );
	request.nCapacity = 4;
	assert( sirEditorScanResourceReferences( request, &nCount ) == SIR_EDITOR_RESOURCE_OK );
	assert( nCount == 3 );
	assert( szOut[0].nOwnerKey == 202 && szOut[0].nTargetKey == 101 && szOut[0].iTargetResource == 0 );
	assert( szOut[1].nOwnerKey == 303 && szOut[1].nTargetKey == 202 && szOut[1].iTargetResource == 1 );
	assert( szOut[2].nOwnerKey == 0 && szOut[2].nTargetKey == 303 && szOut[2].iTargetResource == 2 );
	assert( strstr( szSource + szOut[0].nCallOffset, "ExtResource(\"ext_a\")" ) == szSource + szOut[0].nCallOffset );
	assert( szOut[0].nSourceLine == 4 && szOut[1].nSourceLine == 7 && szOut[2].nSourceLine == 11 );

	// An invalid call or source-index mutation cannot publish partial results.
	char szBad[sizeof( szSource )] = {};
	memcpy( szBad, szSource, sizeof( szSource ) );
	char *pszTarget = strstr( szBad, "SubResource(\"sub_a\")" );
	assert( pszTarget != nullptr );
	pszTarget[13] = 'x';
	request.pszSource = szBad;
	szOut[0].nTargetKey = 999;
	assert( sirEditorScanResourceReferences( request, &nCount ) != SIR_EDITOR_RESOURCE_OK );
	assert( nCount == 0 && szOut[0].nTargetKey == 999 );
	request.pszSource = szSource;
	szResources[1].nIdOffset = UINT32_MAX;
	assert( sirEditorScanResourceReferences( request, &nCount ) == SIR_EDITOR_RESOURCE_INVALID_ARGUMENT );
	assert( nCount == 0 && szOut[0].nTargetKey == 999 );
	szResources[1].nIdOffset = ( uint32_t )( strstr( szSource, "id=\"sub_a\"" ) - szSource + 4 );
	request.pOutput = ( sirEditorResourceReference_t * )szSource;
	assert( sirEditorScanResourceReferences( request, &nCount ) == SIR_EDITOR_RESOURCE_INVALID_ARGUMENT );
	request.pOutput = szOut;
	assert( sirEditorScanResourceReferences( request, nullptr ) == SIR_EDITOR_RESOURCE_INVALID_ARGUMENT );
}

#if !defined( SIR_EDITOR_RESOURCE_REFERENCE_STANDALONE )
static void sirTestNativeScannerIntegration( void )
{
	const char szSource[] = "[gd_scene format=3]\n"
	                        "[ext_resource type=\"Script\" path=\"res://example.gd\" id=\"ext_a\"]\n"
	                        "[sub_resource type=\"Resource\" id=\"sub_a\"]\n"
	                        "script = ExtResource(\"ext_a\")\n"
	                        "[node name=\"Root\" type=\"Node\"]\n"
	                        "resource = SubResource(\"sub_a\")\n";
	sirEditorTextResource_t szResources[2] = {};
	sirEditorTextResourceIndex_t index = { szResources, 2, 0, 0 };
	assert( sirEditorScanTextResources( szSource, sizeof( szSource ) - 1, &index ) == SIR_EDITOR_RESOURCE_OK );
	assert( index.nCount == 2 );
	sirEditorResourceReference_t szReferences[2] = {};
	sirEditorResourceReferenceScan_t request = { szSource, sizeof( szSource ) - 1, &index, szReferences, 2 };
	uint32_t nCount = 0;
	assert( sirEditorScanResourceReferences( request, &nCount ) == SIR_EDITOR_RESOURCE_OK );
	assert( nCount == 2 );
	assert( szReferences[0].nOwnerKey == index.pResources[1].nStableKey );
	assert( szReferences[0].nTargetKey == index.pResources[0].nStableKey );
	assert( szReferences[1].nOwnerKey == 0 );
	assert( szReferences[1].nTargetKey == index.pResources[1].nStableKey );
}
#endif

int main( void )
{
	sirTestResourceReferences();
#if !defined( SIR_EDITOR_RESOURCE_REFERENCE_STANDALONE )
	sirTestNativeScannerIntegration();
#endif
	return 0;
}
