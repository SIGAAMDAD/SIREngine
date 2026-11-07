/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/SIRGraphicsModernRHI.h>
#include <stdio.h>

static uint32_t s_nCommands = 0;

class SIRModernTestCommands final : public SIRGraphicsCommandList
{
public:
    sirGraphicsQueue_t getQueue() const noexcept override { return SIR_GRAPHICS_QUEUE_DIRECT; }
    sirGraphicsResult_t reset() noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *, uint32_t ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t, uint64_t, sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override { return SIR_GRAPHICS_UNSUPPORTED; }
    sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t& ) noexcept override { return SIR_GRAPHICS_UNSUPPORTED; }
    sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t& ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t endRenderPass() noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t bindBindingSet( uint32_t, sirGraphicsHandle_t ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t bindVertexBuffer( uint32_t, sirGraphicsHandle_t, uint64_t, uint32_t ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t, uint64_t, bool ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t setViewport( const sirGraphicsViewport_t&, const sirGraphicsRect_t& ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t draw( uint32_t, uint32_t, uint32_t, uint32_t ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t drawIndexed( uint32_t, uint32_t, uint32_t, int32_t, uint32_t ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t dispatch( uint32_t, uint32_t, uint32_t ) noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
    sirGraphicsResult_t close() noexcept override { ++s_nCommands; return SIR_GRAPHICS_SUCCESS; }
};

static bool expect( sirGraphicsResult_t observed, sirGraphicsResult_t expected, const char *pszLabel ) noexcept
{
    if ( observed != expected ) {
        fprintf( stderr, "%s: expected %u, observed %u\n", pszLabel, (uint32_t)expected, (uint32_t)observed );
        return false;
    }
    return true;
}

static sirGraphicsHandle_t handle( sirGraphicsHandleType_t eType ) noexcept
{
    return { 0, 1, 7, eType };
}

int main()
{
    sirGraphicsABIPipelineModelCaps_t caps = {};
    caps.nSize = sizeof( caps );
    caps.nDeviceGeneration = 7;
    caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
    caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
    caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
    caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT | SIR_GRAPHICS_STAGE_COMPUTE;
    caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_SPIRV - 1 );
    caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE;
    caps.nMaximumSamples = 1;
    caps.nMaximumColorAttachments = 1;
    caps.nMaximumTextureDimension = 4096;
    caps.nMaximumBindings = 8;
    caps.nMaximumVertexAttributes = 8;
    caps.nMaximumVertexStreams = 4;

    sirModernVertexStream_t stream = { 0, handle( SIR_GRAPHICS_HANDLE_BUFFER ), 0, 16 };
    sirModernBinding_t binding = { 0, handle( SIR_GRAPHICS_HANDLE_BINDING_SET ) };
    sirModernDraw_t draw = {};
    draw.hPipeline = handle( SIR_GRAPHICS_HANDLE_PIPELINE );
    draw.pBindings = &binding;
    draw.nBindingCount = 1;
    draw.pStreams = &stream;
    draw.nStreamCount = 1;
    draw.nElementCount = 3;
    draw.nInstanceCount = 1;
    sirGraphicsColorAttachment_t attachment = {};
    attachment.hView = handle( SIR_GRAPHICS_HANDLE_VIEW );
    sirGraphicsRenderPass_t pass = {};
    pass.pColors = &attachment;
    pass.nColorCount = 1;
    sirGraphicsViewport_t viewport = { 0, 0, 100, 100, 0, 1 };
    sirGraphicsRect_t scissor = { 0, 0, 100, 100 };
    sirModernBatch_t batch = {};
    batch.nDeviceGeneration = 7;
    batch.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
    batch.pPass = &pass;
    batch.pViewport = &viewport;
    batch.pScissor = &scissor;
    batch.pDraws = &draw;
    batch.nDrawCount = 1;
    bool ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_SUCCESS, "baseline draw" );
    SIRModernTestCommands commands;
    if ( ok ) {
        ok = expect( SIRGraphicsModernRHI::record( batch, commands ), SIR_GRAPHICS_SUCCESS, "record draw" );
        ok = ok && s_nCommands == 9;
    }
    draw.nIndexed = 1;
    draw.hIndexBuffer = handle( SIR_GRAPHICS_HANDLE_BUFFER );
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_SUCCESS, "indexed draw" ) && ok;
    draw.hIndexBuffer.nDeviceGeneration = 8;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_INVALID_HANDLE, "stale index" ) && ok;
    draw.hIndexBuffer.nDeviceGeneration = 7;
    binding.hSet.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_INVALID_HANDLE, "wrong descriptor type" ) && ok;
    binding.hSet.eType = SIR_GRAPHICS_HANDLE_BINDING_SET;
    stream.nStrideBytes = 0;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero stride" ) && ok;
    stream.nStrideBytes = 16;
    batch.eQueue = SIR_GRAPHICS_QUEUE_COMPUTE;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_UNSUPPORTED, "unavailable queue" ) && ok;
    batch.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
    caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_SUCCESS, "stateful modern" ) && ok;
    caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
    sirModernDispatch_t dispatch = {};
    dispatch.hPipeline = handle( SIR_GRAPHICS_HANDLE_PIPELINE );
    dispatch.nX = 1;
    dispatch.nY = 2;
    dispatch.nZ = 3;
    batch.nDrawCount = 0;
    batch.pPass = nullptr;
    batch.pDispatches = &dispatch;
    batch.nDispatchCount = 1;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_SUCCESS, "dispatch" ) && ok;
    dispatch.nZ = 0;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero dispatch dimension" ) && ok;
    dispatch.nZ = 3;
    caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
    caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
    ok = expect( SIRGraphicsModernRHI::admit( batch, caps ), SIR_GRAPHICS_UNSUPPORTED, "no compute" ) && ok;
    return ok ? 0 : 1;
}
