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

#ifndef __SIRENGINE_SIREDITORSCENEDOCUMENT_H__
#define __SIRENGINE_SIREDITORSCENEDOCUMENT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirEditorResult_e : uint8_t
{
	SIR_EDITOR_OK = 0,
	SIR_EDITOR_INVALID_ARGUMENT = 1,
	SIR_EDITOR_NOT_INITIALIZED = 2,
	SIR_EDITOR_TRANSACTION_ACTIVE = 3,
	SIR_EDITOR_NO_TRANSACTION = 4,
	SIR_EDITOR_STALE_REVISION = 5,
	SIR_EDITOR_OUT_OF_CAPACITY = 6,
	SIR_EDITOR_NOT_FOUND = 7,
	SIR_EDITOR_INVALID_HIERARCHY = 8,
	SIR_EDITOR_INVALID_ID = 9,
	SIR_EDITOR_NO_HISTORY = 10,
	SIR_EDITOR_REVISION_EXHAUSTED = 11
} sirEditorResult_t;

typedef struct sirEditorNode_s
{
	uint64_t nId;
	uint64_t nParentId;
	uint32_t nTypeId;
	uint32_t nFlags;
	uint32_t nGeneration;
	uint32_t nReserved;
} sirEditorNode_t;

typedef enum sirEditorMutationType_e : uint8_t
{
	SIR_EDITOR_MUTATION_UPSERT = 1,
	SIR_EDITOR_MUTATION_REMOVE = 2
} sirEditorMutationType_t;

typedef struct sirEditorMutation_s
{
	sirEditorNode_t node;
	sirEditorMutationType_t eType;
} sirEditorMutation_t;

typedef struct sirEditorDelta_s
{
	sirEditorNode_t previous;
	sirEditorNode_t next;
	uint32_t iSlot;
} sirEditorDelta_t;

typedef struct sirEditorHistoryFrame_s
{
	uint32_t iFirstDelta;
	uint32_t nDeltas;
} sirEditorHistoryFrame_t;

typedef struct sirEditorIndexEntry_s
{
	uint64_t nId;
	uint32_t iSlot;
	uint32_t nReserved;
} sirEditorIndexEntry_t;

// All backing storage is supplied by the editor's SIR_MEMORY_DOMAIN_EDITOR allocator.
// No allocations, hidden heap fallbacks or threads are created by this component.
typedef struct sirEditorDocumentStorage_s
{
	sirEditorNode_t *pNodes;
	sirEditorNode_t *pStaging;
	sirEditorMutation_t *pPending;
	sirEditorDelta_t *pPendingDeltas;
	sirEditorDelta_t *pHistoryDeltas;
	sirEditorHistoryFrame_t *pHistoryFrames;
	sirEditorIndexEntry_t *pIndex;
	uint32_t nNodeCapacity;
	uint32_t nPendingCapacity;
	uint32_t nDeltaCapacity;
	uint32_t nFrameCapacity;
	uint32_t nIndexCapacity;
} sirEditorDocumentStorage_t;

// Single authoring-thread writer. Never borrow pNodes across threads; use copySnapshot.
// Each commit validates the complete candidate hierarchy before changing live state.
class SIREditorSceneDocument
{
private:
	sirEditorDocumentStorage_t m_Storage;
	uint64_t m_nRevision;
	uint32_t m_nPendingCount;
	uint32_t m_nHistoryCount;
	uint32_t m_nHistoryCursor;
	uint32_t m_nHistoryDeltaCount;
	bool m_bInitialized;
	bool m_bEditing;

	uint32_t findSlot( const sirEditorNode_t *pNodes, uint64_t nId ) const noexcept;
	sirEditorResult_t applyPending( uint32_t iMutation, sirEditorDelta_t *pDelta ) noexcept;
	sirEditorResult_t buildValidatedIndex( const sirEditorNode_t *pNodes ) noexcept;
	uint32_t indexedSlot( uint64_t nId ) const noexcept;
	sirEditorResult_t applyHistoryFrame( bool bUndo ) noexcept;

public:
	SIREditorSceneDocument( void ) noexcept;
	sirEditorResult_t initialize( const sirEditorDocumentStorage_t &storage ) noexcept;
	void shutdown( void ) noexcept;
	sirEditorResult_t beginEdit( uint64_t nExpectedRevision ) noexcept;
	sirEditorResult_t queueUpsert( const sirEditorNode_t &node ) noexcept;
	sirEditorResult_t queueRemove( uint64_t nId ) noexcept;
	sirEditorResult_t commitEdit( void ) noexcept;
	sirEditorResult_t cancelEdit( void ) noexcept;
	sirEditorResult_t undo( void ) noexcept;
	sirEditorResult_t redo( void ) noexcept;
	sirEditorResult_t findNode( uint64_t nId, sirEditorNode_t *pOutNode ) const noexcept;
	sirEditorResult_t copySnapshot( sirEditorNode_t *pOutNodes, uint32_t nCapacity, uint32_t *pOutCount,
	                                uint64_t *pOutRevision ) const noexcept;
	uint64_t getRevision( void ) const noexcept;
	bool canUndo( void ) const noexcept;
	bool canRedo( void ) const noexcept;
};

#endif // __SIRENGINE_SIREDITORSCENEDOCUMENT_H__
