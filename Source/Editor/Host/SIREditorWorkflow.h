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

#ifndef __SIR_EDITORWORKFLOW_H__
#define __SIR_EDITORWORKFLOW_H__
#include <Core/SIRPCH.h>
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

	SIREditorResult initialize( SIREditorSceneDocument *pDocument, SIREditorHierarchyModel *pHierarchy ) noexcept;
	void shutdown( void ) noexcept;
	SIREditorResult select( uint64_t nNodeId ) noexcept;
	SIREditorResult createNode( const sirEditorNode_t& node ) noexcept;
	SIREditorResult removeSelected( void ) noexcept;
	SIREditorResult setSelectedProperty( uint32_t nPropertyId, const sirPropertyValue_t& value ) noexcept;
	SIREditorResult removeSelectedProperty( uint32_t nPropertyId ) noexcept;
	SIREditorResult undo( void ) noexcept;
	SIREditorResult redo( void ) noexcept;
	SIREditorResult synchronize( void ) noexcept;

protected:
private:
	SIREditorSceneDocument *m_pDocument;
	SIREditorHierarchyModel *m_pHierarchy;
	uint64_t m_nSelectedId;
	uint64_t m_nRevision;

	SIREditorResult commit( void ) noexcept;
};
#endif // __SIR_EDITORWORKFLOW_H__
