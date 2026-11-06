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

#ifndef __SIRENGINE_SIREDITORRESOURCEREFERENCES_H__
#define __SIRENGINE_SIREDITORRESOURCEREFERENCES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Editor/Scene/SIREditorTextScene.h>

// Every actual reference occurrence in a text scene. A zero owner indicates
// a node/property reference; a nonzero owner is an embedded resource's stable
// key and describes a directed dependency edge to nTargetKey.
typedef struct sirEditorResourceReference_s
{
	uint64_t nOwnerKey;
	uint64_t nTargetKey;
	uint32_t iTargetResource;
	uint32_t nCallOffset;
	uint32_t nSourceLine;
} sirEditorResourceReference_t;

typedef struct sirEditorResourceReferenceScan_s
{
	const char *pszSource;
	size_t nSourceBytes;
	const sirEditorTextResourceIndex_t *pIndex;
	sirEditorResourceReference_t *pOutput;
	uint32_t nCapacity;
} sirEditorResourceReferenceScan_t;

// Requires the immutable source and index returned by sirEditorScanTextResources.
// A null output/capacity zero measures the number of references. Insufficient
// capacity or malformed input never writes a partial output array. References
// retain source offsets; neither file I/O nor resource allocations occur here.
sirEditorTextResourceResult_t sirEditorScanResourceReferences( const sirEditorResourceReferenceScan_t &context,
                                                               uint32_t *pOutCount ) noexcept;

#endif // __SIRENGINE_SIREDITORRESOURCEREFERENCES_H__
