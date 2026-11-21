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

#include <Editor/Scene/SIREditorCatalogBridge.h>

#include <assert.h>
#include <string.h>

typedef struct sirEditorCatalogFixture_s
{
	sirEditorTextResource_t szResources[3];
	sirEditorCatalogIdentity_t szIdentities[3];
	sirEditorResourceReference_t szReferences[5];
	sirResourceEntry_t szEntries[3];
	sirResourceDependency_t szDependencies[4];
	uint32_t szScratch[3];
	uint8_t szSeen[3];
	sirEditorTextResourceIndex_t index;
	SIRResourceCatalog catalog;
	sirEditorCatalogBuildRequest_t request;
} sirEditorCatalogFixture_t;

static void sirEditorCatalogFixtureInit( sirEditorCatalogFixture_t& fixture ) noexcept
{
	fixture.szResources[0].nStableKey = 11;
	fixture.szResources[0].eKind = SIR_EDITOR_RESOURCE_EXTERNAL;
	fixture.szResources[1].nStableKey = 22;
	fixture.szResources[1].eKind = SIR_EDITOR_RESOURCE_EMBEDDED;
	fixture.szResources[2].nStableKey = 33;
	fixture.szResources[2].eKind = SIR_EDITOR_RESOURCE_EMBEDDED;
	fixture.szIdentities[0] = { 1001, 501, 7 };
	fixture.szIdentities[1] = { 2002, 0, 8 };
	fixture.szIdentities[2] = { 3003, 0, 9 };
	fixture.szReferences[0] = { 22, 11, 0, 1, 2 };
	fixture.szReferences[1] = { 33, 22, 1, 3, 4 };
	fixture.szReferences[2] = { 0, 11, 0, 5, 6 };
	fixture.szReferences[3] = { 22, 11, 0, 7, 8 };
	fixture.index = { fixture.szResources, 3, 3, 0 };
	fixture.request.pSourceIndex = &fixture.index;
	fixture.request.pReferences = fixture.szReferences;
	fixture.request.pIdentities = fixture.szIdentities;
	fixture.request.nReferenceCount = 4;
	fixture.request.nIdentityCount = 3;
	fixture.request.pCatalog = &fixture.catalog;
	fixture.request.pEntries = fixture.szEntries;
	fixture.request.pDependencies = fixture.szDependencies;
	fixture.request.pScratch = fixture.szScratch;
	fixture.request.pSeen = fixture.szSeen;
	fixture.request.nEntryCapacity = 3;
	fixture.request.nDependencyCapacity = 4;
}

static void sirEditorCatalogTestSuccess( void ) noexcept
{
	sirEditorCatalogFixture_t fixture = {};
	sirEditorCatalogFixtureInit( fixture );
	assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_OK );
	assert( fixture.catalog.getResourceCount() == 3 );
	assert( fixture.catalog.getDependencyCount() == 2 );
	sirResourceEntry_t entry = {};
	assert( fixture.catalog.findResource( 1001, &entry ) == SIR_RESOURCE_CATALOG_OK );
	assert( entry.eSource == SIR_RESOURCE_SOURCE_EXTERNAL && entry.nPathAtom == 501 && entry.nTypeId == 7 );
	assert( fixture.catalog.findResource( 2002, &entry ) == SIR_RESOURCE_CATALOG_OK );
	assert( entry.eSource == SIR_RESOURCE_SOURCE_EMBEDDED && entry.nPathAtom == 0 );
	uint64_t szOrder[3] = {};
	uint32_t nCount = 0;
	assert( fixture.catalog.buildLoadOrder( szOrder, 3, &nCount ) == SIR_RESOURCE_CATALOG_OK );
	assert( nCount == 3 && szOrder[0] == 1001 && szOrder[1] == 2002 && szOrder[2] == 3003 );
	assert( fixture.catalog.addResource( 4004, 7, 901, SIR_RESOURCE_SOURCE_EXTERNAL ) == SIR_RESOURCE_CATALOG_SEALED );
}

static void sirEditorCatalogTestInvalidInputs( void ) noexcept
{
	// Each independent pointer/capacity gate is checked before catalog mutation.
	for ( uint32_t iCase = 0; iCase < 14; ++iCase ) {
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		sirEditorCatalogBuildResult_t eExpected = SIR_EDITOR_CATALOG_INVALID_ARGUMENT;
		switch ( iCase ) {
		case 0:
			fixture.request.pCatalog = nullptr;
			break;
		case 1:
			fixture.request.pEntries = nullptr;
			break;
		case 2:
			fixture.request.pDependencies = nullptr;
			break;
		case 3:
			fixture.request.pScratch = nullptr;
			break;
		case 4:
			fixture.request.pSeen = nullptr;
			break;
		case 5:
			fixture.request.pIdentities = nullptr;
			break;
		case 6:
			fixture.index.pResources = nullptr;
			eExpected = SIR_EDITOR_CATALOG_INVALID_SOURCE;
			break;
		case 7:
			fixture.request.nEntryCapacity = 0;
			eExpected = SIR_EDITOR_CATALOG_OUT_OF_CAPACITY;
			break;
		case 8:
			fixture.request.nDependencyCapacity = 0;
			eExpected = SIR_EDITOR_CATALOG_OUT_OF_CAPACITY;
			break;
		case 9:
			fixture.szResources[0].nStableKey = 0;
			eExpected = SIR_EDITOR_CATALOG_INVALID_SOURCE;
			break;
		case 10:
			fixture.szResources[0].eKind = ( sirEditorTextResourceKind_t )0;
			eExpected = SIR_EDITOR_CATALOG_INVALID_SOURCE;
			break;
		case 11:
			fixture.szIdentities[0].nCanonicalId = 0;
			eExpected = SIR_EDITOR_CATALOG_INVALID_IDENTITY;
			break;
		case 12:
			fixture.request.nEntryCapacity = UINT32_MAX;
			eExpected = SIR_EDITOR_CATALOG_OUT_OF_CAPACITY;
			break;
		case 13:
			fixture.request.nDependencyCapacity = UINT32_MAX;
			eExpected = SIR_EDITOR_CATALOG_OUT_OF_CAPACITY;
			break;
		}
		assert( sirEditorBuildResourceCatalog( fixture.request ) == eExpected );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.request.pSourceIndex = nullptr;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_ARGUMENT );
		fixture.request.pSourceIndex = &fixture.index;
		fixture.request.pReferences = nullptr;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_ARGUMENT );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.index.nCount = 4;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_SOURCE );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.request.nIdentityCount = 2;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_SOURCE );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szResources[1].nStableKey = 11;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_SOURCE );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szIdentities[1].nCanonicalId = 1001;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_IDENTITY );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szIdentities[0].nPathAtom = 0;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_IDENTITY );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szIdentities[2].nTypeId = 0;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_IDENTITY );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.request.nEntryCapacity = 2;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_OUT_OF_CAPACITY );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.request.nDependencyCapacity = 1;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_OUT_OF_CAPACITY );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szReferences[0].iTargetResource = 3;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_SOURCE );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szReferences[0].nTargetKey = 22;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_SOURCE );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szReferences[0].nOwnerKey = 99;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_SOURCE );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szReferences[0].nOwnerKey = 11;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_SOURCE );
	}
}

static void sirEditorCatalogTestMemoryAliasing( void ) noexcept
{
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.request.pScratch = ( uint32_t * )fixture.szIdentities;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_ARGUMENT );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.request.pDependencies = ( sirResourceDependency_t * )fixture.szEntries;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_ARGUMENT );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.request.pSeen = ( uint8_t * )&fixture.catalog;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_ARGUMENT );
	}
}

static void sirEditorCatalogTestCycleAndLifetime( void ) noexcept
{
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szReferences[0] = { 22, 33, 2, 1, 2 };
		fixture.szReferences[1] = { 33, 22, 1, 3, 4 };
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_CYCLIC );
		assert( fixture.catalog.findResource( 1001, &fixture.szEntries[0] ) == SIR_RESOURCE_CATALOG_UNINITIALIZED );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.szReferences[0] = { 22, 22, 1, 1, 2 };
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_CYCLIC );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_OK );
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_INVALID_ARGUMENT );
		assert( fixture.catalog.getResourceCount() == 3 );
	}
	{
		sirEditorCatalogFixture_t fixture = {};
		sirEditorCatalogFixtureInit( fixture );
		fixture.index.nCount = 0;
		fixture.request.nIdentityCount = 0;
		fixture.request.nReferenceCount = 0;
		fixture.index.pResources = nullptr;
		fixture.request.pReferences = nullptr;
		fixture.request.pIdentities = nullptr;
		assert( sirEditorBuildResourceCatalog( fixture.request ) == SIR_EDITOR_CATALOG_OK );
		assert( fixture.catalog.getResourceCount() == 0 );
	}
}

int main( void )
{
	sirEditorCatalogTestSuccess();
	sirEditorCatalogTestInvalidInputs();
	sirEditorCatalogTestMemoryAliasing();
	sirEditorCatalogTestCycleAndLifetime();
	return 0;
}
