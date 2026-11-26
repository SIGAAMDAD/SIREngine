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

#include <Mesh/SIRVGContracts.h>

SIRVGContractResult sirVGValidateResourceBinding( const sirVGResourceBinding_t& binding ) noexcept
{
	if ( binding.nContractVersion != SIR_VG_CONTRACT_VERSION ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_VERSION_MISMATCH;
	}
	if ( binding.keyMesh.nAssetId == 0 || binding.keyMesh.nRevision == 0 || binding.keyMesh.nReserved != 0 ||
	     binding.keyResource.m_nTypeId == 0 || binding.nResourceTypeId == 0 || binding.keyResource.m_nTypeId != binding.nResourceTypeId ||
	     binding.keyResource.m_nAssetId != binding.keyMesh.nAssetId || binding.keyResource.m_nVariantId != binding.keyMesh.nVariant ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	return binding.nPublishedRevision == binding.keyMesh.nRevision ? SIRVGContractResult::SIR_VG_CONTRACT_OK : SIRVGContractResult::SIR_VG_CONTRACT_STALE_REVISION;
}

SIRVGContractResult sirVGBindResource( const sirMeshKey_t& mesh, const SIRResourceKey& resource, uint64_t nPublishedRevision,
                                         sirVGResourceBinding_t *pBinding ) noexcept
{
	if ( pBinding == nullptr ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	const sirVGResourceBinding_t candidate = { mesh, resource, nPublishedRevision, SIR_VG_CONTRACT_VERSION, resource.m_nTypeId };
	const SIRVGContractResult eResult = sirVGValidateResourceBinding( candidate );
	if ( eResult == SIRVGContractResult::SIR_VG_CONTRACT_OK ) {
		*pBinding = candidate;
	}
	return eResult;
}

SIRVGContractResult sirVGValidatePageObservation( const sirVGPageObservation_t& page, const sirVGFramePlan_t& plan ) noexcept
{
	const SIRVGContractResult eBinding = sirVGValidateResourceBinding( page.resource );
	if ( eBinding != SIRVGContractResult::SIR_VG_CONTRACT_OK ) {
		return eBinding;
	}
	if ( plan.nVersion != SIR_VG_FRAME_PLAN_VERSION ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_VERSION_MISMATCH;
	}
	if ( plan.nPageSnapshotGeneration == 0 || page.nPageId == 0 || page.nPageGeneration == 0 || page.nSourcePinGeneration == 0 ||
	     page.nDeviceGeneration == 0 || page.nManifestGeneration == 0 || page.nRequestEpoch == 0 || page.eState > SIRVGPageState::SIR_VG_PAGE_RETIRING ||
	     page.bRootFallbackResident > 1 || page.bSourcePinned > 1 || page.szReserved[0] != 0 || page.szReserved[1] != 0 ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	if ( page.nPageGeneration != plan.nPageSnapshotGeneration ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_STALE_GENERATION;
	}
	if ( page.eState != SIRVGPageState::SIR_VG_PAGE_RESIDENT || page.bRootFallbackResident == 0 || page.bSourcePinned == 0 ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_NOT_READY;
	}
	if ( !sirProviderCompletionValid( page.required ) || !sirProviderCompletionValid( page.observed ) ||
	     page.required.nProviderId != page.observed.nProviderId || page.required.nProviderGeneration != page.observed.nProviderGeneration ||
	     page.required.nLaneCount != page.observed.nLaneCount ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_STALE_GENERATION;
	}
	return sirProviderCompletionSatisfied( page.required, page.observed ) ? SIRVGContractResult::SIR_VG_CONTRACT_OK : SIRVGContractResult::SIR_VG_CONTRACT_NOT_READY;
}

SIRVGContractResult sirVGSelectExecution( const sirGraphicsABICapabilities_t& device, const sirGraphicsABIPipelineModelCaps_t& model,
                                            sirVGExecutionChoice_t *pChoice ) noexcept
{
	if ( pChoice == nullptr || device.nSize < sizeof( device ) || model.nSize < sizeof( model ) || device.nDeviceGeneration == 0 ||
	     model.nDeviceGeneration == 0 || model.nModel < SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN ||
	     model.nModel > SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION || model.nReserved != 0 || device.nReserved != 0 ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	if ( device.nDeviceGeneration != model.nDeviceGeneration ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_STALE_GENERATION;
	}
	if ( ( device.nPipelineModelMask & ( 1U << ( model.nModel - 1 ) ) ) == 0 ||
	     ( device.nEnabledFeatures & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ||
	     ( device.nEnabledFeatures & model.nRequiredFeatureMask ) != model.nRequiredFeatureMask ||
	     ( device.nEnabledQueues & model.nQueueMask ) != model.nQueueMask || model.nQueueMask == 0 || model.nMaximumVertexStreams == 0 ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_UNSUPPORTED;
	}
	const bool bCompute = ( device.nEnabledFeatures & ( SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) ) ==
	                      ( SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_SHADER_STORAGE );
	const bool bIndirect = ( device.nEnabledFeatures & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_INDIRECT_DRAW ) != 0;
	const bool bMesh = ( device.nEnabledFeatures & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_MESH_SHADER ) != 0;
	SIRVGExecutionTier eTier = SIRVGExecutionTier::SIR_VG_EXECUTION_CPU_INDEXED;
	if ( model.nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN && bCompute && bIndirect ) {
		eTier = bMesh ? SIRVGExecutionTier::SIR_VG_EXECUTION_GPU_MESH : SIRVGExecutionTier::SIR_VG_EXECUTION_GPU_INDIRECT;
	}
	*pChoice = { SIR_VG_CONTRACT_VERSION, device.nDeviceGeneration, eTier, model.nModel };
	return SIRVGContractResult::SIR_VG_CONTRACT_OK;
}

SIRVGContractResult sirVGEncodeIndexedDraw( const sirVGDrawRange_t& range, const sirVGExecutionChoice_t& choice,
                                              sirGraphicsABIDrawIndexed_t *pNativeDraw ) noexcept
{
	if ( pNativeDraw == nullptr || choice.nDeviceGeneration == 0 || choice.nPipelineModel < SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN ||
	     choice.nPipelineModel > SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION || choice.eTier < SIRVGExecutionTier::SIR_VG_EXECUTION_CPU_INDEXED ||
	     choice.eTier > SIRVGExecutionTier::SIR_VG_EXECUTION_GPU_MESH || range.nIndexCount == 0 || range.nIndexCount % 3 != 0 || range.nInstanceCount == 0 ||
	     range.nFirstIndex > UINT32_MAX - range.nIndexCount || range.nFirstInstance > UINT32_MAX - range.nInstanceCount ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	if ( choice.nContractVersion != SIR_VG_CONTRACT_VERSION ) {
		return SIRVGContractResult::SIR_VG_CONTRACT_VERSION_MISMATCH;
	}
	*pNativeDraw = { range.nIndexCount, range.nInstanceCount, range.nFirstIndex, range.nBaseVertex, range.nFirstInstance, 0 };
	return SIRVGContractResult::SIR_VG_CONTRACT_OK;
}
