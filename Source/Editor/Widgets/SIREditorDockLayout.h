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
#ifndef __SIR_EDITORDOCKLAYOUT_H__
#define __SIR_EDITORDOCKLAYOUT_H__
#include <Core/SIRPCH.h>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on
#include <UI/SIRControlTree.h>

enum class SIREditorDockResult : uint8_t {
	SIR_EDITOR_DOCK_OK = 0,
	SIR_EDITOR_DOCK_INVALID_ARGUMENT = 1,
	SIR_EDITOR_DOCK_NOT_INITIALIZED = 2,
	SIR_EDITOR_DOCK_OUT_OF_CAPACITY = 3,
	SIR_EDITOR_DOCK_INVALID_STRUCTURE = 4,
	SIR_EDITOR_DOCK_TOO_SMALL = 5
};

enum class SIREditorDockKind : uint8_t {
	SIR_EDITOR_DOCK_PANEL = 0,
	SIR_EDITOR_DOCK_SPLIT_HORIZONTAL = 1,
	SIR_EDITOR_DOCK_SPLIT_VERTICAL = 2
};

typedef struct sirEditorDockNode_s
{
	sirUIRect_t bounds;
	uint32_t iFirst;
	uint32_t iSecond;
	uint32_t nPanelId;
	uint16_t nSplitPermille;
	SIREditorDockKind eKind;
} sirEditorDockNode_t;

// Caller-owned binary split tree, deterministic integer layout, no allocations.
// Split axes divide available width or height; panel IDs identify presenters.
class SIREditorDockLayout
{
public:
	SIREditorDockLayout( void ) noexcept;

	uint32_t getCount( void ) const noexcept;
	SIREditorDockResult initialize( sirEditorDockNode_t *pNodes, uint32_t *pStack, uint32_t nCapacity ) noexcept;
	void shutdown( void ) noexcept;
	SIREditorDockResult createPanel( uint32_t nPanelId, uint32_t *pOutIndex ) noexcept;
	SIREditorDockResult split( uint32_t iPanel, SIREditorDockKind eAxis, uint16_t nPermille, uint32_t nPanelId ) noexcept;
	SIREditorDockResult layout( const sirUIRect_t& viewport, uint32_t nMinPanelExtent ) noexcept;
	SIREditorDockResult getPanelBounds( uint32_t nPanelId, sirUIRect_t *pOutBounds ) const noexcept;

protected:
private:
	sirEditorDockNode_t *m_pNodes;
	uint32_t *m_pStack;
	uint32_t m_nCapacity;
	uint32_t m_nCount;
};
#endif // __SIR_EDITORDOCKLAYOUT_H__
