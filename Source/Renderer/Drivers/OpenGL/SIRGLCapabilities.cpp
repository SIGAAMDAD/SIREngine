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

#include <Renderer/Drivers/OpenGL/SIRGLCapabilities.h>
#include <string.h>

static bool sirGLHasText( const char *pszHaystack, const char *pszNeedle ) noexcept
{
	return pszHaystack != nullptr && pszNeedle != nullptr && strstr( pszHaystack, pszNeedle ) != nullptr;
}

static uint64_t sirGLHashText( uint64_t nHash, const char *pszText ) noexcept
{
	for ( ; pszText != nullptr && *pszText != '\0'; ++pszText ) {
		nHash = ( nHash ^ ( uint8_t )*pszText ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

static bool sirGLIsSoftware( const sirGLProbe_t *pProbe ) noexcept
{
	return sirGLHasText( pProbe->szRenderer, "llvmpipe" ) || sirGLHasText( pProbe->szRenderer, "softpipe" ) ||
	       sirGLHasText( pProbe->szRenderer, "Software Rasterizer" ) || sirGLHasText( pProbe->szRenderer, "SWR" );
}

sirGraphicsResult_t sirGLValidateCore46( const sirGLProbe_t *pProbe ) noexcept
{
	if ( pProbe == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pProbe->nMajor < 4 || ( pProbe->nMajor == 4 && pProbe->nMinor < 6 ) || ( pProbe->nCoreProfileMask & 1U ) == 0 ||
	     pProbe->nMaximumTextureSize == 0 || pProbe->nMaximumColorAttachments == 0 || pProbe->nMaximumUniformBindings == 0 ||
	     pProbe->nMaximumStorageBindings == 0 || pProbe->nMaximumSamples == 0 || pProbe->szVendor[0] == '\0' ||
	     pProbe->szRenderer[0] == '\0' ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLBuildAdapter( const sirGLProbe_t *pProbe, sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pAdapter == nullptr || pAdapter->nSize < sizeof( *pAdapter ) || pAdapter->nIndex != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eResult = sirGLValidateCore46( pProbe );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	sirGraphicsABIAdapter_t result = {};
	result.nSize = sizeof( result );
	result.nIndex = 0;
	result.nBackend = SIR_GRAPHICS_BACKEND_OPENGL;
	result.nKind = sirGLIsSoftware( pProbe ) ? SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE : SIR_GRAPHICS_ABI_ADAPTER_OTHER;
	result.nApiMajor = pProbe->nMajor;
	result.nApiMinor = pProbe->nMinor;
	result.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	result.nStableId = sirGLHashText( sirGLHashText( UINT64_C( 14695981039346656037 ), pProbe->szVendor ), pProbe->szRenderer );
	if ( result.nStableId == 0 ) {
		result.nStableId = 1;
	}
	// No driver-advertised rendering features until the native operations are implemented.
	result.nFeatureMask = 0;
	memcpy( result.szName, pProbe->szRenderer, sizeof( result.szName ) );
	result.szName[sizeof( result.szName ) - 1] = '\0';
	*pAdapter = result;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLBuildCapabilities( const sirGLProbe_t *pProbe, uint32_t nGeneration,
                                            sirGraphicsABICapabilities_t *pCapabilities ) noexcept
{
	if ( pCapabilities == nullptr || pCapabilities->nSize < sizeof( *pCapabilities ) || nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eResult = sirGLValidateCore46( pProbe );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	sirGraphicsABICapabilities_t result = {};
	result.nSize = sizeof( result );
	result.nDeviceGeneration = nGeneration;
	result.nBackend = SIR_GRAPHICS_BACKEND_OPENGL;
	result.nAdapterKind = sirGLIsSoftware( pProbe ) ? SIR_GRAPHICS_ABI_ADAPTER_SOFTWARE : SIR_GRAPHICS_ABI_ADAPTER_OTHER;
	result.nEnabledQueues = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	result.nMaximumTextureDimension = pProbe->nMaximumTextureSize;
	result.nMaximumColorAttachments = pProbe->nMaximumColorAttachments;
	result.nMaximumBindings = pProbe->nMaximumUniformBindings;
	result.nMaximumSamples = 1;        // Native multisample resource creation is not implemented yet.
	result.nMaximumFramesInFlight = 1; // Native submission/fence support is not implemented yet.
	result.nShaderProfile = 460;
	result.nEnabledFeatures = 0; // Do not advertise commands the provider cannot execute.
	*pCapabilities = result;
	return SIR_GRAPHICS_SUCCESS;
}
