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

#ifndef __SIRENGINE_SIREDITORVIRTUALLIST_H__
#define __SIRENGINE_SIREDITORVIRTUALLIST_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

typedef enum sirEditorVirtualListResult_e : uint8_t
{
	SIR_EDITOR_LIST_OK = 0,
	SIR_EDITOR_LIST_INVALID_ARGUMENT = 1,
	SIR_EDITOR_LIST_OUT_OF_CAPACITY = 2,
	SIR_EDITOR_LIST_NOT_INITIALIZED = 3
} sirEditorVirtualListResult_t;

typedef struct sirEditorVirtualRow_s
{
	uint64_t nId;
	uint32_t nHeight;
	uint32_t nFlags;
} sirEditorVirtualRow_t;

typedef struct sirEditorVirtualRange_s
{
	uint32_t iBegin;
	uint32_t iEnd;
} sirEditorVirtualRange_t;

// Fenwick-indexed list model for very large scene trees and FileSystem views.
// Caller owns both buffers and the editor-only memory backing them.
class SIREditorVirtualList
{
private:
	sirEditorVirtualRow_t *m_pRows;
	uint64_t *m_pFenwick;
	uint32_t m_nCapacity;
	uint32_t m_nSize;
	uint64_t m_nTotalHeight;

	uint64_t prefixHeight( uint32_t nCount ) const noexcept;
	uint32_t lowerBound( uint64_t nOffset ) const noexcept;

public:
	SIREditorVirtualList( void ) noexcept;
	sirEditorVirtualListResult_t initialize( sirEditorVirtualRow_t *pRows, uint64_t *pFenwick, uint32_t nCapacity ) noexcept;
	void shutdown( void ) noexcept;
	sirEditorVirtualListResult_t setRows( const sirEditorVirtualRow_t *pRows, uint32_t nCount ) noexcept;
	sirEditorVirtualListResult_t setHeight( uint32_t iRow, uint32_t nHeight ) noexcept;
	sirEditorVirtualListResult_t getRowOffset( uint32_t iRow, uint64_t *pOutOffset ) const noexcept;
	sirEditorVirtualListResult_t getVisibleRange( uint64_t nScroll, uint64_t nViewportHeight, uint64_t nOverscan,
	                                              sirEditorVirtualRange_t *pOutRange ) const noexcept;
	uint32_t getSize( void ) const noexcept;
	uint64_t getTotalHeight( void ) const noexcept;
};

#endif // __SIRENGINE_SIREDITORVIRTUALLIST_H__
