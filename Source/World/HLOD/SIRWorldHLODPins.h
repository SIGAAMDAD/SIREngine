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

#ifndef __SIR_WORLDHLODPINS_H__
#define __SIR_WORLDHLODPINS_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <World/HLOD/SIRWorldHLODGRDG.h>

// The caller owns renderer-provider backing and the submission fence. These
// GRDG source pins retain immutable cooked geometry; they are not GPU leases.
// Revalidate provider-native lifetime before submitting GPU commands.
typedef struct sirWorldHLODPinInput_s
{
	sirWorldHLODGRDGInput_t snapshot;
	const uint32_t *pSelectedNodes;
	uint32_t nSelectedCount;
	uint32_t nProviderId;
} sirWorldHLODPinInput_t;

typedef struct sirWorldHLODPinOutput_s
{
	SIRResourcePinHandle *pPins;
	SIRResourcePinHandle *pScratchPins;
	uint32_t nCapacity;
	uint32_t *pnPinned;
} sirWorldHLODPinOutput_t;

enum class SIRWorldHLODPinResult : uint8_t {
	SIR_WORLD_HLOD_PIN_OK = 0,
	SIR_WORLD_HLOD_PIN_INVALID_ARGUMENT = 1,
	SIR_WORLD_HLOD_PIN_INVALID_CUT = 2,
	SIR_WORLD_HLOD_PIN_STALE_RESIDENCY = 3,
	SIR_WORLD_HLOD_PIN_CAPACITY = 4,
	SIR_WORLD_HLOD_PIN_PROVIDER_FAILURE = 5,
	SIR_WORLD_HLOD_PIN_ROLLBACK_FAILURE = 6
};

// Pins the selected, ascending unique indices produced by sirWorldHLODPlan
// against exact GRDG resource and provider revisions. On ordinary failure,
// all newly acquired pins are released and the committed output array/count
// remain unchanged. ROLLBACK_FAILURE means the caller must inspect scratch
// pins and reconcile their lifetime with GRDG. All spans must be disjoint.
// On success the caller must release every returned pin through GRDG, after
// its own renderer's GPU completion/lifetime fence makes this safe.
SIRWorldHLODPinResult sirWorldHLODPinCut( const sirWorldHLODPinInput_t& input, const sirWorldHLODPinOutput_t& output ) noexcept;

#endif // __SIR_WORLDHLODPINS_H__
