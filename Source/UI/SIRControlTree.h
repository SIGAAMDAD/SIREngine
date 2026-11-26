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

#ifndef __SIR_CONTROL_TREE_H__
#define __SIR_CONTROL_TREE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class SIRUIResult : uint8_t {
	SIR_UI_OK = 0,
	SIR_UI_INVALID_ARGUMENT = 1,
	SIR_UI_NOT_INITIALIZED = 2,
	SIR_UI_STALE_HANDLE = 3,
	SIR_UI_OUT_OF_CAPACITY = 4,
	SIR_UI_INVALID_LAYOUT = 5
};

enum class SIRUILayout : uint8_t {
	SIR_UI_LAYOUT_ABSOLUTE = 0,
	SIR_UI_LAYOUT_VERTICAL = 1
};

enum class SIRUIInputType : uint8_t {
	SIR_UI_INPUT_POINTER_DOWN = 1,
	SIR_UI_INPUT_POINTER_MOVE = 2,
	SIR_UI_INPUT_KEY_DOWN = 3
};

typedef struct sirUIRect_s
{
	int32_t iX;
	int32_t iY;
	uint32_t nWidth;
	uint32_t nHeight;
} sirUIRect_t;

typedef struct sirUIHandle_s
{
	uint32_t iSlot;
	uint32_t nGeneration;
} sirUIHandle_t;

typedef struct sirUIInput_s
{
	SIRUIInputType eType;
	int32_t iX;
	int32_t iY;
	uint32_t nKey;
} sirUIInput_t;

typedef struct sirUIControl_s
{
	sirUIRect_t requested;
	sirUIRect_t bounds;
	sirUIRect_t clip;
	uint32_t iParent;
	uint32_t iFirstChild;
	uint32_t iLastChild;
	uint32_t iNextSibling;
	uint32_t nGeneration;
	SIRUILayout eLayout;
	bool bLive;
	bool bVisible;
	bool bFocusable;
	bool bClipChildren;
} sirUIControl_t;

// Retained first-party UI tree. Handles never expose Godot Control or Node;
// screen-space rectangles are layout results, not a second math library.
// The caller owns every slot and the DFS scratch array.
class SIRControlTree
{
private:
	sirUIControl_t *m_pControls = nullptr;
	uint32_t *m_pStack = nullptr;
	uint32_t m_nCapacity = 0;
	uint32_t m_nCount = 0;
	sirUIHandle_t m_Focus {};
	uint64_t m_nLayoutEpoch = 0;
	uint64_t m_nPaintEpoch = 0;

	bool isValid( const sirUIHandle_t& handle ) const noexcept;
	static bool contains( const sirUIRect_t& bounds, int32_t iX, int32_t iY ) noexcept;
	static sirUIRect_t intersection( const sirUIRect_t& a, const sirUIRect_t& b ) noexcept;

public:
	SIRUIResult initialize( sirUIControl_t *pControls, uint32_t *pStack, uint32_t nCapacity ) noexcept;
	void shutdown( void ) noexcept;
	SIRUIResult create( const sirUIHandle_t *pParent, const sirUIRect_t& requested, SIRUILayout eLayout, bool bFocusable,
	                      bool bClipChildren, sirUIHandle_t *pOutHandle ) noexcept;
	SIRUIResult remove( sirUIHandle_t handle ) noexcept;
	SIRUIResult setVisible( sirUIHandle_t handle, bool bVisible ) noexcept;
	SIRUIResult setRequestedRect( sirUIHandle_t handle, const sirUIRect_t& requested ) noexcept;
	SIRUIResult layout( const sirUIRect_t& viewport ) noexcept;
	SIRUIResult hitTest( int32_t iX, int32_t iY, sirUIHandle_t *pOutHandle ) noexcept;
	SIRUIResult dispatch( const sirUIInput_t& input, sirUIHandle_t *pTarget ) noexcept;
	SIRUIResult getBounds( sirUIHandle_t handle, sirUIRect_t *pOutBounds ) const noexcept;
	sirUIHandle_t getFocus( void ) const noexcept;
	uint64_t getLayoutEpoch( void ) const noexcept;
	uint64_t getPaintEpoch( void ) const noexcept;
	uint32_t getCount( void ) const noexcept;
};

#endif // __SIR_CONTROL_TREE_H__
