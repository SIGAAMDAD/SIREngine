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

#include <stdio.h>
#include <stdlib.h>

static void requireVG( bool bCondition, const char *pszDescription ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "VG contract FAILED: %s\n", pszDescription );
		abort();
	}
}

int main( void )
{
	const sirMeshKey_t mesh = { 400, 9, 3, 0 };
	const SIRResourceKey resource = { 400, 7, 3 };
	sirVGResourceBinding_t binding = {};
	requireVG( sirVGBindResource( mesh, resource, 9, &binding ) == SIR_VG_CONTRACT_OK, "bind canonical identity" );
	requireVG( binding.nResourceTypeId == 7 && binding.nPublishedRevision == 9, "typed resource revision" );
	requireVG( sirVGBindResource( mesh, resource, 9, nullptr ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "null output" );
	const sirVGResourceBinding_t prior = binding;
	requireVG( sirVGBindResource( mesh, { 400, 8, 3 }, 9, &binding ) == SIR_VG_CONTRACT_OK, "type namespace disjoint" );
	requireVG( binding.nResourceTypeId == 8, "typed disjoint key" );
	binding = prior;
	requireVG( sirVGBindResource( mesh, resource, 10, &binding ) == SIR_VG_CONTRACT_STALE_REVISION, "stale publication" );
	requireVG( binding.nPublishedRevision == 9, "no partial replacement" );
	requireVG( sirVGBindResource( mesh, { 401, 7, 3 }, 9, &binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "wrong asset" );
	requireVG( sirVGBindResource( mesh, { 400, 7, 4 }, 9, &binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "wrong variant" );
	requireVG( sirVGBindResource( mesh, { 400, 0, 3 }, 9, &binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "untyped key" );
	requireVG( sirVGBindResource( { 0, 9, 3, 0 }, resource, 9, &binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "null asset" );
	requireVG( sirVGBindResource( { 400, 0, 3, 0 }, resource, 0, &binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "null revision" );
	requireVG( sirVGBindResource( { 400, 9, 3, 1 }, resource, 9, &binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "reserved mesh bytes" );
	binding.nContractVersion = 2;
	requireVG( sirVGValidateResourceBinding( binding ) == SIR_VG_CONTRACT_VERSION_MISMATCH, "contract version" );
	binding = prior;
	binding.nResourceTypeId = 8;
	requireVG( sirVGValidateResourceBinding( binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "type forgery" );
	binding = prior;

	sirVGFramePlan_t plan = {};
	plan.nVersion = 1;
	plan.nPageSnapshotGeneration = 17;
	sirVGPageObservation_t page = {};
	page.resource = prior;
	page.nPageId = 12;
	page.nPageGeneration = 17;
	page.nSourcePinGeneration = 6;
	page.nDeviceGeneration = 5;
	page.nManifestGeneration = 9;
	page.nRequestEpoch = 3;
	page.eState = SIR_VG_PAGE_RESIDENT;
	page.bRootFallbackResident = 1;
	page.bSourcePinned = 1;
	page.required = { 77, 5, 3, 2, { 10, 15, 0, 0 } };
	page.observed = { 77, 5, 3, 2, { 10, 15, 0, 0 } };
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_OK, "ready resident page" );
	page.observed.szCompletedValues[1] = 14;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_NOT_READY, "all lanes required" );
	page.observed.szCompletedValues[1] = 15;
	page.observed.nProviderGeneration = 6;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_GENERATION, "provider generation" );
	page.observed.nProviderGeneration = 5;
	page.nPageGeneration = 18;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_GENERATION, "snapshot generation" );
	page.nPageGeneration = 17;
	page.bSourcePinned = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_NOT_READY, "missing source pin" );
	page.bSourcePinned = 1;
	page.bRootFallbackResident = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_NOT_READY, "missing root fallback" );
	page.bRootFallbackResident = 1;
	page.eState = SIR_VG_PAGE_RETIRING;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_NOT_READY, "retiring never selected" );
	page.eState = SIR_VG_PAGE_RESIDENT;
	page.nDeviceGeneration = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "invalid device generation" );
	page.nDeviceGeneration = 5;
	page.resource.nPublishedRevision = 8;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_REVISION, "stale mesh revision" );
	page.resource = prior;
	plan.nVersion = 2;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_VERSION_MISMATCH, "plan version mismatch" );
	plan.nVersion = 1;

	sirGraphicsABICapabilities_t device = {};
	device.nSize = sizeof( device );
	device.nDeviceGeneration = 5;
	device.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	device.nEnabledQueues = 1;
	device.nPipelineModelMask = 7;
	sirGraphicsABIPipelineModelCaps_t model = {};
	model.nSize = sizeof( model );
	model.nDeviceGeneration = 5;
	model.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	model.nQueueMask = 1;
	model.nMaximumVertexStreams = 1;
	sirVGExecutionChoice_t choice = {};
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_OK, "fixed function CPU indexed" );
	requireVG( choice.eTier == SIR_VG_EXECUTION_CPU_INDEXED, "no invented legacy GPU indirect" );
	model.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_OK, "programmable CPU indexed" );
	model.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_OK, "modern CPU fallback" );
	device.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE | SIR_GRAPHICS_FEATURE_INDIRECT_DRAW;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_OK && choice.eTier == SIR_VG_EXECUTION_GPU_INDIRECT,
	           "GPU indirect conditional acceleration" );
	device.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_MESH_SHADER;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_OK && choice.eTier == SIR_VG_EXECUTION_GPU_MESH,
	           "GPU mesh capability" );
	model.nDeviceGeneration = 6;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_STALE_GENERATION, "stale device capability" );
	model.nDeviceGeneration = 5;
	device.nPipelineModelMask = 0;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_UNSUPPORTED, "unavailable pipeline model" );
	device.nPipelineModelMask = 7;
	device.nEnabledFeatures = 0;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_UNSUPPORTED, "graphics is required" );
	device.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	model.nMaximumVertexStreams = 0;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_UNSUPPORTED, "missing vertex support" );
	model.nMaximumVertexStreams = 1;
	requireVG( sirVGSelectExecution( device, model, nullptr ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "null choice" );

	choice = { SIR_VG_CONTRACT_VERSION, 5, SIR_VG_EXECUTION_CPU_INDEXED, SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION };
	sirVGDrawRange_t range = {};
	range.nIndexCount = 6;
	range.nFirstIndex = 14;
	range.nBaseVertex = -22;
	range.nFirstInstance = 34;
	range.nInstanceCount = 3;
	sirGraphicsABIDrawIndexed_t draw = {};
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_OK, "backend-neutral indexed draw" );
	requireVG( draw.nIndices == 6 && draw.nFirstIndex == 14 && draw.nInstances == 3 && draw.nVertexOffset == -22 &&
	               draw.nFirstInstance == 34 && draw.nReserved == 0,
	           "lossless firstInstance and baseVertex" );
	choice.nContractVersion = 2;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_VERSION_MISMATCH, "draw version" );
	choice.nContractVersion = 1;
	range.nFirstIndex = UINT32_MAX;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "index overflow" );
	range.nFirstIndex = 14;
	range.nInstanceCount = 0;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero instances" );
	range.nInstanceCount = 3;
	requireVG( sirVGEncodeIndexedDraw( range, choice, nullptr ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "null draw" );
	// Exercise every individual invalidity gate, including short-circuit edges.
	binding = prior;
	binding.nResourceTypeId = 0;
	requireVG( sirVGValidateResourceBinding( binding ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "unregistered resource type" );
	binding = prior;
	page = {};
	page.resource = prior;
	page.nPageId = 12;
	page.nPageGeneration = 17;
	page.nSourcePinGeneration = 6;
	page.nDeviceGeneration = 5;
	page.nManifestGeneration = 9;
	page.nRequestEpoch = 3;
	page.eState = SIR_VG_PAGE_RESIDENT;
	page.bRootFallbackResident = 1;
	page.bSourcePinned = 1;
	page.required = { 77, 5, 3, 2, { 10, 15, 0, 0 } };
	page.observed = { 77, 5, 3, 2, { 10, 15, 0, 0 } };
	plan.nPageSnapshotGeneration = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero snapshot generation" );
	plan.nPageSnapshotGeneration = 17;
	page.nPageId = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero page ID" );
	page.nPageId = 12;
	page.nPageGeneration = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero page generation" );
	page.nPageGeneration = 17;
	page.nSourcePinGeneration = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero pin generation" );
	page.nSourcePinGeneration = 6;
	page.nManifestGeneration = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero manifest generation" );
	page.nManifestGeneration = 9;
	page.nRequestEpoch = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero request epoch" );
	page.nRequestEpoch = 3;
	page.eState = static_cast<sirVGPageState_t>( 5 );
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "unknown page state" );
	page.eState = SIR_VG_PAGE_RESIDENT;
	page.bRootFallbackResident = 2;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "invalid root bool" );
	page.bRootFallbackResident = 1;
	page.bSourcePinned = 2;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "invalid pin bool" );
	page.bSourcePinned = 1;
	page.szReserved[0] = 1;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "reserved zero" );
	page.szReserved[0] = 0;
	page.szReserved[1] = 1;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "reserved one" );
	page.szReserved[1] = 0;
	page.required.nProviderId = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_GENERATION, "invalid required provider" );
	page.required.nProviderId = 77;
	page.observed.nProviderId = 0;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_GENERATION, "invalid observed provider" );
	page.observed.nProviderId = 78;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_GENERATION, "other provider identity" );
	page.observed.nProviderId = 77;
	page.observed.nLaneCount = 1;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_GENERATION, "other provider lane count" );
	page.observed.nLaneCount = 3;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_STALE_GENERATION, "valid but mismatched lane counts" );
	page.observed.nLaneCount = 2;
	requireVG( sirVGValidatePageObservation( page, plan ) == SIR_VG_CONTRACT_OK, "restored page" );

	device = {};
	device.nSize = sizeof( device );
	device.nDeviceGeneration = 5;
	device.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	device.nEnabledQueues = 1;
	device.nPipelineModelMask = 7;
	model = {};
	model.nSize = sizeof( model );
	model.nDeviceGeneration = 5;
	model.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	model.nQueueMask = 1;
	model.nMaximumVertexStreams = 1;
	device.nSize = sizeof( device ) - 1;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "short device caps" );
	device.nSize = sizeof( device );
	model.nSize = sizeof( model ) - 1;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "short model caps" );
	model.nSize = sizeof( model );
	device.nDeviceGeneration = 0;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero device gen" );
	device.nDeviceGeneration = 5;
	model.nDeviceGeneration = 0;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero model gen" );
	model.nDeviceGeneration = 5;
	model.nModel = 0;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "low model" );
	model.nModel = 4;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "high model" );
	model.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	model.nReserved = 1;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "reserved model bytes" );
	model.nReserved = 0;
	device.nReserved = 1;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "reserved device bytes" );
	device.nReserved = 0;
	model.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_UNSUPPORTED, "missing model-required feature" );
	model.nRequiredFeatureMask = 0;
	model.nQueueMask = 3;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_UNSUPPORTED, "missing model-required queue" );
	model.nQueueMask = 0;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_UNSUPPORTED, "zero execution queues" );
	model.nQueueMask = 1;
	device.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_OK && choice.eTier == SIR_VG_EXECUTION_CPU_INDEXED,
	           "no indirect feature still CPU" );
	device.nEnabledFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_INDIRECT_DRAW;
	requireVG( sirVGSelectExecution( device, model, &choice ) == SIR_VG_CONTRACT_OK && choice.eTier == SIR_VG_EXECUTION_CPU_INDEXED,
	           "no compute feature still CPU" );

	choice = { SIR_VG_CONTRACT_VERSION, 5, SIR_VG_EXECUTION_CPU_INDEXED, SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION };
	range = {};
	range.nIndexCount = 6;
	range.nFirstIndex = 14;
	range.nBaseVertex = -22;
	range.nFirstInstance = 34;
	range.nInstanceCount = 3;
	choice.nDeviceGeneration = 0;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "draw device invalid" );
	choice.nDeviceGeneration = 5;
	choice.nPipelineModel = 0;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "draw model low" );
	choice.nPipelineModel = 4;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "draw model high" );
	choice.nPipelineModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	choice.eTier = static_cast<sirVGExecutionTier_t>( 0 );
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "draw tier low" );
	choice.eTier = static_cast<sirVGExecutionTier_t>( 4 );
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "draw tier high" );
	choice.eTier = SIR_VG_EXECUTION_CPU_INDEXED;
	range.nIndexCount = 0;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "zero draw indices" );
	range.nIndexCount = 5;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "nontriangle indices" );
	range.nIndexCount = 6;
	range.nFirstInstance = UINT32_MAX;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_INVALID_ARGUMENT, "instance overflow" );
	range.nFirstInstance = 34;
	requireVG( sirVGEncodeIndexedDraw( range, choice, &draw ) == SIR_VG_CONTRACT_OK, "restored draw" );

	return 0;
}
