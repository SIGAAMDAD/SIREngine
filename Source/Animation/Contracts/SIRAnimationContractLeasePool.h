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

#ifndef __SIR_ANIMATIONCONTRACTLEASEPOOL_H__
#define __SIR_ANIMATIONCONTRACTLEASEPOOL_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationTypes.h"

// Contract-only ticket accounting. Storage comes from the animation owner;
// source/pose bytes are deliberately NOT stored here. Mutations, including
// acquire/release, run on the animation publisher's serialized owner thread.
// Reader threads may retain immutable pose bytes after acquiring a lease, but
// must return the lease through the owner before those bytes can be reclaimed.
typedef struct sirAnimationContractPoseSlot_s
{
	sirAnimationInstance_t instance;
	sirAnimationTick_t tick;
	uint32_t nGeneration;
	uint32_t nActiveReaders;
	uint8_t bActive;
	uint8_t bCancelled;
	uint8_t nReserved[2];
} sirAnimationContractPoseSlot_t;

typedef struct sirAnimationContractReaderSlot_s
{
	sirAnimationPoseHandle_t pose;
	uint32_t nGeneration;
	uint8_t bActive;
	uint8_t nReserved[3];
} sirAnimationContractReaderSlot_t;

class SIRAnimationContractLeasePool
{
public:
	SIRAnimationContractLeasePool( void ) noexcept;
	~SIRAnimationContractLeasePool( void ) noexcept = default;

	uint32_t getOutstandingReaders( void ) const noexcept;

	SIRAnimationResult initialize( sirAnimationContractPoseSlot_t *pPoses, uint32_t nPoseCapacity,
	                                 sirAnimationContractReaderSlot_t *pReaders, uint32_t nReaderCapacity ) noexcept;
	SIRAnimationResult registerPose( sirAnimationPoseHandle_t pose, const sirAnimationInstance_t& instance,
	                                   const sirAnimationTick_t& tick ) noexcept;
	SIRAnimationResult acquire( sirAnimationPoseHandle_t pose, sirAnimationPoseLease_t *pLease ) noexcept;
	SIRAnimationResult release( const sirAnimationPoseLease_t& lease ) noexcept;
	SIRAnimationResult cancelEpoch( uint64_t nWorldEpoch, uint32_t nCancellationGeneration ) noexcept;
	SIRAnimationResult retirePose( sirAnimationPoseHandle_t pose ) noexcept;
	SIRAnimationResult shutdown( void ) noexcept;

protected:
private:
	sirAnimationContractPoseSlot_t *m_pPoses;
	sirAnimationContractReaderSlot_t *m_pReaders;
	uint32_t m_nPoseCapacity;
	uint32_t m_nReaderCapacity;
	uint32_t m_nOutstandingReaders;
	uint64_t m_nCancelledWorldEpoch;
	uint32_t m_nCancellationGeneration;
	uint8_t m_bHasCancellation;
};

#endif // __SIR_ANIMATIONCONTRACTLEASEPOOL_H__
