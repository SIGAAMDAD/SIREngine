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

#include <Renderer/Visibility/SIRHiZCommands.h>
#include <stdio.h>

#define REQUIRE( check )                                                                                                                   \
	if ( !( check ) ) {                                                                                                                    \
		printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #check );                                                                          \
		return false;                                                                                                                      \
	}

typedef struct sirHiZRecordedCommand_s
{
	uint32_t nKind;
	sirGraphicsBarrier_t barriers[2];
	uint32_t nCount;
	uint32_t nX;
	uint32_t nY;
	sirGraphicsHandle_t handle;
} sirHiZRecordedCommand_t;

class SIRHiZMockCommands final : public SIRGraphicsCommandList
{
public:
	SIRHiZMockCommands( void ) noexcept = default;
	~SIRHiZMockCommands( void ) noexcept override = default;

	sirGraphicsQueue_t getQueue( void ) const noexcept override
	{
		return m_eQueue;
	}

	sirGraphicsResult_t reset( void ) noexcept override
	{
		m_nCalls = 0;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept override
	{
		if ( m_nCalls == m_nFailAt ) {
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		sirHiZRecordedCommand_t& command = m_szCalls[m_nCalls++];
		command.nKind = 1;
		command.nCount = nCount;
		for ( uint32_t i = 0; i < nCount; ++i ) {
			command.barriers[i] = pBarriers[i];
		}
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t handle ) noexcept override
	{
		if ( m_nCalls == m_nFailAt ) {
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		m_szCalls[m_nCalls++] = { 2, {}, 0, 0, 0, handle };
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t handle ) noexcept override
	{
		if ( m_nCalls == m_nFailAt ) {
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		m_szCalls[m_nCalls++] = { 3, {}, nSetIndex, 0, 0, handle };
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t dispatch( uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept override
	{
		if ( m_nCalls == m_nFailAt ) {
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		m_szCalls[m_nCalls++] = { 4, {}, nZ, nX, nY, {} };
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
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t endRenderPass( void ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t bindVertexBuffer( uint32_t, sirGraphicsHandle_t, uint64_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t, uint64_t, bool ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t&, const sirGraphicsRect_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t draw( uint32_t, uint32_t, uint32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t drawIndexed( uint32_t, uint32_t, uint32_t, int32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t close( void ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsQueue_t m_eQueue = SIR_GRAPHICS_QUEUE_COMPUTE;
	uint32_t m_nFailAt = UINT32_MAX;
	uint32_t m_nCalls = 0;
	sirHiZRecordedCommand_t m_szCalls[160] = {};

protected:
private:
};

static sirHiZCommandResources_t sirHiZTestResources( uint32_t nWidth, uint32_t nHeight, sirHiZDispatchPlan_t *pPlan,
                                                     sirGraphicsHandle_t *pSets ) noexcept
{
	sirHiZLayout_t layout = {};
	( void )sirHiZCalculateLayout( nWidth, nHeight, &layout );
	pPlan->pyramid = { nWidth,
	                   nHeight,
	                   1,
	                   1,
	                   layout.nMipCount,
	                   1,
	                   SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE,
	                   SIR_GRAPHICS_TEXTURE_2D,
	                   SIR_GRAPHICS_FORMAT_R32_FLOAT,
	                   SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	pPlan->nPassCount = layout.nMipCount;
	for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
		sirHiZDispatch_t& pass = pPlan->szPasses[iMip];
		pass.nSourceMip = iMip == 0 ? 0 : iMip - 1;
		pass.nDestinationMip = iMip;
		pass.bCopyDepth = iMip == 0;
		pass.parameters.nSourceWidth = iMip == 0 ? nWidth : layout.szMips[iMip - 1].nWidth;
		pass.parameters.nSourceHeight = iMip == 0 ? nHeight : layout.szMips[iMip - 1].nHeight;
		pass.parameters.nDestinationWidth = layout.szMips[iMip].nWidth;
		pass.parameters.nDestinationHeight = layout.szMips[iMip].nHeight;
		pass.nWorkgroupsX = layout.szMips[iMip].nWidth / 8 + ( layout.szMips[iMip].nWidth % 8 != 0 );
		pass.nWorkgroupsY = layout.szMips[iMip].nHeight / 8 + ( layout.szMips[iMip].nHeight % 8 != 0 );
		pSets[iMip] = { iMip + 10, 1, 4, SIR_GRAPHICS_HANDLE_BINDING_SET };
	}
	sirHiZCommandResources_t r = {};
	r.pPlan = pPlan;
	r.pPassSets = pSets;
	r.nPassSetCount = layout.nMipCount;
	r.hDepthTexture = { 1, 1, 4, SIR_GRAPHICS_HANDLE_TEXTURE };
	r.hPyramidTexture = { 2, 1, 4, SIR_GRAPHICS_HANDLE_TEXTURE };
	r.hCopyPipeline = { 3, 1, 4, SIR_GRAPHICS_HANDLE_PIPELINE };
	r.hReducePipeline = { 4, 1, 4, SIR_GRAPHICS_HANDLE_PIPELINE };
	r.eDepthBefore = SIR_GRAPHICS_STATE_DEPTH_WRITE;
	r.ePyramidBefore = SIR_GRAPHICS_STATE_UNDEFINED;
	return r;
}

static bool sirHiZVerifyCommands( void ) noexcept
{
	const uint32_t szWidths[] = { 1, 1, 7, 13, 1920, 2560, 3840 };
	const uint32_t szHeights[] = { 1, 17, 3, 9, 1080, 1440, 2160 };
	for ( uint32_t iCase = 0; iCase < sizeof( szWidths ) / sizeof( szWidths[0] ); ++iCase ) {
		sirHiZDispatchPlan_t plan = {};
		sirGraphicsHandle_t szSets[32] = {};
		sirHiZCommandResources_t resources = sirHiZTestResources( szWidths[iCase], szHeights[iCase], &plan, szSets );
		SIRHiZMockCommands commands;
		REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_SUCCESS );
		REQUIRE( commands.m_nCalls == plan.nPassCount * 4 + 1 );
		for ( uint32_t iMip = 0; iMip < plan.nPassCount; ++iMip ) {
			const sirHiZRecordedCommand_t& transition = commands.m_szCalls[iMip * 4];
			REQUIRE( transition.nKind == 1 && transition.nCount == 2 );
			REQUIRE( transition.barriers[0].nBaseMip == ( iMip == 0 ? 0 : iMip - 1 ) );
			REQUIRE( transition.barriers[0].eBefore == ( iMip == 0 ? SIR_GRAPHICS_STATE_DEPTH_WRITE : SIR_GRAPHICS_STATE_SHADER_WRITE ) );
			REQUIRE( transition.barriers[0].eAfter == SIR_GRAPHICS_STATE_SHADER_READ );
			REQUIRE( transition.barriers[1].eBefore == SIR_GRAPHICS_STATE_UNDEFINED );
			REQUIRE( transition.barriers[1].eAfter == SIR_GRAPHICS_STATE_SHADER_WRITE );
			REQUIRE( transition.barriers[1].nBaseMip == iMip && transition.barriers[1].nMipCount == 1 );
			REQUIRE( commands.m_szCalls[iMip * 4 + 1].nKind == 2 );
			REQUIRE( commands.m_szCalls[iMip * 4 + 1].handle.nSlot == ( iMip == 0 ? 3U : 4U ) );
			REQUIRE( commands.m_szCalls[iMip * 4 + 2].nKind == 3 );
			REQUIRE( commands.m_szCalls[iMip * 4 + 2].handle.nSlot == iMip + 10 );
			REQUIRE( commands.m_szCalls[iMip * 4 + 3].nKind == 4 );
			REQUIRE( commands.m_szCalls[iMip * 4 + 3].nX == plan.szPasses[iMip].nWorkgroupsX );
			REQUIRE( commands.m_szCalls[iMip * 4 + 3].nY == plan.szPasses[iMip].nWorkgroupsY );
		}
		REQUIRE( commands.m_szCalls[plan.nPassCount * 4].nCount == 1 );
		REQUIRE( commands.m_szCalls[plan.nPassCount * 4].barriers[0].nBaseMip == plan.nPassCount - 1 );
	}
	return true;
}

static bool sirHiZVerifyRejections( void ) noexcept
{
	sirHiZDispatchPlan_t plan = {};
	sirGraphicsHandle_t szSets[32] = {};
	sirHiZCommandResources_t resources = sirHiZTestResources( 13, 9, &plan, szSets );
	SIRHiZMockCommands commands;
	resources.nPassSetCount = 1;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	resources.nPassSetCount = plan.nPassCount;
	resources.pPassSets = nullptr;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	resources.pPassSets = szSets;
	resources.hCopyPipeline.nDeviceGeneration = 5;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_HANDLE );
	resources.hCopyPipeline.nDeviceGeneration = 4;
	resources.hDepthTexture = resources.hPyramidTexture;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_HANDLE );
	resources.hDepthTexture = { 1, 1, 4, SIR_GRAPHICS_HANDLE_TEXTURE };
	szSets[3].nGeneration = 0;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_HANDLE );
	szSets[3].nGeneration = 1;
	plan.szPasses[3].parameters.nReserved1 = 1;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	plan.szPasses[3].parameters.nReserved1 = 0;
	plan.szPasses[0].parameters.nSourceOriginX = UINT32_MAX;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	plan.szPasses[0].parameters.nSourceOriginX = 0;
	plan.szPasses[2].nWorkgroupsY++;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	plan.szPasses[2].nWorkgroupsY--;
	resources.ePyramidBefore = SIR_GRAPHICS_STATE_SHADER_WRITE;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	resources.ePyramidBefore = SIR_GRAPHICS_STATE_SHADER_READ;
	resources.eDepthBefore = SIR_GRAPHICS_STATE_SHADER_READ;
	commands.m_eQueue = SIR_GRAPHICS_QUEUE_COPY;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_UNSUPPORTED );
	commands.m_eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( commands.m_szCalls[0].nCount == 1 );
	REQUIRE( commands.m_szCalls[0].barriers[0].hResource.nSlot == resources.hPyramidTexture.nSlot );
	REQUIRE( commands.m_szCalls[1].nKind == 2 );
	REQUIRE( commands.m_nCalls == plan.nPassCount * 4 + 1 );
	return true;
}

static bool sirHiZVerifyDriverFailure( void ) noexcept
{
	sirHiZDispatchPlan_t plan = {};
	sirGraphicsHandle_t szSets[32] = {};
	const sirHiZCommandResources_t resources = sirHiZTestResources( 13, 9, &plan, szSets );
	for ( uint32_t iFail = 0; iFail < plan.nPassCount * 4 + 1; ++iFail ) {
		SIRHiZMockCommands commands;
		commands.m_nFailAt = iFail;
		REQUIRE( sirHiZRecordPyramid( commands, resources ) == SIR_GRAPHICS_DRIVER_ERROR );
		REQUIRE( commands.m_nCalls == iFail );
	}
	return true;
}

int main( void )
{
	if ( !sirHiZVerifyCommands() || !sirHiZVerifyRejections() || !sirHiZVerifyDriverFailure() ) {
		return 1;
	}
	puts( "PASS: Hi-Z command sequencing and preflight failure isolation" );
	return 0;
}
