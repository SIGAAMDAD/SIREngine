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
	caps.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_MODERN - 1 );
	caps.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE;
	EXPECT_EQ( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	caps.nEnabledQueues |= 1U << SIR_GRAPHICS_QUEUE_COMPUTE;
	EXPECT_EQ( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_SUCCESS );
	caps.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_COPY_QUEUE;
	EXPECT_EQ( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	caps.nEnabledQueues |= 1U << SIR_GRAPHICS_QUEUE_COPY;
	EXPECT_EQ( sirGraphicsValidateABICapabilities( &caps ), SIR_GRAPHICS_SUCCESS );
	caps.nEnabledFeatures |= SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_COMPUTE;
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
	handle.nGeneration = 1;
	EXPECT_FALSE( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_TEXTURE, 0 ) );
	handle.nDeviceGeneration = 0;
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
	completion.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	EXPECT_FALSE( sirGraphicsIsCompletionSetValid( completion, 0 ) );
	completion.nDeviceGeneration = 0;
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
	desc.nSampleCount = 128;
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nSampleCount = 1;
	desc.nArrayLayers = 0;
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nArrayLayers = 1;
	desc.nMipLevels = 33;
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nMipLevels = 1;
	desc.eFormat = ( sirGraphicsFormat_t )( SIR_GRAPHICS_FORMAT_BC7_UNORM + 1 );
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_1D;
	desc.nHeight = 2;
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nHeight = 1;
	EXPECT_EQ( sirGraphicsValidateTexture( desc ), SIR_GRAPHICS_SUCCESS );
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
	pipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.pFixed = nullptr;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	pipeline.pFixed = &fixed;
	pipeline.nBindingCount = 1;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	pipeline.nBindingCount = 0;
	sirGraphicsABIProgrammableRecipe_t programmable = {};
	programmable.nSize = sizeof( programmable );
	pipeline.pProgrammable = &programmable;
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

// Exercise each operand of descriptor conditions independently. The public
// contract rejects malformed models before querying any native driver.
TEST( GraphicsABI, PipelineStructureCompoundErrorBranches )
{
	uint32_t nShader = 0x07230203;
	sirGraphicsABIShaderCode_t shader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 1, "main" };
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	sirGraphicsABIProgrammableRecipe_t programmable = {};
	programmable.nSize = sizeof( programmable );
	programmable.nVertexShaderProfile = 1;
	programmable.nFragmentShaderProfile = 1;
	sirGraphicsBindingDesc_t binding = {};

	sirGraphicsABIPipeline_t fixedPipeline = {};
	fixedPipeline.nSize = sizeof( fixedPipeline );
	fixedPipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	fixedPipeline.nType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	fixedPipeline.nSampleCount = 1;
	fixedPipeline.pFixed = &fixed;
	ASSERT_EQ( sirGraphicsValidateABIPipelineStructure( &fixedPipeline ), SIR_GRAPHICS_SUCCESS );
	fixedPipeline.nBindingCount = 1;
	fixedPipeline.pBindings = &binding;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &fixedPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	fixedPipeline.nBindingCount = 0;
	fixedPipeline.vertexShader = shader;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &fixedPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	fixedPipeline.vertexShader = {};
	fixedPipeline.pixelShader = shader;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &fixedPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	fixedPipeline.pixelShader = {};
	fixedPipeline.computeShader = shader;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &fixedPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	fixedPipeline.computeShader = {};
	fixedPipeline.pModern = &modern;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &fixedPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	fixedPipeline.pModern = nullptr;
	fixedPipeline.pProgrammable = &programmable;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &fixedPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );

	sirGraphicsABIPipeline_t legacy = {};
	legacy.nSize = sizeof( legacy );
	legacy.nModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	legacy.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	legacy.nSampleCount = 1;
	legacy.vertexShader = shader;
	legacy.pixelShader = shader;
	legacy.pProgrammable = &programmable;
	ASSERT_EQ( sirGraphicsValidateABIPipelineStructure( &legacy ), SIR_GRAPHICS_SUCCESS );
	legacy.vertexShader = {};
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &legacy ), SIR_GRAPHICS_INVALID_ARGUMENT );
	legacy.vertexShader = shader;
	legacy.pixelShader = {};
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &legacy ), SIR_GRAPHICS_INVALID_ARGUMENT );
	legacy.pixelShader = shader;
	legacy.computeShader = shader;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &legacy ), SIR_GRAPHICS_INVALID_ARGUMENT );
	legacy.computeShader = {};
	legacy.pModern = &modern;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &legacy ), SIR_GRAPHICS_INVALID_ARGUMENT );
	legacy.pModern = nullptr;
	legacy.pFixed = &fixed;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &legacy ), SIR_GRAPHICS_INVALID_ARGUMENT );
	legacy.pFixed = nullptr;
	legacy.pProgrammable = nullptr;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &legacy ), SIR_GRAPHICS_INVALID_ARGUMENT );

	sirGraphicsABIPipeline_t modernPipeline = legacy;
	modernPipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	modernPipeline.pModern = &modern;
	ASSERT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_SUCCESS );
	modernPipeline.pProgrammable = &programmable;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.pProgrammable = nullptr;
	modernPipeline.pFixed = &fixed;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.pFixed = nullptr;
	modernPipeline.vertexShader = {};
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.vertexShader = shader;
	modernPipeline.pixelShader = {};
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.pixelShader = shader;
	modernPipeline.computeShader = shader;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.vertexShader = {};
	modernPipeline.pixelShader = {};
	modernPipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	ASSERT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_SUCCESS );
	modernPipeline.vertexShader = shader;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.vertexShader = {};
	modernPipeline.pixelShader = shader;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.pixelShader = {};
	modernPipeline.computeShader = {};
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	modernPipeline.computeShader = shader;
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX;
	EXPECT_EQ( sirGraphicsValidateABIPipelineStructure( &modernPipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, SourceTextureAndPipelineCompoundErrorBranches )
{
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 32;
	texture.nHeight = 1;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_1D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	ASSERT_EQ( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_SUCCESS );
	texture.nDepth = 2;
	EXPECT_EQ( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT );
	texture.nDepth = 1;
	texture.nMipLevels = 0;
	EXPECT_EQ( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT );
	texture.nMipLevels = 1;
	texture.nSampleCount = 0;
	EXPECT_EQ( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT );

	uint32_t nShader = 0x07230203;
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	sirGraphicsABIProgrammableRecipe_t programmable = {};
	programmable.nSize = sizeof( programmable );
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	sirGraphicsPipelineDesc_t desc = {};
	desc.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	desc.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	desc.nSampleCount = 1;
	desc.pModern = &modern;
	desc.vertexShader = { SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 1, "main" };
	desc.pixelShader = desc.vertexShader;
	ASSERT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_SUCCESS );
	desc.eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	desc.pModern = nullptr;
	desc.pProgrammable = &programmable;
	ASSERT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_SUCCESS );
	desc.pModern = &modern;
	EXPECT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pModern = nullptr;
	desc.pFixed = &fixed;
	EXPECT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pFixed = nullptr;
	desc.eModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	desc.eType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	desc.vertexShader = {};
	desc.pixelShader = {};
	desc.pProgrammable = nullptr;
	desc.pFixed = &fixed;
	ASSERT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_SUCCESS );
	desc.pModern = &modern;
	EXPECT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pModern = nullptr;
	desc.pProgrammable = &programmable;
	EXPECT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pProgrammable = nullptr;
	sirGraphicsBindingDesc_t binding = {};
	desc.nBindingCount = 1;
	desc.pBindings = &binding;
	EXPECT_EQ( sirGraphicsValidatePipeline( desc ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, FixedModelCapsShaderMaskBranches )
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 1;
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	caps.nMaximumSamples = 1;
	caps.nMaximumTextureDimension = 1024;
	caps.nMaximumTextureUnits = 1;
	caps.nMaximumTextureStages = 1;
	caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM;
	caps.nFixedTextureOperationMask = 1U << SIR_GRAPHICS_FIXED_OP_SELECT;
	caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_NONE;
	ASSERT_EQ( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS );
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	EXPECT_EQ( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, ShaderModelCapabilityMaskIndependence )
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 1;
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	caps.nMaximumSamples = 1;
	caps.nMaximumTextureDimension = 1024;
	ASSERT_EQ( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_SUCCESS );
	caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM;
	EXPECT_EQ( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	caps.nFixedFeatureMask = 0;
	caps.nFixedTextureOperationMask = 1U << SIR_GRAPHICS_FIXED_OP_SELECT;
	EXPECT_EQ( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	caps.nFixedTextureOperationMask = 0;
	caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_NONE;
	EXPECT_EQ( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	caps.nFixedFogModeMask = 0;
	caps.nMaximumFixedLights = 1;
	EXPECT_EQ( sirGraphicsValidateABIPipelineModelCaps( &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, SubmissionWaitPreflightBeforeCommandDereference )
{
	SIRGraphicsCommandList *pInvalidList = nullptr;
	sirGraphicsSubmission_t submission = {};
	submission.nCommandListCount = 1;
	submission.ppCommandLists = &pInvalidList;
	submission.nWaitCount = 65;
	EXPECT_EQ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, submission, 1 ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, SourceShaderNullByteTriples )
{
	sirGraphicsABIFixedRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	sirGraphicsPipelineDesc_t pipeline = {};
	pipeline.eType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.eModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.pFixed = &recipe;
	pipeline.nSampleCount = 1;
	ASSERT_EQ( sirGraphicsValidatePipeline( pipeline ), SIR_GRAPHICS_SUCCESS );
	pipeline.pixelShader.nByteCount = 4;
	EXPECT_EQ( sirGraphicsValidatePipeline( pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	pipeline.pixelShader = {};
	pipeline.computeShader.nContentHash = 1;
	EXPECT_EQ( sirGraphicsValidatePipeline( pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
	pipeline.computeShader = {};
	uint32_t nWord = 0;
	pipeline.vertexShader.pBytes = &nWord;
	EXPECT_EQ( sirGraphicsValidatePipeline( pipeline ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( GraphicsABI, ModernShaderAdmissionIndependentFailures )
{
	uint32_t nShader = 0x07230203;
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 1;
	caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE;
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
	caps.nMaximumSamples = 4;
	caps.nMaximumColorAttachments = 4;
	caps.nMaximumTextureDimension = 1024;
	caps.nMaximumBindings = 4;
	caps.nMaximumVertexAttributes = 1;
	caps.nMaximumVertexStreams = 1;
	sirGraphicsABIModernRecipe_t recipe = {};
	recipe.nSize = sizeof( recipe );
	recipe.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	recipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	sirGraphicsABIPipeline_t pipeline = {};
	pipeline.nSize = sizeof( pipeline );
	pipeline.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.nType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.vertexShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 1, "main" };
	pipeline.pixelShader = pipeline.vertexShader;
	pipeline.pModern = &recipe;
	ASSERT_EQ( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS );

	pipeline.vertexShader.nSize += 4;
	EXPECT_EQ( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED );
	pipeline.vertexShader.nSize -= 4;
	pipeline.nDepthFormat = SIR_GRAPHICS_FORMAT_BC7_UNORM;
	EXPECT_EQ( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	pipeline.nDepthFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	sirGraphicsVertexAttribute_t attribute = {};
	attribute.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	pipeline.nAttributeCount = 2;
	pipeline.pAttributes = &attribute;
	EXPECT_EQ( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED );
	pipeline.nAttributeCount = 0;
	pipeline.pAttributes = nullptr;

	pipeline.nType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.nColorFormatCount = 0;
	pipeline.vertexShader = {};
	pipeline.pixelShader = {};
	pipeline.computeShader = { sizeof( sirGraphicsABIShaderCode_t ), SIR_GRAPHICS_SHADER_SPIRV, &nShader, 4, 2, "main" };
	recipe.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	recipe.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	ASSERT_EQ( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_SUCCESS );
	caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_DXIL - 1 );
	EXPECT_EQ( sirGraphicsValidateABIPipeline( &pipeline, &caps ), SIR_GRAPHICS_UNSUPPORTED );
}
