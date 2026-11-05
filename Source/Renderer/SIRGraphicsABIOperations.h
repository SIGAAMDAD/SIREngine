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

#ifndef __SIRENGINE_SIRGRAPHICSABIOPERATIONS_H__
#define __SIRENGINE_SIRGRAPHICSABIOPERATIONS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

// Fixed-width boundary mirrors only for source structures containing C++ bool.
// All pointers are borrowed for the duration of the call unless specified.
typedef struct sirGraphicsABIShaderCode_s
{
	uint32_t nSize;
	uint32_t nFormat;
	const void *pBytes;
	uint64_t nByteCount;
	uint64_t nContentHash;
	const char *pszEntryPoint;
} sirGraphicsABIShaderCode_t;

typedef struct sirGraphicsABIBlendState_s
{
	uint32_t nSourceColor;
	uint32_t nDestinationColor;
	uint32_t nColorOperation;
	uint32_t nSourceAlpha;
	uint32_t nDestinationAlpha;
	uint32_t nAlphaOperation;
	uint32_t nColorWriteMask;
	uint32_t nEnabled;
} sirGraphicsABIBlendState_t;

typedef struct sirGraphicsABIPipelineModelCaps_s
{
	uint32_t nSize;
	uint32_t nDeviceGeneration;
	uint32_t nModel;
	uint32_t nExecutionModel;
	uint32_t nShaderStageMask;
	uint32_t nQueueMask;
	uint64_t nRequiredFeatureMask;
	uint32_t nShaderFormatMask;
	uint32_t nMaximumTextureUnits;
	uint32_t nMaximumTextureStages;
	uint32_t nMaximumFixedLights;
	uint32_t nMaximumVertexStreams;
	uint32_t nMaximumVertexAttributes;
	uint32_t nMaximumVaryings;
	uint32_t nMaximumConstantSlots;
	uint32_t nMaximumBindings;
	uint32_t nMaximumColorAttachments;
	uint32_t nMaximumSamples;
	uint32_t nMaximumTextureDimension;
	uint32_t nMaximumVertexShaderProfile;
	uint32_t nMaximumFragmentShaderProfile;
	uint32_t nFixedFeatureMask;
	uint32_t nFixedTextureOperationMask;
	uint32_t nFixedFogModeMask;
	uint32_t nSupportsNPOT;
	uint32_t nReserved;
} sirGraphicsABIPipelineModelCaps_t;

typedef struct sirGraphicsABIModernRecipe_s
{
	uint32_t nSize;
	uint32_t nStageMask;
	uint64_t nRequiredFeatures;
	uint64_t nBindingLayoutHash;
	uint64_t nCookedVariantHash;
} sirGraphicsABIModernRecipe_t;

typedef struct sirGraphicsABIProgrammableRecipe_s
{
	uint32_t nSize;
	uint32_t nVertexShaderProfile;
	uint32_t nFragmentShaderProfile;
	uint32_t nTextureUnits;
	uint32_t nVaryings;
	uint32_t nConstantSlots;
	uint32_t nReserved;
	uint64_t nStateBundleHash;
} sirGraphicsABIProgrammableRecipe_t;

typedef struct sirGraphicsABIFixedStage_s
{
	uint32_t nTextureUnit;
	uint32_t nCoordinateSet;
	uint32_t nColorOperation;
	uint32_t nAlphaOperation;
	uint32_t nColorArgument0;
	uint32_t nColorArgument1;
	uint32_t nAlphaArgument0;
	uint32_t nAlphaArgument1;
	float szConstantColor[4];
} sirGraphicsABIFixedStage_t;

typedef struct sirGraphicsABIFixedLight_s
{
	float szPosition[4];
	float szDirection[4];
	float szDiffuse[4];
	float szSpecular[4];
	float fRange;
	float fConstantAttenuation;
	float fLinearAttenuation;
	float fQuadraticAttenuation;
} sirGraphicsABIFixedLight_t;

typedef struct sirGraphicsABIFixedMaterial_s
{
	float szAmbient[4];
	float szDiffuse[4];
	float szSpecular[4];
	float szEmissive[4];
	float fPower;
	uint32_t nReserved;
} sirGraphicsABIFixedMaterial_t;

typedef struct sirGraphicsABIFixedRecipe_s
{
	uint32_t nSize;
	uint32_t nRequiredFeatures;
	uint32_t nTextureStageCount;
	uint32_t nLightCount;
	const sirGraphicsABIFixedStage_t *pStages;
	const sirGraphicsABIFixedLight_t *pLights;
	uint32_t nFogMode;
	uint32_t nAlphaCompare;
	float fAlphaReference;
	float fFogStart;
	float fFogEnd;
	float fFogDensity;
	float szFogColor[4];
	uint32_t nReserved;
	sirGraphicsABIFixedMaterial_t material;
	float szWorld[16];
	float szView[16];
	float szProjection[16];
} sirGraphicsABIFixedRecipe_t;

typedef struct sirGraphicsABIPipeline_s
{
	uint32_t nSize;
	uint32_t nType;
	sirGraphicsABIShaderCode_t vertexShader;
	sirGraphicsABIShaderCode_t pixelShader;
	sirGraphicsABIShaderCode_t computeShader;
	const sirGraphicsBindingDesc_t *pBindings;
	const sirGraphicsVertexAttribute_t *pAttributes;
	uint32_t nBindingCount;
	uint32_t nAttributeCount;
	uint32_t nColorFormatCount;
	uint32_t szColorFormats[8];
	uint32_t nDepthFormat;
	uint32_t nSampleCount;
	uint32_t nTopology;
	uint32_t nCullMode;
	uint32_t nDepthCompare;
	sirGraphicsABIBlendState_t szBlendStates[8];
	uint32_t nDepthTest;
	uint32_t nDepthWrite;
	uint32_t nModel;
	uint32_t nReserved;
	const sirGraphicsABIModernRecipe_t *pModern;
	const sirGraphicsABIProgrammableRecipe_t *pProgrammable;
	const sirGraphicsABIFixedRecipe_t *pFixed;
} sirGraphicsABIPipeline_t;

typedef struct sirGraphicsABIBufferRange_s
{
	uint32_t nSize;
	uint32_t nReserved;
	sirGraphicsHandle_t hBuffer;
	uint64_t nOffsetBytes;
	uint64_t nBytes;
} sirGraphicsABIBufferRange_t;

typedef struct sirGraphicsABICopyBuffer_s
{
	uint32_t nSize;
	uint32_t nReserved;
	sirGraphicsHandle_t hDestination;
	sirGraphicsHandle_t hSource;
	uint64_t nDstOffset;
	uint64_t nSrcOffset;
	uint64_t nBytes;
} sirGraphicsABICopyBuffer_t;

typedef struct sirGraphicsABIBufferTextureCopy_s
{
	uint32_t nSize;
	uint32_t nBufferToTexture;
	sirGraphicsHandle_t hBuffer;
	sirGraphicsHandle_t hTexture;
	uint64_t nBufferOffset;
	uint32_t nBytesPerRow;
	uint32_t nRowsPerImage;
	uint32_t nMip;
	uint32_t nLayer;
	uint32_t nX;
	uint32_t nY;
	uint32_t nZ;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nDepth;
} sirGraphicsABIBufferTextureCopy_t;

typedef struct sirGraphicsABIColorAttachment_s
{
	sirGraphicsHandle_t hView;
	float szClearColor[4];
	uint32_t nClear;
	uint32_t nStore;
} sirGraphicsABIColorAttachment_t;

typedef struct sirGraphicsABIRenderPass_s
{
	uint32_t nSize;
	uint32_t nColorCount;
	const sirGraphicsABIColorAttachment_t *pColors;
	sirGraphicsHandle_t hDepthView;
	float fClearDepth;
	uint32_t nClearStencil;
	uint32_t nClearDepth;
	uint32_t nStoreDepth;
} sirGraphicsABIRenderPass_t;

typedef struct sirGraphicsABIVertexBuffer_s
{
	uint32_t nSize;
	uint32_t nSlot;
	sirGraphicsHandle_t hBuffer;
	uint64_t nOffsetBytes;
	uint32_t nStrideBytes;
	uint32_t nReserved;
} sirGraphicsABIVertexBuffer_t;

typedef struct sirGraphicsABIIndexBuffer_s
{
	uint32_t nSize;
	uint32_t nIndex32;
	sirGraphicsHandle_t hBuffer;
	uint64_t nOffsetBytes;
} sirGraphicsABIIndexBuffer_t;

typedef struct sirGraphicsABIDraw_s
{
	uint32_t nVertices;
	uint32_t nInstances;
	uint32_t nFirstVertex;
	uint32_t nFirstInstance;
} sirGraphicsABIDraw_t;

typedef struct sirGraphicsABIDrawIndexed_s
{
	uint32_t nIndices;
	uint32_t nInstances;
	uint32_t nFirstIndex;
	int32_t nVertexOffset;
	uint32_t nFirstInstance;
	uint32_t nReserved;
} sirGraphicsABIDrawIndexed_t;

typedef struct sirGraphicsABINativeSurface_s
{
	uint32_t nSize;
	uint32_t nPlatform;
	void *pWindow;
	void *pDisplayOrInstance;
	uint64_t nWindowGeneration;
} sirGraphicsABINativeSurface_t;

typedef struct sirGraphicsABIPresentDesc_s
{
	uint32_t nSize;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nBackbufferCount;
	uint32_t nFormat;
	uint32_t nMode;
	uint32_t nAllowTearing;
	uint32_t nHdr;
} sirGraphicsABIPresentDesc_t;

typedef struct sirGraphicsABISubmission_s
{
	uint32_t nSize;
	uint32_t nCommandListCount;
	const void *const *ppCommandLists;
	const sirGraphicsCompletion_t *pWaits;
	const sirGraphicsSurfaceImage_t *pAcquisitions;
	uint32_t nWaitCount;
	uint32_t nAcquisitionCount;
	uint32_t nReserved;
} sirGraphicsABISubmission_t;

// Commands are driver-owned, device-generation-scoped borrowed contexts.
// Reset only after the preceding queue completion has retired.
typedef struct sirGraphicsABICommandList_s
{
	uint32_t nSize;
	uint32_t nDeviceGeneration;
	uint32_t nQueue;
	uint32_t nReserved;
	void *pCommands;
	uint32_t ( *pReset )( void *pCommands );
	uint32_t ( *pBarrier )( void *pCommands, const sirGraphicsBarrier_t *pBarriers, uint32_t nCount );
	uint32_t ( *pCopyBuffer )( void *pCommands, const sirGraphicsABICopyBuffer_t *pCopy );
	uint32_t ( *pCopyBufferTexture )( void *pCommands, const sirGraphicsABIBufferTextureCopy_t *pCopy );
	uint32_t ( *pBeginRenderPass )( void *pCommands, const sirGraphicsABIRenderPass_t *pPass );
	uint32_t ( *pEndRenderPass )( void *pCommands );
	uint32_t ( *pBindPipeline )( void *pCommands, sirGraphicsHandle_t hPipeline );
	uint32_t ( *pBindBindingSet )( void *pCommands, uint32_t nSetIndex, sirGraphicsHandle_t hSet );
	uint32_t ( *pBindVertexBuffer )( void *pCommands, const sirGraphicsABIVertexBuffer_t *pBuffer );
	uint32_t ( *pBindIndexBuffer )( void *pCommands, const sirGraphicsABIIndexBuffer_t *pBuffer );
	uint32_t ( *pSetViewport )( void *pCommands, const sirGraphicsViewport_t *pViewport, const sirGraphicsRect_t *pScissor );
	uint32_t ( *pDraw )( void *pCommands, const sirGraphicsABIDraw_t *pDraw );
	uint32_t ( *pDrawIndexed )( void *pCommands, const sirGraphicsABIDrawIndexed_t *pDraw );
	uint32_t ( *pDispatch )( void *pCommands, uint32_t nX, uint32_t nY, uint32_t nZ );
	uint32_t ( *pClose )( void *pCommands );
} sirGraphicsABICommandList_t;

static_assert( sizeof( sirGraphicsABIDraw_t ) == 16, "draw ABI layout" );
static_assert( sizeof( sirGraphicsABIFixedStage_t ) == 48, "fixed stage ABI layout" );
static_assert( sizeof( sirGraphicsABIFixedLight_t ) == 80, "fixed light ABI layout" );
static_assert( sizeof( sirGraphicsABIDrawIndexed_t ) == 24, "indexed draw ABI layout" );
static_assert( sizeof( sirGraphicsABIBufferRange_t ) == 40, "buffer range ABI layout" );
static_assert( sizeof( sirGraphicsABIColorAttachment_t ) == 40, "attachment ABI layout" );

#endif // __SIRENGINE_SIRGRAPHICSABIOPERATIONS_H__
