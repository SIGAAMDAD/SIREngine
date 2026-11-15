/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Prediction/World/SIRWorldTrajectoryPredictor.h>

#include <limits.h>

static bool sirPredictionCoordinate( int32_t iOrigin, int32_t iVelocity16, int32_t iAcceleration16, uint32_t nTicks,
                                     int32_t *pCoordinate ) noexcept
{
	const int64_t nTicks64 = nTicks;
	const int64_t nAdvance16 = ( int64_t )iVelocity16 * nTicks64 + ( int64_t )iAcceleration16 * nTicks64 * ( nTicks64 - 1 ) / 2;
	const int64_t nPosition = ( int64_t )iOrigin + nAdvance16 / 65536;
	if ( nPosition < INT32_MIN || nPosition > INT32_MAX ) {
		return false;
	}
	*pCoordinate = ( int32_t )nPosition;
	return true;
}

static bool sirPredictionValidActor( const sirWorldTrajectorySource_t& actor, const sirWorldPredictionConfig_t& config ) noexcept
{
	if ( actor.source.nSourceId == 0 || actor.source.nGeneration == 0 || actor.nWorldGeneration != config.nWorldGeneration ||
	     actor.source.nRadiusCells > 65536 ) {
		return false;
	}
	if ( actor.source.eTarget != SIR_STREAMING_TARGET_PREPARED && actor.source.eTarget != SIR_STREAMING_TARGET_ACTIVE ) {
		return false;
	}
	const int64_t nLimit = 64LL * 65536;
	const int32_t szValues[6] = { actor.iVelocityX16,     actor.iVelocityY16,     actor.iVelocityZ16,
	                              actor.iAccelerationX16, actor.iAccelerationY16, actor.iAccelerationZ16 };
	for ( uint32_t iValue = 0; iValue < 6; ++iValue ) {
		if ( szValues[iValue] < -nLimit || szValues[iValue] > nLimit ) {
			return false;
		}
	}
	return true;
}

static bool sirPredictionAt( const sirWorldTrajectorySource_t& actor, uint32_t nStep, const sirWorldPredictionConfig_t& config,
                             sirWorldStreamingSource_t *pOutput ) noexcept
{
	const uint32_t nTicks = actor.bTeleported ? 0 : nStep * config.nStepTicks;
	sirWorldStreamingSource_t projected = actor.source;
	if ( !sirPredictionCoordinate( actor.source.iGridX, actor.iVelocityX16, actor.iAccelerationX16, nTicks, &projected.iGridX ) ||
	     !sirPredictionCoordinate( actor.source.iGridY, actor.iVelocityY16, actor.iAccelerationY16, nTicks, &projected.iGridY ) ||
	     !sirPredictionCoordinate( actor.source.iGridZ, actor.iVelocityZ16, actor.iAccelerationZ16, nTicks, &projected.iGridZ ) ) {
		return false;
	}
	if ( nStep != 0 ) {
		projected.eTarget = SIR_STREAMING_TARGET_PREPARED;
		const uint32_t nDrop = nStep < 64 ? nStep : 64;
		projected.nPriority = projected.nPriority > nDrop ? ( uint8_t )( projected.nPriority - nDrop ) : 1;
		if ( nTicks > UINT64_MAX - projected.nDeadlineTick ) {
			return false;
		}
		projected.nDeadlineTick += nTicks;
	}
	*pOutput = projected;
	return true;
}

static bool sirPredictionHintMatches( const sirWorldStreamingSource_t& source, const sirWorldCoAccessHint_t& hint ) noexcept
{
	return source.iGridX == hint.iOriginX && source.iGridY == hint.iOriginY && source.iGridZ == hint.iOriginZ;
}

static sirWorldPredictionResult_t sirPredictionCollect( const sirWorldTrajectorySource_t *pActors, uint32_t nActors,
                                                        const sirWorldCoAccessHint_t *pHints, uint32_t nHints,
                                                        const sirWorldPredictionConfig_t& config, sirWorldStreamingSource_t *pOutput,
                                                        uint32_t *pCount ) noexcept
{
	uint32_t nWritten = 0;
	const uint32_t nSteps = config.nHorizonTicks / config.nStepTicks;
	for ( uint32_t iActor = 0; iActor < nActors; ++iActor ) {
		const sirWorldTrajectorySource_t& actor = pActors[iActor];
		for ( uint32_t iStep = 0; iStep <= nSteps; ++iStep ) {
			sirWorldStreamingSource_t projected = {};
			if ( !sirPredictionAt( actor, iStep, config, &projected ) ) {
				return SIR_WORLD_PREDICTION_OUT_OF_RANGE;
			}
			if ( nWritten == UINT32_MAX ) {
				return SIR_WORLD_PREDICTION_OUT_OF_RANGE;
			}
			if ( pOutput != nullptr ) {
				pOutput[nWritten] = projected;
			}
			++nWritten;
			uint32_t nHintsMatched = 0;
			for ( uint32_t iHint = 0; iHint < nHints; ++iHint ) {
				const sirWorldCoAccessHint_t& hint = pHints[iHint];
				if ( !sirPredictionHintMatches( projected, hint ) || nHintsMatched >= config.nMaxCoAccessPerStep ) {
					continue;
				}
				++nHintsMatched;
				if ( nWritten == UINT32_MAX ) {
					return SIR_WORLD_PREDICTION_OUT_OF_RANGE;
				}
				if ( pOutput != nullptr ) {
					sirWorldStreamingSource_t associated = projected;
					associated.iGridX = hint.iHintX;
					associated.iGridY = hint.iHintY;
					associated.iGridZ = hint.iHintZ;
					associated.eTarget = SIR_STREAMING_TARGET_PREPARED;
					associated.nPriority = associated.nPriority < hint.nPriority ? associated.nPriority : hint.nPriority;
					pOutput[nWritten] = associated;
				}
				++nWritten;
			}
		}
	}
	*pCount = nWritten;
	return SIR_WORLD_PREDICTION_OK;
}

sirWorldPredictionResult_t SIRWorldTrajectoryPredictor::plan( const sirWorldTrajectorySource_t *pActors, uint32_t nActors,
                                                              const sirWorldCoAccessHint_t *pHints, uint32_t nHints,
                                                              const sirWorldPredictionConfig_t& config, sirWorldStreamingSource_t *pOutput,
                                                              uint32_t nCapacity, uint32_t *pRequired ) noexcept
{
	if ( pRequired == nullptr || config.nWorldGeneration == 0 || config.nStepTicks == 0 || config.nHorizonTicks > 32 ||
	     config.nStepTicks > 32 || ( nActors != 0 && pActors == nullptr ) || ( nHints != 0 && pHints == nullptr ) ) {
		return SIR_WORLD_PREDICTION_INVALID_ARGUMENT;
	}
	for ( uint32_t iActor = 0; iActor < nActors; ++iActor ) {
		if ( pActors[iActor].nWorldGeneration != config.nWorldGeneration ) {
			return SIR_WORLD_PREDICTION_STALE_WORLD;
		}
		if ( !sirPredictionValidActor( pActors[iActor], config ) ) {
			return SIR_WORLD_PREDICTION_INVALID_ARGUMENT;
		}
	}
	uint32_t nRequired = 0;
	const sirWorldPredictionResult_t eCount = sirPredictionCollect( pActors, nActors, pHints, nHints, config, nullptr, &nRequired );
	if ( eCount != SIR_WORLD_PREDICTION_OK ) {
		return eCount;
	}
	*pRequired = nRequired;
	if ( nRequired > nCapacity ) {
		return SIR_WORLD_PREDICTION_OUT_OF_CAPACITY;
	}
	if ( nRequired != 0 && pOutput == nullptr ) {
		return SIR_WORLD_PREDICTION_INVALID_ARGUMENT;
	}
	return sirPredictionCollect( pActors, nActors, pHints, nHints, config, pOutput, &nRequired );
}
