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

#include <Editor/Bridge/SIREditorNativeBridge.h>
#include <Editor/Host/SIREditorSceneWorkspace.h>

#include <Core/Memory/SIRMemorySegment.h>

#include <new>
#include <string.h>

typedef struct sirEditorNativeBridgeHandle_s
{
	SIREditorSceneWorkspace workspace;
	uint64_t nGeneration = 1;
} sirEditorNativeBridgeHandle_t;

static void sirEditorBridgeBumpGeneration( sirEditorNativeBridgeHandle_t *pHandle ) noexcept
{
	if ( pHandle->nGeneration != UINT64_MAX ) {
		++pHandle->nGeneration;
	} else {
		pHandle->nGeneration = 1;
	}
}

static sirEditorBridgeResult_t sirEditorBridgeCheckRead( const sirEditorNativeBridgeHandle_t *pHandle,
                                                         uint64_t nExpectedGeneration ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( nExpectedGeneration != pHandle->nGeneration ) {
		return SIR_EDITOR_BRIDGE_STALE;
	}
	return pHandle->workspace.isOpen() ? SIR_EDITOR_BRIDGE_OK : SIR_EDITOR_BRIDGE_NOT_OPEN;
}

static sirEditorBridgeResult_t sirEditorBridgeWorkspaceError( sirEditorWorkspaceResult_t eWorkspace ) noexcept
{
	if ( eWorkspace == SIR_EDITOR_WORKSPACE_OK ) {
		return SIR_EDITOR_BRIDGE_OK;
	}
	if ( eWorkspace == SIR_EDITOR_WORKSPACE_ALREADY_OPEN ) {
		return SIR_EDITOR_BRIDGE_ALREADY_OPEN;
	}
	if ( eWorkspace == SIR_EDITOR_WORKSPACE_NOT_INITIALIZED ) {
		return SIR_EDITOR_BRIDGE_UNINITIALIZED;
	}
	if ( eWorkspace == SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( eWorkspace == SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY ) {
		return SIR_EDITOR_BRIDGE_MEMORY_FAILURE;
	}
	return SIR_EDITOR_BRIDGE_SCENE_FAILURE;
}

size_t sirEditorNativeBridgeStorageBytes( void ) noexcept
{
	return sizeof( sirEditorNativeBridgeHandle_t );
}

size_t sirEditorNativeBridgeStorageAlignment( void ) noexcept
{
	return alignof( sirEditorNativeBridgeHandle_t );
}

sirEditorBridgeResult_t sirEditorNativeBridgeInit( void *pStorage, size_t nStorageBytes, void *pEditorSegment,
                                                   sirEditorNativeBridgeHandle_t **ppOutHandle ) noexcept
{
	if ( ppOutHandle == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	*ppOutHandle = nullptr;
	if ( pStorage == nullptr || pEditorSegment == nullptr || nStorageBytes < sizeof( sirEditorNativeBridgeHandle_t ) ||
	     ( ( uintptr_t )pStorage % alignof( sirEditorNativeBridgeHandle_t ) ) != 0 ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}

	SIRMemorySegment *pSegment = ( SIRMemorySegment * )pEditorSegment;
	if ( pSegment->getDomain() != SIR_MEMORY_DOMAIN_EDITOR ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}

	sirEditorNativeBridgeHandle_t *pHandle = new ( pStorage ) sirEditorNativeBridgeHandle_t();
	if ( !pHandle->workspace.initialize( pSegment ) ) {
		pHandle->~sirEditorNativeBridgeHandle_t();
		return SIR_EDITOR_BRIDGE_MEMORY_FAILURE;
	}

	*ppOutHandle = pHandle;
	return SIR_EDITOR_BRIDGE_OK;
}

void sirEditorNativeBridgeDestroy( sirEditorNativeBridgeHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return;
	}
	pHandle->workspace.shutdown();
	pHandle->~sirEditorNativeBridgeHandle_t();
}

sirEditorBridgeResult_t sirEditorNativeBridgeOpenText( sirEditorNativeBridgeHandle_t *pHandle, const char *pszSource,
                                                       size_t nBytes ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	const sirEditorWorkspaceResult_t eResult = pHandle->workspace.openTextSource( pszSource, nBytes );
	if ( eResult == SIR_EDITOR_WORKSPACE_OK ) {
		sirEditorBridgeBumpGeneration( pHandle );
	}
	return sirEditorBridgeWorkspaceError( eResult );
}

sirEditorBridgeResult_t sirEditorNativeBridgeOpenFile( sirEditorNativeBridgeHandle_t *pHandle, const char *pszPath ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	const sirEditorWorkspaceResult_t eResult = pHandle->workspace.openTextFile( pszPath );
	if ( eResult == SIR_EDITOR_WORKSPACE_OK ) {
		sirEditorBridgeBumpGeneration( pHandle );
	}
	return sirEditorBridgeWorkspaceError( eResult );
}

void sirEditorNativeBridgeClose( sirEditorNativeBridgeHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return;
	}
	pHandle->workspace.close();
	sirEditorBridgeBumpGeneration( pHandle );
}

sirEditorBridgeResult_t sirEditorNativeBridgeSnapshot( const sirEditorNativeBridgeHandle_t *pHandle,
                                                       sirEditorBridgeSnapshot_t *pOutSnapshot ) noexcept
{
	if ( pHandle == nullptr || pOutSnapshot == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( !pHandle->workspace.isOpen() ) {
		return SIR_EDITOR_BRIDGE_NOT_OPEN;
	}

	const SIREditorSceneWorkspace &workspace = pHandle->workspace;
	const sirEditorBridgeSnapshot_t snapshot = { SIR_EDITOR_BRIDGE_ABI_VERSION,
	                                             workspace.getSceneIndex().nNodeCount,
	                                             workspace.getResourceIndex().nCount,
	                                             workspace.getPropertyIndex().nCount,
	                                             pHandle->nGeneration,
	                                             workspace.getDocument().getRevision() };
	*pOutSnapshot = snapshot;
	return SIR_EDITOR_BRIDGE_OK;
}

sirEditorBridgeResult_t sirEditorNativeBridgeNodeAt( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                     uint32_t iNode, sirEditorBridgeNode_t *pOutNode ) noexcept
{
	if ( pOutNode == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	const sirEditorBridgeResult_t eRead = sirEditorBridgeCheckRead( pHandle, nExpectedGeneration );
	if ( eRead != SIR_EDITOR_BRIDGE_OK ) {
		return eRead;
	}
	const sirEditorTextSceneIndex_t &index = pHandle->workspace.getSceneIndex();
	if ( iNode >= index.nNodeCount ) {
		return SIR_EDITOR_BRIDGE_NOT_FOUND;
	}

	const sirEditorTextSceneNode_t &node = index.storage.pNodes[iNode];
	*pOutNode = { node.node.nId, node.node.nParentId, node.nSourceLine, node.nGodotUniqueId, node.nNameLength, node.nPathLength };
	return SIR_EDITOR_BRIDGE_OK;
}

sirEditorBridgeResult_t sirEditorNativeBridgeResourceAt( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                         uint32_t iResource, sirEditorBridgeResource_t *pOutResource ) noexcept
{
	if ( pOutResource == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	const sirEditorBridgeResult_t eRead = sirEditorBridgeCheckRead( pHandle, nExpectedGeneration );
	if ( eRead != SIR_EDITOR_BRIDGE_OK ) {
		return eRead;
	}
	const sirEditorTextResourceIndex_t &index = pHandle->workspace.getResourceIndex();
	if ( iResource >= index.nCount ) {
		return SIR_EDITOR_BRIDGE_NOT_FOUND;
	}

	const sirEditorTextResource_t &resource = index.pResources[iResource];
	*pOutResource = { resource.nStableKey,  resource.nSourceLine, ( uint32_t )resource.eKind, resource.nIdLength,
	                  resource.nTypeLength, resource.nPathLength, resource.nUidLength };
	return SIR_EDITOR_BRIDGE_OK;
}

sirEditorBridgeResult_t sirEditorNativeBridgePropertyAt( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                         uint32_t iProperty, sirEditorBridgeProperty_t *pOutProperty ) noexcept
{
	if ( pOutProperty == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	const sirEditorBridgeResult_t eRead = sirEditorBridgeCheckRead( pHandle, nExpectedGeneration );
	if ( eRead != SIR_EDITOR_BRIDGE_OK ) {
		return eRead;
	}
	const sirEditorTextPropertyIndex_t &index = pHandle->workspace.getPropertyIndex();
	if ( iProperty >= index.nCount ) {
		return SIR_EDITOR_BRIDGE_NOT_FOUND;
	}

	const sirEditorTextProperty_t &property = index.pProperties[iProperty];
	*pOutProperty = { property.nNodeId, property.nSourceLine, property.nNameLength, property.nValueLength };
	return SIR_EDITOR_BRIDGE_OK;
}

static sirEditorBridgeResult_t sirEditorBridgeFieldSpan( const SIREditorSceneWorkspace &workspace, sirEditorBridgeStringField_t eField,
                                                         uint32_t iRecord, const char **ppText, size_t *pLength ) noexcept
{
	const char *pszSource = workspace.getSource();
	size_t nSourceBytes = workspace.getSourceBytes();
	uint32_t nOffset = 0;
	uint32_t nLength = 0;
	if ( eField == SIR_EDITOR_BRIDGE_NODE_NAME || eField == SIR_EDITOR_BRIDGE_NODE_PATH ) {
		const sirEditorTextSceneIndex_t &index = workspace.getSceneIndex();
		if ( iRecord >= index.nNodeCount ) {
			return SIR_EDITOR_BRIDGE_NOT_FOUND;
		}
		const sirEditorTextSceneNode_t &node = index.storage.pNodes[iRecord];
		if ( eField == SIR_EDITOR_BRIDGE_NODE_PATH ) {
			pszSource = index.storage.pszPaths;
			nSourceBytes = index.nPathBytes;
			nOffset = node.nPathOffset;
			nLength = node.nPathLength;
		} else {
			nOffset = node.nNameOffset;
			nLength = node.nNameLength;
		}
	} else if ( eField >= SIR_EDITOR_BRIDGE_RESOURCE_ID && eField <= SIR_EDITOR_BRIDGE_RESOURCE_UID ) {
		const sirEditorTextResourceIndex_t &index = workspace.getResourceIndex();
		if ( iRecord >= index.nCount ) {
			return SIR_EDITOR_BRIDGE_NOT_FOUND;
		}
		const sirEditorTextResource_t &resource = index.pResources[iRecord];
		switch ( eField ) {
		case SIR_EDITOR_BRIDGE_RESOURCE_ID:
			nOffset = resource.nIdOffset;
			nLength = resource.nIdLength;
			break;
		case SIR_EDITOR_BRIDGE_RESOURCE_TYPE:
			nOffset = resource.nTypeOffset;
			nLength = resource.nTypeLength;
			break;
		case SIR_EDITOR_BRIDGE_RESOURCE_PATH:
			nOffset = resource.nPathOffset;
			nLength = resource.nPathLength;
			break;
		case SIR_EDITOR_BRIDGE_RESOURCE_UID:
			nOffset = resource.nUidOffset;
			nLength = resource.nUidLength;
			break;
		default:
			return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
		}
	} else if ( eField == SIR_EDITOR_BRIDGE_PROPERTY_NAME || eField == SIR_EDITOR_BRIDGE_PROPERTY_VALUE ) {
		const sirEditorTextPropertyIndex_t &index = workspace.getPropertyIndex();
		if ( iRecord >= index.nCount ) {
			return SIR_EDITOR_BRIDGE_NOT_FOUND;
		}
		const sirEditorTextProperty_t &property = index.pProperties[iRecord];
		nOffset = eField == SIR_EDITOR_BRIDGE_PROPERTY_NAME ? property.nNameOffset : property.nValueOffset;
		nLength = eField == SIR_EDITOR_BRIDGE_PROPERTY_NAME ? property.nNameLength : property.nValueLength;
	} else {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( pszSource == nullptr || nOffset > nSourceBytes || nLength > nSourceBytes - nOffset ) {
		return SIR_EDITOR_BRIDGE_SCENE_FAILURE;
	}
	*ppText = pszSource + nOffset;
	*pLength = ( size_t )nLength;
	return SIR_EDITOR_BRIDGE_OK;
}

sirEditorBridgeResult_t sirEditorNativeBridgeCopyField( const sirEditorNativeBridgeHandle_t *pHandle, uint64_t nExpectedGeneration,
                                                        sirEditorBridgeStringField_t eField, uint32_t iRecord, char *pszOutput,
                                                        size_t nCapacity, size_t *pRequiredBytes ) noexcept
{
	if ( pRequiredBytes == nullptr ) {
		return SIR_EDITOR_BRIDGE_INVALID_ARGUMENT;
	}
	*pRequiredBytes = 0;
	const sirEditorBridgeResult_t eRead = sirEditorBridgeCheckRead( pHandle, nExpectedGeneration );
	if ( eRead != SIR_EDITOR_BRIDGE_OK ) {
		return eRead;
	}
	const char *pValue = nullptr;
	size_t nLength = 0;
	const sirEditorBridgeResult_t eSpan = sirEditorBridgeFieldSpan( pHandle->workspace, eField, iRecord, &pValue, &nLength );
	if ( eSpan != SIR_EDITOR_BRIDGE_OK ) {
		return eSpan;
	}
	if ( nLength == SIZE_MAX ) {
		return SIR_EDITOR_BRIDGE_OUT_OF_CAPACITY;
	}
	*pRequiredBytes = nLength + 1;
	if ( pszOutput == nullptr || nCapacity < *pRequiredBytes ) {
		return SIR_EDITOR_BRIDGE_OUT_OF_CAPACITY;
	}
	memcpy( pszOutput, pValue, nLength );
	pszOutput[nLength] = '\0';
	return SIR_EDITOR_BRIDGE_OK;
}
