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

#ifndef __SIR_RESOURCEREFERENCE_H__
#define __SIR_RESOURCEREFERENCE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceGraph.h>

// Implemented by the canonical GRDG registry.
// Acquire is linearizable against writer retirement and checks generation/kind.
// A held strong reference cannot be invalidated by ordinary retirement.
// Release is noexcept and only schedules writer-owned logical retirement.
// These operations do not allocate per-reference control blocks.
SIRResourceResult SIRResourceAcquireReference( SIRResourceGraph& rGraph, SIRResourceHandle hResource, SIRResourceKind eKind ) noexcept;
void SIRResourceReleaseReference( SIRResourceGraph& rGraph, SIRResourceHandle hResource ) noexcept;
bool SIRResourceIsReferenceAlive( const SIRResourceGraph& rGraph, SIRResourceHandle hResource ) noexcept;

#endif // __SIR_RESOURCEREFERENCE_H__
