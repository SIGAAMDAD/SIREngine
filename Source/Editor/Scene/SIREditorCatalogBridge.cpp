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

#include <limits.h>

typedef struct sirEditorCatalogMemorySpan_s
{
	const void *pData;
	size_t nBytes;
} sirEditorCatalogMemorySpan_t;

static bool sirEditorCatalogOverlaps( const sirEditorCatalogMemorySpan_t& left, const sirEditorCatalogMemorySpan_t& right ) noexcept
{
	if ( left.pData == nullptr || right.pData == nullptr || left.nBytes == 0 || right.nBytes == 0 ) {
		return false;
	}
	const uintptr_t nLeft = ( uintptr_t )left.pData;
	const uintptr_t nRight = ( uintptr_t )right.pData;
	if ( left.nBytes > UINTPTR_MAX - nLeft || right.nBytes > UINTPTR_MAX - nRight ) {
		return true;
	}
	return nLeft < nRight + right.nBytes && nRight < nLeft + left.nBytes;
}

static uint32_t sirEditorCatalogFindOwner( const sirEditorTextResourceIndex_t& index, uint64_t nOwnerKey ) noexcept
{
	for ( uint32_t iResource = 0; iResource < index.nCount; ++iResource ) {
		if ( index.pResources[iResource].nStableKey == nOwnerKey ) {
			return iResource;
		}
	}
	return UINT32_MAX;
}

static bool sirEditorCatalogRepeatedEdge( const sirEditorCatalogBuildRequest_t& request, uint32_t iReference ) noexcept
{
	const sirEditorResourceReference_t& reference = request.pReferences[iReference];
	for ( uint32_t iPrior = 0; iPrior < iReference; ++iPrior ) {
		const sirEditorResourceReference_t& prior = request.pReferences[iPrior];
		if ( prior.nOwnerKey == reference.nOwnerKey && prior.nTargetKey == reference.nTargetKey ) {
			return true;
		}
	}
	return false;
}

static sirEditorCatalogBuildResult_t sirEditorCatalogPreflight( const sirEditorCatalogBuildRequest_t& request,
                                                                uint32_t *pUniqueEdges ) noexcept
{
	if ( request.pSourceIndex == nullptr || request.pCatalog == nullptr || request.pEntries == nullptr ||
	     request.pDependencies == nullptr || request.pScratch == nullptr || request.pSeen == nullptr || pUniqueEdges == nullptr ||
	     ( request.nIdentityCount != 0 && request.pIdentities == nullptr ) ||
	     ( request.nReferenceCount != 0 && request.pReferences == nullptr ) ) {
		return SIR_EDITOR_CATALOG_INVALID_ARGUMENT;
	}
	const sirEditorTextResourceIndex_t& index = *request.pSourceIndex;
	if ( index.nCount > index.nCapacity || ( index.nCount != 0 && index.pResources == nullptr ) ||
	     index.nCount != request.nIdentityCount ) {
		return SIR_EDITOR_CATALOG_INVALID_SOURCE;
	}
	if ( request.nEntryCapacity == 0 || request.nDependencyCapacity == 0 || request.nEntryCapacity < index.nCount ||
	     request.nEntryCapacity > UINT32_MAX / sizeof( sirResourceEntry_t ) ||
	     request.nDependencyCapacity > UINT32_MAX / sizeof( sirResourceDependency_t ) ) {
		return SIR_EDITOR_CATALOG_OUT_OF_CAPACITY;
	}
	// Catalog output and scratch must never overwrite the source identities,
	// reference index, catalog object, or another output allocation.
	const sirEditorCatalogMemorySpan_t szOutputs[] = {
	    { request.pEntries, ( size_t )request.nEntryCapacity * sizeof( sirResourceEntry_t ) },
	    { request.pDependencies, ( size_t )request.nDependencyCapacity * sizeof( sirResourceDependency_t ) },
	    { request.pScratch, ( size_t )request.nEntryCapacity * sizeof( uint32_t ) },
	    { request.pSeen, ( size_t )request.nEntryCapacity } };
	const sirEditorCatalogMemorySpan_t szInputs[] = {
	    { request.pSourceIndex, sizeof( *request.pSourceIndex ) },
	    { index.pResources, ( size_t )index.nCount * sizeof( sirEditorTextResource_t ) },
	    { request.pIdentities, ( size_t )request.nIdentityCount * sizeof( sirEditorCatalogIdentity_t ) },
	    { request.pReferences, ( size_t )request.nReferenceCount * sizeof( sirEditorResourceReference_t ) },
	    { request.pCatalog, sizeof( *request.pCatalog ) } };
	for ( uint32_t iOutput = 0; iOutput < 4; ++iOutput ) {
		for ( uint32_t iInput = 0; iInput < 5; ++iInput ) {
			if ( sirEditorCatalogOverlaps( szOutputs[iOutput], szInputs[iInput] ) ) {
				return SIR_EDITOR_CATALOG_INVALID_ARGUMENT;
			}
		}
		for ( uint32_t iPrior = 0; iPrior < iOutput; ++iPrior ) {
			if ( sirEditorCatalogOverlaps( szOutputs[iOutput], szOutputs[iPrior] ) ) {
				return SIR_EDITOR_CATALOG_INVALID_ARGUMENT;
			}
		}
	}
	for ( uint32_t iResource = 0; iResource < index.nCount; ++iResource ) {
		const sirEditorTextResource_t& resource = index.pResources[iResource];
		const sirEditorCatalogIdentity_t& identity = request.pIdentities[iResource];
		if ( resource.nStableKey == 0 ||
		     ( resource.eKind != SIR_EDITOR_RESOURCE_EXTERNAL && resource.eKind != SIR_EDITOR_RESOURCE_EMBEDDED ) ) {
			return SIR_EDITOR_CATALOG_INVALID_SOURCE;
		}
		if ( identity.nCanonicalId == 0 || identity.nTypeId == 0 ||
		     ( resource.eKind == SIR_EDITOR_RESOURCE_EXTERNAL && identity.nPathAtom == 0 ) ) {
			return SIR_EDITOR_CATALOG_INVALID_IDENTITY;
		}
		for ( uint32_t iPrior = 0; iPrior < iResource; ++iPrior ) {
			if ( resource.nStableKey == index.pResources[iPrior].nStableKey ) {
				return SIR_EDITOR_CATALOG_INVALID_SOURCE;
			}
			if ( identity.nCanonicalId == request.pIdentities[iPrior].nCanonicalId ) {
				return SIR_EDITOR_CATALOG_INVALID_IDENTITY;
			}
		}
	}
	uint32_t nEdges = 0;
	for ( uint32_t iReference = 0; iReference < request.nReferenceCount; ++iReference ) {
		const sirEditorResourceReference_t& reference = request.pReferences[iReference];
		if ( reference.iTargetResource >= index.nCount || reference.nTargetKey != index.pResources[reference.iTargetResource].nStableKey ) {
			return SIR_EDITOR_CATALOG_INVALID_SOURCE;
		}
		if ( reference.nOwnerKey == 0 ) {
			continue;
		}
		const uint32_t iOwner = sirEditorCatalogFindOwner( index, reference.nOwnerKey );
		if ( iOwner == UINT32_MAX || index.pResources[iOwner].eKind != SIR_EDITOR_RESOURCE_EMBEDDED ) {
			return SIR_EDITOR_CATALOG_INVALID_SOURCE;
		}
		if ( !sirEditorCatalogRepeatedEdge( request, iReference ) ) {
			if ( nEdges == UINT32_MAX ) {
				return SIR_EDITOR_CATALOG_OUT_OF_CAPACITY;
			}
			++nEdges;
		}
	}
	if ( nEdges > request.nDependencyCapacity ) {
		return SIR_EDITOR_CATALOG_OUT_OF_CAPACITY;
	}
	*pUniqueEdges = nEdges;
	return SIR_EDITOR_CATALOG_OK;
}

sirEditorCatalogBuildResult_t sirEditorBuildResourceCatalog( const sirEditorCatalogBuildRequest_t& request ) noexcept
{
	uint32_t nEdges = 0;
	const sirEditorCatalogBuildResult_t ePreflight = sirEditorCatalogPreflight( request, &nEdges );
	if ( ePreflight != SIR_EDITOR_CATALOG_OK ) {
		return ePreflight;
	}
	SIRResourceCatalog& catalog = *request.pCatalog;
	if ( catalog.initialize( request.pEntries, request.nEntryCapacity, request.pDependencies, request.nDependencyCapacity, request.pScratch,
	                         request.pSeen ) != SIR_RESOURCE_CATALOG_OK ) {
		return SIR_EDITOR_CATALOG_INVALID_ARGUMENT;
	}
	const sirEditorTextResourceIndex_t& index = *request.pSourceIndex;
	for ( uint32_t iResource = 0; iResource < index.nCount; ++iResource ) {
		const sirEditorCatalogIdentity_t& identity = request.pIdentities[iResource];
		const sirResourceSource_t eSource =
		    index.pResources[iResource].eKind == SIR_EDITOR_RESOURCE_EXTERNAL ? SIR_RESOURCE_SOURCE_EXTERNAL : SIR_RESOURCE_SOURCE_EMBEDDED;
		if ( catalog.addResource( identity.nCanonicalId, identity.nTypeId, identity.nPathAtom, eSource ) != SIR_RESOURCE_CATALOG_OK ) {
			catalog.shutdown();
			return SIR_EDITOR_CATALOG_FAILED;
		}
	}
	for ( uint32_t iReference = 0; iReference < request.nReferenceCount; ++iReference ) {
		const sirEditorResourceReference_t& reference = request.pReferences[iReference];
		if ( reference.nOwnerKey == 0 || sirEditorCatalogRepeatedEdge( request, iReference ) ) {
			continue;
		}
		const uint32_t iOwner = sirEditorCatalogFindOwner( index, reference.nOwnerKey );
		const uint64_t nOwnerId = request.pIdentities[iOwner].nCanonicalId;
		const uint64_t nTargetId = request.pIdentities[reference.iTargetResource].nCanonicalId;
		const sirResourceCatalogResult_t eResult = catalog.addDependency( nOwnerId, nTargetId );
		if ( eResult != SIR_RESOURCE_CATALOG_OK ) {
			catalog.shutdown();
			return eResult == SIR_RESOURCE_CATALOG_CYCLIC ? SIR_EDITOR_CATALOG_CYCLIC : SIR_EDITOR_CATALOG_FAILED;
		}
	}
	if ( catalog.seal() != SIR_RESOURCE_CATALOG_OK ) {
		catalog.shutdown();
		return SIR_EDITOR_CATALOG_FAILED;
	}
	return SIR_EDITOR_CATALOG_OK;
}
