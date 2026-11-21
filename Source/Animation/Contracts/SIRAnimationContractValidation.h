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

#ifndef __SIRENGINE_SIRANIMATIONCONTRACTVALIDATION_H__
#define __SIRENGINE_SIRANIMATIONCONTRACTVALIDATION_H__

#include <SIRPCH.hpp>

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

	static sirAnimationResult_t validateVersion( const sirAnimationContractVersion_t& requested ) noexcept;
	static sirAnimationResult_t validateCapacity( const sirAnimationCapacityContract_t& capacity ) noexcept;
	static sirAnimationResult_t validateAsset( const sirAnimationAssetRef_t& asset, sirAnimationAssetKind_t eExpectedKind,
	                                           uint64_t nExpectedRevision ) noexcept;
	static sirAnimationResult_t validateInstance( const sirAnimationInstance_t& instance ) noexcept;
	static sirAnimationResult_t validateBoneSelection( const sirAnimationBoneSelection_t& bones, uint32_t nMaximumBones ) noexcept;
	static sirAnimationResult_t validateStateAdvance( const sirAnimationStateSnapshot_t& current,
	                                                  const sirAnimationStateSnapshot_t& next ) noexcept;
	static sirAnimationResult_t validatePublication( const sirAnimationPosePublication_t& pose, uint32_t nMaximumBones ) noexcept;
	static sirAnimationResult_t validateGraphScratch( const sirAnimationGraphRequest_t& request,
	                                                  const sirAnimationGraphScratch_t& scratch ) noexcept;
	static sirAnimationResult_t validateBufferRange( uint64_t nOffset, uint64_t nStride, uint64_t nCount, uint64_t nCapacity ) noexcept;
	static sirAnimationResult_t validateAuthorityAck( const sirAnimationAuthorityRequest_t& request,
	                                                  const sirAnimationAuthorityAck_t& acknowledgement ) noexcept;
	static sirAnimationResult_t validateGraphicsCompletion( const sirGraphicsCompletionSet_t& completion,
	                                                        uint32_t nExpectedDeviceGeneration ) noexcept;
	static sirAnimationResult_t selectDeformation( const sirGraphicsCapabilities_t& graphics, sirAnimationDeformationMode_t ePreferred,
	                                               sirAnimationDeformationMode_t *pSelected ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONCONTRACTVALIDATION_H__
