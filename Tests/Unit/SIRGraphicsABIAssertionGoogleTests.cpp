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
#include <gtest/gtest.h>
#include <limits>

TEST( GraphicsABI, VersionAndLayout )
{
	EXPECT_EQ( SIR_GRAPHICS_DRIVER_ABI_MAJOR, 1U );
	EXPECT_EQ( SIR_GRAPHICS_DRIVER_ABI_MINOR, 0U );
	EXPECT_EQ( sizeof( sirGraphicsHandle_t ), 16U );
	EXPECT_EQ( sizeof( sirGraphicsViewDesc_t ), 56U );
}

TEST( GraphicsABI, NullRecords )
{
	EXPECT_EQ( sirGraphicsValidateABIHost( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABIDriver( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABIAdapter( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABICapabilities( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABIFormat( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABIPipelineModelCaps( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABIDevice( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( sirGraphicsValidateABICommandList( nullptr ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, UnknownAdapterFeatures )
{
	sirGraphicsABIAdapter_t adapter = {};
	adapter.nSize = sizeof( adapter );
	adapter.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	adapter.nKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	adapter.nApiMajor = 1;
	adapter.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	adapter.nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	EXPECT_EQ( sirGraphicsValidateABIAdapter( &adapter ), SIR_GRAPHICS_SUCCESS );
	adapter.nFeatureMask |= 1ULL << 63;
	EXPECT_EQ( sirGraphicsValidateABIAdapter( &adapter ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, DeviceCapabilities )
{
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 1;
	caps.nBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	caps.nAdapterKind = SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE;
	caps.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nEnabledFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_MODERN - 1 );
	caps.nMaximumFramesInFlight = 2;
	caps.nMaximumSamples = 1;
	EXPECT_EQ( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_SUCCESS );
	caps.nEnabledQueues = 0;
	EXPECT_EQ( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	caps.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nPipelineModelMask = 0;
	EXPECT_EQ( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, StaleGeneration )
{
	sirGraphicsHandle_t handle = { 0, 1, 7, SIR_GRAPHICS_HANDLE_TEXTURE };
	EXPECT_TRUE( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_TEXTURE, 7 ) );
	EXPECT_FALSE( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, 7 ) );
	EXPECT_FALSE( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_TEXTURE, 8 ) );
	handle.nGeneration = 0;
	EXPECT_FALSE( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_TEXTURE, 7 ) );
}

TEST( GraphicsABI, RetirementFenceQueueMask )
{
	sirGraphicsCompletionSet_t completion = {};
	completion.nDeviceGeneration = 7;
	completion.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 3;
	EXPECT_TRUE( sirGraphicsIsCompletionSetValid( completion, 7 ) );
	completion.nQueueMask = 0;
	EXPECT_FALSE( sirGraphicsIsCompletionSetValid( completion, 7 ) );
}

TEST( GraphicsABI, UnsupportedImageFormatFlags )
{
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	format.nSampleCountMask = 1;
	format.nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
	EXPECT_EQ( sirGraphicsValidateABIFormat( &format ), SIR_GRAPHICS_SUCCESS );
	format.nOptimalFeatures |= 1U << 15;
	EXPECT_EQ( sirGraphicsValidateABIFormat( &format ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, BufferAlignment )
{
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = 64;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	desc.nAlignmentBytes = 64;
	EXPECT_EQ( sirGraphicsValidateBuffer( desc ), SIR_GRAPHICS_SUCCESS );
	desc.nAlignmentBytes = 24;
	EXPECT_EQ( sirGraphicsValidateBuffer( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, TextureSamples )
{
	sirGraphicsTextureDesc_t desc = {};
	desc.nWidth = 64;
	desc.nHeight = 64;
	desc.nDepth = 1;
	desc.nArrayLayers = 1;
	desc.nMipLevels = 1;
	desc.nSampleCount = 1;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_SUCCESS );
	desc.nSampleCount = 3;
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, SamplerNaN )
{
	sirGraphicsSamplerDesc_t desc = {};
	desc.fMaximumAnisotropy = 1.0f;
	desc.fMinimumLod = 0.0f;
	desc.fMaximumLod = 10.0f;
	EXPECT_EQ( sirGraphicsValidateSampler( desc ), SIR_GRAPHICS_SUCCESS );
	desc.fMaximumAnisotropy = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ( sirGraphicsValidateSampler( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, ShaderlessFixedFunction )
{
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.nSampleCount = 1;
	pipeline.pFixed = &fixed;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &pipeline ), SIR_GRAPHICS_SUCCESS );
	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, MutuallyExclusiveRecipeTags )
{
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.nSampleCount = 1;
	pipeline.pFixed = &fixed;
	pipeline.pModern = &modern;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
}
