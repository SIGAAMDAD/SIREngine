/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRWORLDTRAJECTORYPREDICTOR_H__
#define __SIRENGINE_SIRWORLDTRAJECTORYPREDICTOR_H__

#include <World/Partition/SIRWorldPartition.h>

// Q16.16 grid-cell velocity/acceleration, fixed tick horizon and stable
// integer arithmetic. No hardware-dependent floating-point prediction.
typedef struct sirWorldTrajectorySource_s
{
	sirWorldStreamingSource_t source;
	int32_t iVelocityX16;
	int32_t iVelocityY16;
	int32_t iVelocityZ16;
	int32_t iAccelerationX16;
	int32_t iAccelerationY16;
	int32_t iAccelerationZ16;
	uint32_t nWorldGeneration;
	bool bTeleported;
} sirWorldTrajectorySource_t;

// Offline static co-access/visibility prior; never records live actor state.
// Each matching future grid cell emits a low-priority PREPARED demand.
typedef struct sirWorldCoAccessHint_s
{
	int32_t iOriginX;
	int32_t iOriginY;
	int32_t iOriginZ;
	int32_t iHintX;
	int32_t iHintY;
	int32_t iHintZ;
	uint8_t nPriority;
} sirWorldCoAccessHint_t;

typedef struct sirWorldPredictionConfig_s
{
	uint32_t nWorldGeneration;
	uint32_t nHorizonTicks;
	uint32_t nStepTicks;
	uint32_t nMaxCoAccessPerStep;
} sirWorldPredictionConfig_t;

typedef enum sirWorldPredictionResult_e : uint8_t
{
	SIR_WORLD_PREDICTION_OK = 0,
	SIR_WORLD_PREDICTION_INVALID_ARGUMENT,
	SIR_WORLD_PREDICTION_STALE_WORLD,
	SIR_WORLD_PREDICTION_OUT_OF_CAPACITY,
	SIR_WORLD_PREDICTION_OUT_OF_RANGE
} sirWorldPredictionResult_t;

// Stateless producer of immutable World Partition sources. No second world
// streaming scheduler, physical cache or resource registry is introduced.
class SIRWorldTrajectoryPredictor final
{
public:
	static sirWorldPredictionResult_t plan( const sirWorldTrajectorySource_t *pActors, uint32_t nActors,
	                                        const sirWorldCoAccessHint_t *pHints, uint32_t nHints, const sirWorldPredictionConfig_t& config,
	                                        sirWorldStreamingSource_t *pOutput, uint32_t nCapacity, uint32_t *pRequired ) noexcept;
};

#endif // __SIRENGINE_SIRWORLDTRAJECTORYPREDICTOR_H__
