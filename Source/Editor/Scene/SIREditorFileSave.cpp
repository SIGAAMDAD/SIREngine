/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Editor/Scene/SIREditorFileSave.h>
#include <Core/Platform/Shared/File/SIRPlatformFileSystem.h>

static SIREditorFileSaveResult sirEditorMapFileResult( sirPlatformAtomicResult eResult ) noexcept
{
	switch ( eResult ) {
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_OK:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_OK;
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_INVALID_ARGUMENT:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_INVALID_PATH:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_INVALID_PATH;
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_TEMP_EXISTS:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_TEMP_EXISTS;
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_IO_ERROR;
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_REPLACE_FAILED:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_REPLACE_FAILED;
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_COMMITTED_NOT_DURABLE:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_COMMITTED_NOT_DURABLE;
	case sirPlatformAtomicResult::SIR_ATOMIC_SAVE_CONFLICT:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_CONFLICT;
	default:
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
}

SIREditorFileSaveResult sirEditorWriteFileAtomically( const sirEditorFileWriteRequest_t& request ) noexcept
{
	static constexpr size_t MAX_EDITOR_SAVE_BYTES = 32ULL * 1024ULL * 1024ULL;
	if ( request.nBytes > MAX_EDITOR_SAVE_BYTES || request.nExpectedBytes > MAX_EDITOR_SAVE_BYTES ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	sirFileSystemAtomicWriteRequest_t platform {};
	platform.pszDestination = request.pszDestination;
	platform.pszTemporary = request.pszTemporary;
	platform.pBytes = request.pBytes;
	platform.nBytes = request.nBytes;
	platform.pExpectedBytes = request.pExpectedBytes;
	platform.nExpectedBytes = request.nExpectedBytes;
	return sirEditorMapFileResult( SIRPlatformFileSystem::writeAtomically( platform ) );
}

SIREditorFileSaveResult sirEditorSaveBoundScene( const sirEditorSceneSaveRequest_t& request ) noexcept
{
	if ( request.pBridge == nullptr || request.pSourceScene == nullptr || request.pszScratch == nullptr || request.nScratchCapacity == 0 ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	const sirEditorTextSceneIndex_t& source = *request.pSourceScene;
	if ( request.pBridge->pDocument == nullptr || source.storage.pNodes == nullptr || source.nNodeCount == 0 ||
	     source.nNodeCount > source.storage.nNodeCapacity ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	uint32_t nAuthoredNodes = 0;
	uint64_t nRevision = 0;
	const SIREditorResult eCount = request.pBridge->pDocument->copySnapshot( nullptr, 0, &nAuthoredNodes, &nRevision );
	if ( eCount != SIREditorResult::SIR_EDITOR_OUT_OF_CAPACITY && eCount != SIREditorResult::SIR_EDITOR_OK ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	if ( nAuthoredNodes != source.nNodeCount ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_UNSUPPORTED_EDIT;
	}
	for ( uint32_t iNode = 0; iNode < source.nNodeCount; ++iNode ) {
		const sirEditorNode_t& original = source.storage.pNodes[iNode].node;
		sirEditorNode_t current {};
		if ( request.pBridge->pDocument->findNode( original.nId, &current ) != SIREditorResult::SIR_EDITOR_OK || current.nId != original.nId ||
		     current.nTypeId != original.nTypeId || current.nParentId != original.nParentId || current.nFlags != original.nFlags ||
		     original.nGeneration == UINT32_MAX || current.nGeneration != original.nGeneration + 1 ) {
			return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_UNSUPPORTED_EDIT;
		}
	}
	// Only authored properties represented by this scalar binding table
	// can be persisted. Added/unbound properties require the full writer.
	uint32_t nAuthoredProperties = 0;
	const SIREditorResult eProperties = request.pBridge->pDocument->copyPropertySnapshot( nullptr, 0, &nAuthoredProperties );
	if ( eProperties != SIREditorResult::SIR_EDITOR_OUT_OF_CAPACITY && eProperties != SIREditorResult::SIR_EDITOR_OK ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	if ( nAuthoredProperties != request.pBridge->nBindingCount ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_UNSUPPORTED_EDIT;
	}
	size_t nOutputBytes = 0;
	if ( sirEditorExportBoundScalars( *request.pBridge, nullptr, 0, &nOutputBytes ) != SIREditorScalarBridgeResult::SIR_EDITOR_SCALAR_BRIDGE_OK ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	if ( nOutputBytes == 0 || request.nScratchCapacity <= nOutputBytes ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_OUT_OF_CAPACITY;
	}
	if ( sirEditorExportBoundScalars( *request.pBridge, request.pszScratch, request.nScratchCapacity, &nOutputBytes ) !=
	     SIREditorScalarBridgeResult::SIR_EDITOR_SCALAR_BRIDGE_OK ) {
		return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	sirEditorFileWriteRequest_t write = { request.pszDestination, request.pszTemporary, request.pszScratch, nOutputBytes };
	if ( request.bRejectExternalChanges ) {
		if ( request.pBridge->pszSource == nullptr || request.pBridge->nSourceBytes == 0 ) {
			return SIREditorFileSaveResult::SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
		}
		write.pExpectedBytes = request.pBridge->pszSource;
		write.nExpectedBytes = request.pBridge->nSourceBytes;
	}
	return sirEditorWriteFileAtomically( write );
}
