/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/SIRGraphicsEraRHI.h>
#include <stdio.h>

static uint32_t s_nCommands = 0;

class SIREraTestCommands final : public SIRGraphicsCommandList
{
public:
	sirGraphicsQueue_t getQueue() const noexcept override
	{
		return SIR_GRAPHICS_QUEUE_DIRECT;
	}
	sirGraphicsResult_t reset() noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *, uint32_t ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t, uint64_t, sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t& ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t endRenderPass() noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindBindingSet( uint32_t, sirGraphicsHandle_t ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindVertexBuffer( uint32_t, sirGraphicsHandle_t, uint64_t, uint32_t ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t, uint64_t, bool ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t&, const sirGraphicsRect_t& ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t draw( uint32_t, uint32_t, uint32_t, uint32_t ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t drawIndexed( uint32_t, uint32_t, uint32_t, int32_t, uint32_t ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t dispatch( uint32_t, uint32_t, uint32_t ) noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t close() noexcept override
	{
		++s_nCommands;
		return SIR_GRAPHICS_SUCCESS;
	}
};

static bool expect( sirGraphicsResult_t observed, sirGraphicsResult_t expected, const char *pszLabel ) noexcept
{
	if ( observed != expected ) {
		fprintf( stderr, "%s: expected %u, observed %u\n", pszLabel, ( uint32_t )expected, ( uint32_t )observed );
		return false;
	}
	return true;
}

static sirGraphicsHandle_t handle( sirGraphicsHandleType_t eType ) noexcept
{
	return { 0, 1, 7, eType };
}

static sirGraphicsABIPipelineModelCaps_t makeCaps( uint32_t nModel ) noexcept
{
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	caps.nDeviceGeneration = 7;
	caps.nModel = nModel;
	caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL;
	caps.nQueueMask = 1;
	caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	caps.nMaximumSamples = 1;
	caps.nMaximumColorAttachments = 1;
	caps.nMaximumTextureDimension = 4096;
	caps.nMaximumTextureUnits = 4;
	caps.nMaximumVertexStreams = 2;
	if ( nModel == SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		caps.nRequiredFeatureMask |= SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
		caps.nMaximumTextureStages = 2;
		caps.nFixedFeatureMask = SIR_GRAPHICS_FIXED_TRANSFORM;
		caps.nFixedTextureOperationMask = 1U << SIR_GRAPHICS_FIXED_OP_SELECT;
		caps.nFixedFogModeMask = 1U << SIR_GRAPHICS_FIXED_FOG_NONE;
	} else {
		caps.nShaderStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
		caps.nShaderFormatMask = 1U << ( SIR_GRAPHICS_SHADER_DXBC - 1 );
		caps.nMaximumVertexShaderProfile = 3;
		caps.nMaximumFragmentShaderProfile = 3;
		caps.nMaximumVertexAttributes = 8;
		caps.nMaximumBindings = 4;
	}
	return caps;
}

int main()
{
	bool ok = true;
	sirGraphicsEraStream_t stream = { 0, handle( SIR_GRAPHICS_HANDLE_BUFFER ), 0, 16 };
	sirGraphicsEraDraw_t draw = {};
	draw.hPipeline = handle( SIR_GRAPHICS_HANDLE_PIPELINE );
	draw.pStreams = &stream;
	draw.nStreamCount = 1;
	draw.nElementCount = 3;
	sirGraphicsColorAttachment_t color = {};
	color.hView = handle( SIR_GRAPHICS_HANDLE_VIEW );
	sirGraphicsRenderPass_t pass = {};
	pass.pColors = &color;
	pass.nColorCount = 1;
	sirGraphicsViewport_t vp = { 0, 0, 32, 32, 0, 1 };
	sirGraphicsRect_t scissor = { 0, 0, 32, 32 };
	sirGraphicsEraBatch_t batch = {};
	batch.nDeviceGeneration = 7;
	batch.eModel = SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE;
	batch.pPass = &pass;
	batch.pViewport = &vp;
	batch.pScissor = &scissor;
	batch.pDraws = &draw;
	batch.nDrawCount = 1;
	sirGraphicsABIPipelineModelCaps_t caps = makeCaps( batch.eModel );
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_SUCCESS, "programmable draw" ) && ok;
	SIREraTestCommands commands;
	ok = expect( SIRGraphicsEraRHI::record( batch, commands, caps ), SIR_GRAPHICS_SUCCESS, "record programmable" ) && ok;
	const uint32_t nRecorded = s_nCommands;
	stream.hBuffer.nDeviceGeneration = 9;
	ok = expect( SIRGraphicsEraRHI::record( batch, commands, caps ), SIR_GRAPHICS_INVALID_HANDLE, "reject stale without recording" ) && ok;
	ok = ( nRecorded == s_nCommands ) && ok;
	stream.hBuffer.nDeviceGeneration = 7;
	draw.nStreamCount = 0;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "missing vertex input" ) && ok;
	draw.nStreamCount = 1;
	draw.nFirstElement = UINT32_MAX;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "draw range overflow" ) && ok;
	draw.nFirstElement = 0;
	sirGraphicsEraBinding_t b0 = { 0, handle( SIR_GRAPHICS_HANDLE_BINDING_SET ) };
	sirGraphicsEraBinding_t b1 = { 0, handle( SIR_GRAPHICS_HANDLE_BINDING_SET ) };
	sirGraphicsEraBinding_t bindings[] = { b0, b1 };
	draw.pBindings = bindings;
	draw.nBindingCount = 2;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "duplicate bindings" ) && ok;
	draw.nBindingCount = 1;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_SUCCESS, "single binding" ) && ok;
	batch.eModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	caps = makeCaps( batch.eModel );
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_UNSUPPORTED, "fixed cannot bind descriptor" ) && ok;
	draw.nBindingCount = 0;
	draw.pBindings = nullptr;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_SUCCESS, "fixed draw" ) && ok;
	ok = expect( SIRGraphicsEraRHI::record( batch, commands, caps ), SIR_GRAPHICS_SUCCESS, "record fixed" ) && ok;
	draw.nIndexed = 1;
	draw.hIndexBuffer = handle( SIR_GRAPHICS_HANDLE_BUFFER );
	ok = expect( SIRGraphicsEraRHI::record( batch, commands, caps ), SIR_GRAPHICS_SUCCESS, "indexed fixed" ) && ok;
	draw.nIndexOffsetBytes = 1;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_INVALID_ARGUMENT, "index alignment" ) && ok;
	draw.nIndexOffsetBytes = 0;
	draw.nVertexOffset = 1;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_UNSUPPORTED, "unqualified base vertex" ) && ok;
	draw.nVertexOffset = 0;
	draw.hIndexBuffer.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_INVALID_HANDLE, "invalid index" ) && ok;
	draw.hIndexBuffer.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	batch.nDrawCount = 0;
	ok = expect( SIRGraphicsEraRHI::record( batch, commands, caps ), SIR_GRAPHICS_SUCCESS, "clear-only fixed" ) && ok;
	caps = makeCaps( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE );
	ok = expect( SIRGraphicsEraRHI::admitBatch( batch, caps ), SIR_GRAPHICS_UNSUPPORTED, "wrong model" ) && ok;
	return ok ? 0 : 1;
}
