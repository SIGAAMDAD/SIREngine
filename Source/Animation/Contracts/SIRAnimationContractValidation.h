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

#ifndef __SIR_ANIMATIONCONTRACTVALIDATION_H__
#define __SIR_ANIMATIONCONTRACTVALIDATION_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationTypes.h"
#include <Renderer/SIRGraphicsTypes.h>

// These checks contain no allocations or mutable global state. Producers must
// validate before publishing, while consumers must check generations before
// using borrowed data. A successful validation does not acquire a GRDG pin.
class SIRAnimationContractValidation
{
public:
	SIRAnimationContractValidation( void ) noexcept = delete;

	static SIRAnimationResult validateVersion( const sirAnimationContractVersion_t& requested ) noexcept;
	static SIRAnimationResult validateCapacity( const sirAnimationCapacityContract_t& capacity ) noexcept;
	static SIRAnimationResult validateAsset( const sirAnimationAssetRef_t& asset, SIRAnimationAssetKind eExpectedKind,
	                                           uint64_t nExpectedRevision ) noexcept;
	static SIRAnimationResult validateInstance( const sirAnimationInstance_t& instance ) noexcept;
	static SIRAnimationResult validateBoneSelection( const sirAnimationBoneSelection_t& bones, uint32_t nMaximumBones ) noexcept;
	static SIRAnimationResult validateStateAdvance( const sirAnimationStateSnapshot_t& current,
	                                                  const sirAnimationStateSnapshot_t& next ) noexcept;
	static SIRAnimationResult validatePublication( const sirAnimationPosePublication_t& pose, uint32_t nMaximumBones ) noexcept;
	static SIRAnimationResult validateGraphScratch( const sirAnimationGraphRequest_t& request,
	                                                  const sirAnimationGraphScratch_t& scratch ) noexcept;
	static SIRAnimationResult validateBufferRange( uint64_t nOffset, uint64_t nStride, uint64_t nCount, uint64_t nCapacity ) noexcept;
	static SIRAnimationResult validateAuthorityAck( const sirAnimationAuthorityRequest_t& request,
	                                                  const sirAnimationAuthorityAck_t& acknowledgement ) noexcept;
	static SIRAnimationResult validateGraphicsCompletion( const sirGraphicsCompletionSet_t& completion,
	                                                        uint32_t nExpectedDeviceGeneration ) noexcept;
	static SIRAnimationResult selectDeformation( const sirGraphicsCapabilities_t& graphics, SIRAnimationDeformationMode ePreferred,
	                                               SIRAnimationDeformationMode *pSelected ) noexcept;

protected:
private:
};

#endif // __SIR_ANIMATIONCONTRACTVALIDATION_H__
