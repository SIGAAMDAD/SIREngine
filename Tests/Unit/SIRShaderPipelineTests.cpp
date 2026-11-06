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

#include <Renderer/SIRShaderPipeline.h>

#include <stdio.h>

static bool testFormatsAndStages( void ) noexcept
{
	sirGraphicsShaderFormat_t eFormat = SIR_GRAPHICS_SHADER_DXBC;
	if ( sirShaderGetBackendFormat( SIR_GRAPHICS_BACKEND_VULKAN, &eFormat ) != SIR_GRAPHICS_SUCCESS ||
	     eFormat != SIR_GRAPHICS_SHADER_SPIRV ) {
		return false;
	}
	if ( sirShaderGetBackendFormat( SIR_GRAPHICS_BACKEND_D3D12, &eFormat ) != SIR_GRAPHICS_SUCCESS ||
	     eFormat != SIR_GRAPHICS_SHADER_DXIL ) {
		return false;
	}
	if ( sirShaderGetBackendFormat( SIR_GRAPHICS_BACKEND_OPENGL, &eFormat ) != SIR_GRAPHICS_SUCCESS ||
	     eFormat != SIR_GRAPHICS_SHADER_GLSL ) {
		return false;
	}
	if ( sirShaderGetBackendFormat( SIR_GRAPHICS_BACKEND_D3D11, &eFormat ) != SIR_GRAPHICS_UNSUPPORTED ||
	     sirShaderGetBackendFormat( SIR_GRAPHICS_BACKEND_VULKAN, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	if ( sirShaderIsStageValid( 0 ) || sirShaderIsStageValid( 3 ) || sirShaderIsStageValid( 1U << 12 ) ) {
		return false;
	}
	return sirShaderIsStageValid( SIR_GRAPHICS_STAGE_VERTEX ) && sirShaderIsStageValid( SIR_GRAPHICS_STAGE_TASK );
}

static bool testCacheKeys( void ) noexcept
{
	sirShaderCacheDescriptor_t desc = {};
	desc.nSchemaVersion = 1;
	desc.nCompilerVersion = 23;
	desc.nOptimizationFlags = 2;
	desc.nStage = SIR_GRAPHICS_STAGE_COMPUTE;
	desc.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	desc.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	desc.nSourceHash = 0x1020304050607080ULL;
	desc.nEntryPointHash = 123;
	desc.nPermutationKey = 7;
	sirShaderCacheKey_t first = {};
	sirShaderCacheKey_t second = {};
	if ( sirShaderBuildCacheKey( desc, &first ) != SIR_GRAPHICS_SUCCESS ||
	     sirShaderBuildCacheKey( desc, &second ) != SIR_GRAPHICS_SUCCESS || first.nLow != second.nLow || first.nHigh != second.nHigh ||
	     first.nLow == 0 || first.nHigh == 0 ) {
		return false;
	}
	desc.nTargetFingerprint = 900;
	if ( sirShaderBuildCacheKey( desc, &second ) != SIR_GRAPHICS_SUCCESS || ( first.nLow == second.nLow && first.nHigh == second.nHigh ) ) {
		return false;
	}
	desc.nTargetFingerprint = 0;
	desc.nDefinitionsHash = 444;
	if ( sirShaderBuildCacheKey( desc, &second ) != SIR_GRAPHICS_SUCCESS || ( first.nLow == second.nLow && first.nHigh == second.nHigh ) ) {
		return false;
	}
	desc.nDefinitionsHash = 0;
	desc.eFormat = SIR_GRAPHICS_SHADER_DXIL;
	if ( sirShaderBuildCacheKey( desc, &second ) != SIR_GRAPHICS_INVALID_ARGUMENT || second.nLow != 0 || second.nHigh != 0 ) {
		return false;
	}
	desc.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	desc.nEntryPointHash = 0;
	if ( sirShaderBuildCacheKey( desc, &second ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	desc.nEntryPointHash = 123;
	desc.nStage = 0;
	if ( sirShaderBuildCacheKey( desc, &second ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	desc.nStage = SIR_GRAPHICS_STAGE_COMPUTE;
	desc.nRequiredFeatures = 1ULL << 40;
	if ( sirShaderBuildCacheKey( desc, &second ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	desc.nRequiredFeatures = 0;
	desc.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	return sirShaderBuildCacheKey( desc, &second ) == SIR_GRAPHICS_UNSUPPORTED &&
	       sirShaderBuildCacheKey( desc, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testSelectionAndBridge( void ) noexcept
{
	const uint8_t szCode[] = { 1, 2, 3, 4, 5 };
	sirShaderVariant_t variants[4] = {};
	for ( uint32_t iVariant = 0; iVariant < 4; ++iVariant ) {
		variants[iVariant].eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
		variants[iVariant].eFormat = SIR_GRAPHICS_SHADER_SPIRV;
		variants[iVariant].nStage = SIR_GRAPHICS_STAGE_COMPUTE;
		variants[iVariant].nContentHash = 111 + iVariant;
		variants[iVariant].pBytes = szCode;
		variants[iVariant].nByteCount = sizeof( szCode );
		variants[iVariant].pszEntryPoint = "main";
	}
	variants[0].bUber = true;
	variants[1].nPermutationKey = 42;
	variants[2].nPermutationKey = 42;
	variants[2].nVendorId = 0x8086;
	variants[3].nPermutationKey = 42;
	variants[3].nVendorId = 0x8086;
	variants[3].nDeviceId = 1234;
	variants[3].nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;

	sirShaderSelection_t selection = {};
	selection.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	selection.nStage = SIR_GRAPHICS_STAGE_COMPUTE;
	selection.nAvailableFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	selection.nPermutationKey = 42;
	selection.nVendorId = 0x8086;
	selection.nDeviceId = 1234;
	selection.bAllowUber = true;
	const sirShaderVariant_t *pResult = nullptr;
	if ( sirShaderSelectVariant( variants, 4, selection, &pResult ) != SIR_GRAPHICS_SUCCESS || pResult != &variants[3] ) {
		return false;
	}
	selection.nDeviceId = 44;
	if ( sirShaderSelectVariant( variants, 4, selection, &pResult ) != SIR_GRAPHICS_SUCCESS || pResult != &variants[2] ) {
		return false;
	}
	selection.nVendorId = 99;
	if ( sirShaderSelectVariant( variants, 4, selection, &pResult ) != SIR_GRAPHICS_SUCCESS || pResult != &variants[1] ) {
		return false;
	}
	selection.nPermutationKey = 300;
	if ( sirShaderSelectVariant( variants, 4, selection, &pResult ) != SIR_GRAPHICS_SUCCESS || pResult != &variants[0] ) {
		return false;
	}
	selection.bAllowUber = false;
	if ( sirShaderSelectVariant( variants, 4, selection, &pResult ) != SIR_GRAPHICS_UNSUPPORTED || pResult != nullptr ) {
		return false;
	}
	selection.nAvailableFeatures = 0;
	if ( sirShaderSelectVariant( variants, 4, selection, &pResult ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	selection.nAvailableFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	if ( sirShaderSelectVariant( variants, 4, selection, &pResult ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	selection.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	if ( sirShaderSelectVariant( variants, 4, selection, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderSelectVariant( nullptr, 4, selection, &pResult ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     sirShaderSelectVariant( variants, 0, selection, &pResult ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirGraphicsShaderCode_t code = {};
	if ( sirShaderToGraphicsCode( variants[3], &code ) != SIR_GRAPHICS_SUCCESS || code.pBytes != szCode ||
	     code.nContentHash != variants[3].nContentHash || code.eFormat != SIR_GRAPHICS_SHADER_SPIRV ||
	     code.nByteCount != sizeof( szCode ) || code.pszEntryPoint != variants[3].pszEntryPoint ) {
		return false;
	}
	if ( sirShaderToGraphicsCode( variants[3], nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	variants[3].nDeviceId = 1234;
	variants[3].nVendorId = 0;
	if ( sirShaderValidateVariant( variants[3] ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	variants[3].nVendorId = 0x8086;
	variants[3].nRequiredFeatures = 1ULL << 40;
	if ( sirShaderValidateVariant( variants[3] ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	variants[3].nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	variants[3].bUber = true;
	return sirShaderValidateVariant( variants[3] ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testWarmup( void ) noexcept
{
	SIRShaderWarmupQueue queue;
	sirShaderCacheKey_t invalid = {};
	if ( queue.getCount() != 0 || queue.getCapacity() != 128 || queue.push( invalid, 5 ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     queue.tryPop( nullptr ) ) {
		return false;
	}
	const sirShaderCacheKey_t first = { 1, 1 };
	const sirShaderCacheKey_t second = { 2, 2 };
	const sirShaderCacheKey_t third = { 3, 3 };
	if ( queue.push( first, 1 ) != SIR_GRAPHICS_SUCCESS || queue.push( second, 2 ) != SIR_GRAPHICS_SUCCESS ||
	     queue.push( third, 2 ) != SIR_GRAPHICS_SUCCESS || queue.push( first, 10 ) != SIR_GRAPHICS_SUCCESS || queue.getCount() != 3 ) {
		return false;
	}
	sirShaderWarmupItem_t item = {};
	if ( !queue.tryPop( &item ) || item.key.nLow != 1 || item.nPriority != 10 ) {
		return false;
	}
	if ( !queue.tryPop( &item ) || item.key.nLow != 2 || !queue.tryPop( &item ) || item.key.nLow != 3 ) {
		return false;
	}
	if ( queue.tryPop( &item ) || queue.getCount() != 0 ) {
		return false;
	}
	for ( uint32_t iItem = 1; iItem <= queue.getCapacity(); ++iItem ) {
		const sirShaderCacheKey_t key = { iItem, iItem };
		if ( queue.push( key, iItem ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	const sirShaderCacheKey_t overflow = { 999, 999 };
	if ( queue.push( overflow, 0 ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || queue.getCount() != queue.getCapacity() ) {
		return false;
	}
	queue.clear();
	return queue.getCount() == 0 && !queue.tryPop( &item ) && queue.push( first, 0 ) == SIR_GRAPHICS_SUCCESS;
}

int main( void )
{
	if ( !testFormatsAndStages() || !testCacheKeys() || !testSelectionAndBridge() || !testWarmup() ) {
		fprintf( stderr, "SIRShaderPipelineTests failed\n" );
		return 1;
	}
	printf( "SIRShaderPipelineTests passed\n" );
	return 0;
}
