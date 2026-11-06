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

#ifndef __SIRENGINE_SIREDITORHIERARCHYMODEL_H__
#define __SIRENGINE_SIREDITORHIERARCHYMODEL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorSceneDocument.h>
#include <Editor/Scene/SIREditorTextScene.h>
#include <Editor/Widgets/SIREditorVirtualList.h>

typedef enum sirEditorHierarchyResult_e : uint8_t
{
	SIR_EDITOR_HIERARCHY_OK = 0,
	SIR_EDITOR_HIERARCHY_INVALID_ARGUMENT = 1,
	SIR_EDITOR_HIERARCHY_NOT_INITIALIZED = 2,
	SIR_EDITOR_HIERARCHY_OUT_OF_CAPACITY = 3,
	SIR_EDITOR_HIERARCHY_NOT_FOUND = 4,
	SIR_EDITOR_HIERARCHY_STALE_SOURCE = 5,
	SIR_EDITOR_HIERARCHY_INVALID_SOURCE = 6
} sirEditorHierarchyResult_t;

// Optional authoritative sibling order, sorted ascending by nId. When absent,
// SIREditorSceneDocument's deterministic ID ordering is used instead.
// Source-scene consumers can supply nOrder from nSourceLine to preserve authored order.
typedef struct sirEditorHierarchyOrder_s
{
	uint64_t nId;
	uint32_t nOrder;
	uint32_t nReserved;
} sirEditorHierarchyOrder_t;

typedef struct sirEditorHierarchyLink_s
{
	uint64_t nId;
	uint64_t nParentId;
	uint32_t nOrder;
	uint32_t nReserved;
} sirEditorHierarchyLink_t;

typedef struct sirEditorHierarchyState_s
{
	uint64_t nId;
	uint32_t nRowHeight;
	bool bExpanded;
} sirEditorHierarchyState_t;

typedef struct sirEditorHierarchyCursor_s
{
	uint32_t iNext;
	uint32_t iEnd;
	uint32_t nDepth;
} sirEditorHierarchyCursor_t;

typedef struct sirEditorHierarchyRow_s
{
	uint64_t nId;
	uint64_t nParentId;
	uint32_t nDepth;
	bool bHasChildren;
	bool bExpanded;
	bool bSelected;
} sirEditorHierarchyRow_t;

// All buffers are separately owned by the editor-only allocation domain. Every
// array except pFenwick has nCapacity elements; pFenwick has nCapacity + 1.
// No hidden allocations, recursion, Godot objects or mutable SceneTree access.
typedef struct sirEditorHierarchyStorage_s
{
	sirEditorNode_t *pSnapshot;
	sirEditorHierarchyLink_t *pLinks;
	sirEditorHierarchyState_t *pState;
	sirEditorHierarchyState_t *pNextState;
	sirEditorHierarchyCursor_t *pStack;
	sirEditorHierarchyRow_t *pVisible;
	sirEditorVirtualRow_t *pListRows;
	uint64_t *pFenwick;
	uint32_t nCapacity;
} sirEditorHierarchyStorage_t;

// Convert a validated Godot text-scene index into sibling order hints. Output
// is sorted by ID for binary lookup, while nOrder preserves the source line.
// Caller owns the output, and it must not alias the scanned node array.
sirEditorHierarchyResult_t sirEditorHierarchyOrderFromTextScene( const sirEditorTextSceneIndex_t &index, sirEditorHierarchyOrder_t *pOutput,
                                                                 uint32_t nCapacity, uint32_t *pOutCount ) noexcept;

// Editor-main-thread view model. Godot's Control/Tree adapter will consume rows;
// the native hierarchy never owns or creates UI Control objects.
class SIREditorHierarchyModel
{
private:
	sirEditorHierarchyStorage_t m_Storage;
	SIREditorVirtualList m_List;
	uint64_t m_nSourceRevision;
	uint64_t m_nLayoutEpoch;
	uint64_t m_nPaintEpoch;
	uint64_t m_nSelectedId;
	uint32_t m_nNodeCount;
	uint32_t m_nVisibleCount;
	uint32_t m_nDefaultRowHeight;
	bool m_bInitialized;

	uint32_t findState( uint64_t nId, const sirEditorHierarchyState_t *pStates, uint32_t nCount ) const noexcept;
	uint32_t findFirstChild( uint64_t nParentId ) const noexcept;
	uint32_t findAfterChildren( uint64_t nParentId ) const noexcept;
	bool hasChildren( uint64_t nId ) const noexcept;
	sirEditorHierarchyResult_t buildVisibleRows( void ) noexcept;
	uint32_t findVisibleRow( uint64_t nId ) const noexcept;

public:
	SIREditorHierarchyModel( void ) noexcept;
	sirEditorHierarchyResult_t initialize( const sirEditorHierarchyStorage_t &storage, uint32_t nDefaultRowHeight ) noexcept;
	void shutdown( void ) noexcept;
	sirEditorHierarchyResult_t rebuild( const SIREditorSceneDocument &document, const sirEditorHierarchyOrder_t *pOrder = nullptr,
	                                    uint32_t nOrderCount = 0 ) noexcept;
	bool isCurrent( const SIREditorSceneDocument &document ) const noexcept;
	sirEditorHierarchyResult_t setExpanded( const SIREditorSceneDocument &document, uint64_t nId, bool bExpanded ) noexcept;
	sirEditorHierarchyResult_t setSelected( const SIREditorSceneDocument &document, uint64_t nId ) noexcept;
	sirEditorHierarchyResult_t setRowHeight( const SIREditorSceneDocument &document, uint64_t nId, uint32_t nHeight ) noexcept;
	sirEditorHierarchyResult_t getVisibleRange( uint64_t nScroll, uint64_t nViewportHeight, uint64_t nOverscan,
	                                            sirEditorVirtualRange_t *pOutRange ) const noexcept;
	sirEditorHierarchyResult_t getVisibleRow( uint32_t iRow, sirEditorHierarchyRow_t *pOutRow ) const noexcept;
	uint32_t getVisibleCount( void ) const noexcept;
	uint64_t getSourceRevision( void ) const noexcept;
	uint64_t getLayoutEpoch( void ) const noexcept;
	uint64_t getPaintEpoch( void ) const noexcept;
	uint64_t getSelectedId( void ) const noexcept;
	uint64_t getTotalHeight( void ) const noexcept;
};

#endif // __SIRENGINE_SIREDITORHIERARCHYMODEL_H__
