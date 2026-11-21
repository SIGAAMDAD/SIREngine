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

#include <Mesh/SIRVGContracts.h>

sirVGContractResult_t sirVGValidateResourceBinding( const sirVGResourceBinding_t& binding ) noexcept
{
	if ( binding.nContractVersion != SIR_VG_CONTRACT_VERSION ) {
		return SIR_VG_CONTRACT_VERSION_MISMATCH;
	}
	if ( binding.keyMesh.nAssetId == 0 || binding.keyMesh.nRevision == 0 || binding.keyMesh.nReserved != 0 ||
	     binding.keyResource.m_nTypeId == 0 || binding.nResourceTypeId == 0 || binding.keyResource.m_nTypeId != binding.nResourceTypeId ||
	     binding.keyResource.m_nAssetId != binding.keyMesh.nAssetId || binding.keyResource.m_nVariantId != binding.keyMesh.nVariant ) {
		return SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	return binding.nPublishedRevision == binding.keyMesh.nRevision ? SIR_VG_CONTRACT_OK : SIR_VG_CONTRACT_STALE_REVISION;
}

sirVGContractResult_t sirVGBindResource( const sirMeshKey_t& mesh, const SIRResourceKey& resource, uint64_t nPublishedRevision,
                                         sirVGResourceBinding_t *pBinding ) noexcept
{
	if ( pBinding == nullptr ) {
		return SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	const sirVGResourceBinding_t candidate = { mesh, resource, nPublishedRevision, SIR_VG_CONTRACT_VERSION, resource.m_nTypeId };
	const sirVGContractResult_t eResult = sirVGValidateResourceBinding( candidate );
	if ( eResult == SIR_VG_CONTRACT_OK ) {
		*pBinding = candidate;
	}
	return eResult;
}

sirVGContractResult_t sirVGValidatePageObservation( const sirVGPageObservation_t& page, const sirVGFramePlan_t& plan ) noexcept
{
	const sirVGContractResult_t eBinding = sirVGValidateResourceBinding( page.resource );
	if ( eBinding != SIR_VG_CONTRACT_OK ) {
		return eBinding;
	}
	if ( plan.nVersion != SIR_VG_FRAME_PLAN_VERSION ) {
		return SIR_VG_CONTRACT_VERSION_MISMATCH;
	}
	if ( plan.nPageSnapshotGeneration == 0 || page.nPageId == 0 || page.nPageGeneration == 0 || page.nSourcePinGeneration == 0 ||
	     page.nDeviceGeneration == 0 || page.nManifestGeneration == 0 || page.nRequestEpoch == 0 || page.eState > SIR_VG_PAGE_RETIRING ||
	     page.bRootFallbackResident > 1 || page.bSourcePinned > 1 || page.szReserved[0] != 0 || page.szReserved[1] != 0 ) {
		return SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	if ( page.nPageGeneration != plan.nPageSnapshotGeneration ) {
		return SIR_VG_CONTRACT_STALE_GENERATION;
	}
	if ( page.eState != SIR_VG_PAGE_RESIDENT || page.bRootFallbackResident == 0 || page.bSourcePinned == 0 ) {
		return SIR_VG_CONTRACT_NOT_READY;
	}
	if ( !sirProviderCompletionValid( page.required ) || !sirProviderCompletionValid( page.observed ) ||
	     page.required.nProviderId != page.observed.nProviderId || page.required.nProviderGeneration != page.observed.nProviderGeneration ||
	     page.required.nLaneCount != page.observed.nLaneCount ) {
		return SIR_VG_CONTRACT_STALE_GENERATION;
	}
	return sirProviderCompletionSatisfied( page.required, page.observed ) ? SIR_VG_CONTRACT_OK : SIR_VG_CONTRACT_NOT_READY;
}

sirVGContractResult_t sirVGSelectExecution( const sirGraphicsABICapabilities_t& device, const sirGraphicsABIPipelineModelCaps_t& model,
                                            sirVGExecutionChoice_t *pChoice ) noexcept
{
	if ( pChoice == nullptr || device.nSize < sizeof( device ) || model.nSize < sizeof( model ) || device.nDeviceGeneration == 0 ||
	     model.nDeviceGeneration == 0 || model.nModel < SIR_GRAPHICS_PIPELINE_MODEL_MODERN ||
	     model.nModel > SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION || model.nReserved != 0 || device.nReserved != 0 ) {
		return SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	if ( device.nDeviceGeneration != model.nDeviceGeneration ) {
		return SIR_VG_CONTRACT_STALE_GENERATION;
	}
	if ( ( device.nPipelineModelMask & ( 1U << ( model.nModel - 1 ) ) ) == 0 ||
	     ( device.nEnabledFeatures & SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ||
	     ( device.nEnabledFeatures & model.nRequiredFeatureMask ) != model.nRequiredFeatureMask ||
	     ( device.nEnabledQueues & model.nQueueMask ) != model.nQueueMask || model.nQueueMask == 0 || model.nMaximumVertexStreams == 0 ) {
		return SIR_VG_CONTRACT_UNSUPPORTED;
	}
	const bool bCompute = ( device.nEnabledFeatures & ( SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) ) ==
	                      ( SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE );
	const bool bIndirect = ( device.nEnabledFeatures & SIR_GRAPHICS_FEATURE_INDIRECT_DRAW ) != 0;
	const bool bMesh = ( device.nEnabledFeatures & SIR_GRAPHICS_FEATURE_MESH_SHADER ) != 0;
	sirVGExecutionTier_t eTier = SIR_VG_EXECUTION_CPU_INDEXED;
	if ( model.nModel == SIR_GRAPHICS_PIPELINE_MODEL_MODERN && bCompute && bIndirect ) {
		eTier = bMesh ? SIR_VG_EXECUTION_GPU_MESH : SIR_VG_EXECUTION_GPU_INDIRECT;
	}
	*pChoice = { SIR_VG_CONTRACT_VERSION, device.nDeviceGeneration, eTier, model.nModel };
	return SIR_VG_CONTRACT_OK;
}

sirVGContractResult_t sirVGEncodeIndexedDraw( const sirVGDrawRange_t& range, const sirVGExecutionChoice_t& choice,
                                              sirGraphicsABIDrawIndexed_t *pNativeDraw ) noexcept
{
	if ( pNativeDraw == nullptr || choice.nDeviceGeneration == 0 || choice.nPipelineModel < SIR_GRAPHICS_PIPELINE_MODEL_MODERN ||
	     choice.nPipelineModel > SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION || choice.eTier < SIR_VG_EXECUTION_CPU_INDEXED ||
	     choice.eTier > SIR_VG_EXECUTION_GPU_MESH || range.nIndexCount == 0 || range.nIndexCount % 3 != 0 || range.nInstanceCount == 0 ||
	     range.nFirstIndex > UINT32_MAX - range.nIndexCount || range.nFirstInstance > UINT32_MAX - range.nInstanceCount ) {
		return SIR_VG_CONTRACT_INVALID_ARGUMENT;
	}
	if ( choice.nContractVersion != SIR_VG_CONTRACT_VERSION ) {
		return SIR_VG_CONTRACT_VERSION_MISMATCH;
	}
	*pNativeDraw = { range.nIndexCount, range.nInstanceCount, range.nFirstIndex, range.nBaseVertex, range.nFirstInstance, 0 };
	return SIR_VG_CONTRACT_OK;
}
