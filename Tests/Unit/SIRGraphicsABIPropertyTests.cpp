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

#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <stdint.h>
#include <stdio.h>
#include <limits>

static constexpr uint64_t RANDOM_SEED = 0x91A2C35B49E5D607ULL;
static uint64_t s_nRandom = RANDOM_SEED;
static uint32_t s_nCases = 0;

static uint32_t nextRandom( void ) noexcept
{
	s_nRandom ^= s_nRandom >> 12;
	s_nRandom ^= s_nRandom << 25;
	s_nRandom ^= s_nRandom >> 27;
	return ( uint32_t )( ( s_nRandom * 2685821657736338717ULL ) >> 32 );
}

static bool expect( bool bPass, const char *pszCase, uint32_t iIteration ) noexcept
{
	++s_nCases;
	if ( !bPass ) {
		fprintf( stderr, "%s: case=%u seed=0x%llx\n", pszCase, iIteration, ( unsigned long long )RANDOM_SEED );
		return false;
	}
	return true;
}

static bool testBufferMutations( uint32_t iIteration ) noexcept
{
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = ( uint64_t )( nextRandom() % 65536 ) + 1;
	buffer.nAlignmentBytes = 1ULL << ( nextRandom() % 13 );
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	if ( !expect( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_SUCCESS, "valid buffer", iIteration ) ) {
		return false;
	}
	buffer.nSizeBytes = 0;
	if ( !expect( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero buffer", iIteration ) ) {
		return false;
	}
	buffer.nSizeBytes = 1;
	buffer.nAlignmentBytes = 3;
	if ( !expect( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_INVALID_ARGUMENT, "alignment", iIteration ) ) {
		return false;
	}
	buffer.nAlignmentBytes = 1;
	buffer.nUsage = 0;
	if ( !expect( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero usage", iIteration ) ) {
		return false;
	}
	buffer.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	if ( !expect( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_INVALID_ARGUMENT, "attachment buffer", iIteration ) ) {
		return false;
	}
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = ( sirGraphicsMemoryClass_t )( SIR_GRAPHICS_MEMORY_TRANSIENT + 1 );
	return expect( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_INVALID_ARGUMENT, "invalid memory class", iIteration );
}

static bool testTextureMutations( uint32_t iIteration ) noexcept
{
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 1 + ( nextRandom() % 2048 );
	texture.nHeight = 1 + ( nextRandom() % 2048 );
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	if ( !expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_SUCCESS, "valid 2D texture", iIteration ) ) {
		return false;
	}
	texture.nDepth = 2;
	if ( !expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "2D nonunit depth", iIteration ) ) {
		return false;
	}
	texture.nDepth = 1;
	texture.nSampleCount = 3;
	if ( !expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "nonpower samples", iIteration ) ) {
		return false;
	}
	texture.nSampleCount = 1;
	texture.nMipLevels = 33;
	if ( !expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "mip overflow", iIteration ) ) {
		return false;
	}
	texture.nMipLevels = 1;
	texture.eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	if ( !expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "unknown format", iIteration ) ) {
		return false;
	}
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
	texture.nArrayLayers = 6;
	texture.nWidth = 3;
	texture.nHeight = 4;
	if ( !expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "nonsquare cube", iIteration ) ) {
		return false;
	}
	texture.nWidth = 4;
	texture.nHeight = 4;
	if ( !expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_SUCCESS, "valid cube", iIteration ) ) {
		return false;
	}
	texture.nArrayLayers = 5;
	return expect( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "partial cube array", iIteration );
}

static bool testSamplerMutations( uint32_t iIteration ) noexcept
{
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.eFilter = SIR_GRAPHICS_FILTER_LINEAR;
	sampler.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
	sampler.eAddressW = SIR_GRAPHICS_ADDRESS_BORDER;
	sampler.fMaximumAnisotropy = ( float )( 1 + nextRandom() % 16 );
	sampler.fMinimumLod = 0.0f;
	sampler.fMaximumLod = ( float )( nextRandom() % 16 );
	if ( !expect( sirGraphicsValidateSampler( sampler ) == SIR_GRAPHICS_SUCCESS, "valid sampler", iIteration ) ) {
		return false;
	}
	sampler.fMaximumAnisotropy = std::numeric_limits<float>::quiet_NaN();
	if ( !expect( sirGraphicsValidateSampler( sampler ) == SIR_GRAPHICS_INVALID_ARGUMENT, "nan anisotropy", iIteration ) ) {
		return false;
	}
	sampler.fMaximumAnisotropy = 1.0f;
	sampler.fMinimumLod = std::numeric_limits<float>::quiet_NaN();
	if ( !expect( sirGraphicsValidateSampler( sampler ) == SIR_GRAPHICS_INVALID_ARGUMENT, "nan lod", iIteration ) ) {
		return false;
	}
	sampler.fMinimumLod = sampler.fMaximumLod + 1.0f;
	return expect( sirGraphicsValidateSampler( sampler ) == SIR_GRAPHICS_INVALID_ARGUMENT, "inverted lod", iIteration );
}

static bool testGenerationMutations( uint32_t iIteration ) noexcept
{
	const uint32_t nGeneration = 1 + ( nextRandom() % 65536 );
	sirGraphicsHandle_t handle = { nextRandom(), 1, nGeneration, SIR_GRAPHICS_HANDLE_BUFFER };
	if ( !expect( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, nGeneration ), "valid handle", iIteration ) ) {
		return false;
	}
	handle.nDeviceGeneration = nGeneration + 1;
	if ( !expect( !sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, nGeneration ), "stale handle", iIteration ) ) {
		return false;
	}
	handle.nDeviceGeneration = nGeneration;
	handle.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	if ( !expect( !sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, nGeneration ), "wrong handle type", iIteration ) ) {
		return false;
	}
	sirGraphicsCompletionSet_t set = {};
	set.nDeviceGeneration = nGeneration;
	set.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	set.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 1ULL + nextRandom();
	if ( !expect( sirGraphicsIsCompletionSetValid( set, nGeneration ), "valid retirement", iIteration ) ) {
		return false;
	}
	set.nQueueMask = 0;
	if ( !expect( !sirGraphicsIsCompletionSetValid( set, nGeneration ), "orphan retirement", iIteration ) ) {
		return false;
	}
	set.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	set.nDeviceGeneration = nGeneration + 1;
	return expect( !sirGraphicsIsCompletionSetValid( set, nGeneration ), "stale retirement", iIteration );
}

int main( void )
{
	for ( uint32_t i = 0; i < 4096; ++i ) {
		if ( !testBufferMutations( i ) || !testTextureMutations( i ) ||
		     !testSamplerMutations( i ) || !testGenerationMutations( i ) ) {
			return 1;
		}
	}
	printf( "PASS: %u deterministic property assertions, seed=0x%llx\n", s_nCases,
	        ( unsigned long long )RANDOM_SEED );
	return 0;
}
