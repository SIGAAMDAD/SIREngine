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
#ifndef __SIRENGINE_SIREDITORWORKFLOW_H__
#define __SIRENGINE_SIREDITORWORKFLOW_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on
#include <Editor/Widgets/SIREditorHierarchyModel.h>

// One main-thread action layer for hierarchy and inspector commands. All
// authored mutations pass through the existing atomic SceneDocument journal.
class SIREditorWorkflow
{
public:
	SIREditorWorkflow( void ) noexcept;

	uint64_t getSelectedId( void ) const noexcept;
	uint64_t getLastKnownRevision( void ) const noexcept;

	sirEditorResult_t initialize( SIREditorSceneDocument *pDocument, SIREditorHierarchyModel *pHierarchy ) noexcept;
	void shutdown( void ) noexcept;
	sirEditorResult_t select( uint64_t nNodeId ) noexcept;
	sirEditorResult_t createNode( const sirEditorNode_t& node ) noexcept;
	sirEditorResult_t removeSelected( void ) noexcept;
	sirEditorResult_t setSelectedProperty( uint32_t nPropertyId, const sirPropertyValue_t& value ) noexcept;
	sirEditorResult_t removeSelectedProperty( uint32_t nPropertyId ) noexcept;
	sirEditorResult_t undo( void ) noexcept;
	sirEditorResult_t redo( void ) noexcept;
	sirEditorResult_t synchronize( void ) noexcept;

protected:
private:
	SIREditorSceneDocument *m_pDocument;
	SIREditorHierarchyModel *m_pHierarchy;
	uint64_t m_nSelectedId;
	uint64_t m_nRevision;

	sirEditorResult_t commit( void ) noexcept;
};
#endif // __SIRENGINE_SIREDITORWORKFLOW_H__
