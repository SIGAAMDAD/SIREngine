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

#ifndef __SIRENGINE_SIRGLTEXTURES_H__
#define __SIRENGINE_SIRGLTEXTURES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLTextureDispatch_s
{
	void ( *pCreateTextures )( uint32_t nTarget, int32_t nCount, uint32_t *pNames );
	void ( *pGenTextures )( int32_t nCount, uint32_t *pNames );
	void ( *pTextureView )( uint32_t nName, uint32_t nTarget, uint32_t nParent, uint32_t nFormat, uint32_t nBaseMip, uint32_t nMipLevels,
	                        uint32_t nBaseLayer, uint32_t nLayers );
	void ( *pTextureStorage2D )( uint32_t nName, int32_t nLevels, uint32_t nInternalFormat, int32_t nWidth, int32_t nHeight );
	void ( *pDeleteTextures )( int32_t nCount, const uint32_t *pNames );
	uint32_t ( *pGetError )( void );
} sirGLTextureDispatch_t;

typedef struct sirGLNativeTexture_s
{
	uint32_t nName;
	uint32_t nInternalFormat;
	uint64_t nRequestedBytes;
} sirGLNativeTexture_t;

typedef struct sirGLTextureViewCreateInfo_s
{
	uint32_t nParentName;
	uint32_t nInternalFormat;
	uint32_t nBaseMip;
	uint32_t nMipLevels;
} sirGLTextureViewCreateInfo_t;

sirGraphicsResult_t sirGLLoadTextureDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLTextureDispatch_t *pAPI ) noexcept;
sirGraphicsResult_t sirGLDescribeTexture2D( const sirGraphicsTextureDesc_t *pDesc, uint32_t nMaximumSize,
                                            sirGLNativeTexture_t *pTexture ) noexcept;
sirGraphicsResult_t sirGLCreateNativeTexture2D( const sirGLTextureDispatch_t *pAPI, const sirGraphicsTextureDesc_t *pDesc,
                                                sirGLNativeTexture_t *pTexture ) noexcept;
sirGraphicsResult_t sirGLCreateNativeTextureView( const sirGLTextureDispatch_t *pAPI, const sirGLTextureViewCreateInfo_t *pInfo,
                                                  uint32_t *pName ) noexcept;
sirGraphicsResult_t sirGLDeleteNativeTexture( const sirGLTextureDispatch_t *pAPI, uint32_t nName ) noexcept;

#endif // __SIRENGINE_SIRGLTEXTURES_H__
