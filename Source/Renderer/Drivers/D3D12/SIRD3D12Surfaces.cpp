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

#include <Renderer/Drivers/D3D12/SIRD3D12Surfaces.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <limits.h>

static DXGI_FORMAT sirD3D12SurfaceFormat( sirGraphicsFormat_t eFormat ) noexcept
{
	if ( eFormat == SIR_GRAPHICS_FORMAT_RGBA8_UNORM ) {
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	if ( eFormat == SIR_GRAPHICS_FORMAT_BGRA8_UNORM ) {
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
	return DXGI_FORMAT_UNKNOWN;
}

SIRD3D12Surfaces::~SIRD3D12Surfaces( void ) noexcept
{
	shutdown( false );
}

bool SIRD3D12Surfaces::initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, uint32_t nGeneration ) noexcept
{
	if ( m_pNative != nullptr || pNative == nullptr || pResources == nullptr || nGeneration == 0 ) {
		return false;
	}
	m_pFactory = pNative->getNativeFactory();
	m_pDirectQueue = pNative->getDirectQueue();
	if ( m_pFactory == nullptr || m_pDirectQueue == nullptr ) {
		return false;
	}
	m_pNative = pNative;
	m_pResources = pResources;
	m_nDeviceGeneration = nGeneration;
	BOOL bSupported = FALSE;
	m_bTearingSupported =
	    SUCCEEDED( m_pFactory->CheckFeatureSupport( DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bSupported, sizeof( bSupported ) ) ) &&
	    bSupported != FALSE;
	return true;
}

void SIRD3D12Surfaces::shutdown( bool bGpuComplete ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < s_nSurfaceCapacity; ++iSlot ) {
		sirD3D12SurfaceSlot_t& slot = m_szSlots[iSlot];
		if ( bGpuComplete ) {
			// Resource/view ownership is discharged by SIRD3D12Resources after
			// surfaces stop presenting and before the D3D12MA device shuts down.
			slot.pSwapchain.Reset();
		} else {
			( void )slot.pSwapchain.Detach();
		}
		const uint32_t nGeneration = slot.nGeneration;
		slot = {};
		slot.nGeneration = nGeneration;
	}
	m_pNative = nullptr;
	m_pResources = nullptr;
	m_pFactory = nullptr;
	m_pDirectQueue = nullptr;
	m_nDeviceGeneration = 0;
	m_bTearingSupported = false;
}

sirD3D12SurfaceSlot_t *SIRD3D12Surfaces::findSlot( sirGraphicsHandle_t hSurface ) noexcept
{
	if ( m_pNative == nullptr || !sirGraphicsIsHandleValid( hSurface, SIR_GRAPHICS_HANDLE_SURFACE, m_nDeviceGeneration ) ||
	     hSurface.nSlot >= s_nSurfaceCapacity ) {
		return nullptr;
	}
	sirD3D12SurfaceSlot_t& slot = m_szSlots[hSurface.nSlot];
	return slot.bLive && slot.nGeneration == hSurface.nGeneration ? &slot : nullptr;
}

sirGraphicsResult_t SIRD3D12Surfaces::validateWindow( const sirGraphicsNativeSurface_t& native,
                                                      const sirGraphicsPresentDesc_t& present ) const noexcept
{
	if ( native.ePlatform != SIR_GRAPHICS_SURFACE_WIN32 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( native.pWindow == nullptr || native.nWindowGeneration == 0 || !IsWindow( ( HWND )native.pWindow ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( present.nWidth == 0 || present.nHeight == 0 || present.nWidth > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
	     present.nHeight > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || present.nBackbufferCount < 2 || present.nBackbufferCount > 3 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( sirD3D12SurfaceFormat( present.eFormat ) == DXGI_FORMAT_UNKNOWN || present.bHdr ||
	     present.eMode == SIR_GRAPHICS_PRESENT_MAILBOX ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( present.eMode > SIR_GRAPHICS_PRESENT_MAILBOX || ( present.bAllowTearing && !m_bTearingSupported ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}

bool SIRD3D12Surfaces::isDirectComplete( uint64_t nFence ) noexcept
{
	if ( nFence == 0 ) {
		return true;
	}
	sirD3D12CompletionSet_t snapshot = {};
	if ( !m_pNative->snapshotCompletionSet( &snapshot ) || snapshot.nGraphicsFence < nFence ) {
		return false;
	}
	sirD3D12CompletionSet_t requested = {};
	requested.nDeviceGeneration = snapshot.nDeviceGeneration;
	requested.nGraphicsFence = nFence;
	bool bComplete = false;
	return m_pNative->pollCompletionSet( requested, &bComplete ) && bComplete;
}

sirGraphicsResult_t SIRD3D12Surfaces::releaseImages( sirD3D12SurfaceSlot_t& slot ) noexcept
{
	for ( uint32_t iImage = 0; iImage < slot.nBufferCount; ++iImage ) {
		if ( slot.szImages[iImage].nGeneration != 0 && !m_pResources->canReleaseExternalTexture( slot.szImages[iImage] ) ) {
			return SIR_GRAPHICS_BUSY;
		}
	}
	for ( uint32_t iImage = 0; iImage < slot.nBufferCount; ++iImage ) {
		if ( slot.szImages[iImage].nGeneration != 0 ) {
			const sirGraphicsResult_t eRelease = m_pResources->releaseExternalTexture( slot.szImages[iImage] );
			if ( eRelease != SIR_GRAPHICS_SUCCESS ) {
				return eRelease;
			}
			slot.szImages[iImage] = {};
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Surfaces::importImages( sirD3D12SurfaceSlot_t& slot ) noexcept
{
	for ( uint32_t iImage = 0; iImage < slot.nBufferCount; ++iImage ) {
		Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer;
		if ( FAILED( slot.pSwapchain->GetBuffer( iImage, IID_PPV_ARGS( pBuffer.GetAddressOf() ) ) ) ) {
			( void )releaseImages( slot );
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		const sirGraphicsResult_t eResult = m_pResources->importExternalTexture( pBuffer.Get(), slot.eFormat, &slot.szImages[iImage] );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			( void )releaseImages( slot );
			return eResult;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Surfaces::createSwapchain( sirD3D12SurfaceSlot_t& slot, const sirGraphicsNativeSurface_t& native,
                                                       const sirGraphicsPresentDesc_t& present ) noexcept
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = present.nWidth;
	desc.Height = present.nHeight;
	desc.Format = sirD3D12SurfaceFormat( present.eFormat );
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = present.nBackbufferCount;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	desc.Flags = present.bAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapchain;
	const HRESULT nResult =
	    m_pFactory->CreateSwapChainForHwnd( m_pDirectQueue, ( HWND )native.pWindow, &desc, nullptr, nullptr, pSwapchain.GetAddressOf() );
	if ( FAILED( nResult ) || FAILED( pSwapchain.As( &slot.pSwapchain ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	slot.hWindow = ( HWND )native.pWindow;
	slot.nWindowGeneration = native.nWindowGeneration;
	slot.nWidth = present.nWidth;
	slot.nHeight = present.nHeight;
	slot.nBufferCount = present.nBackbufferCount;
	slot.eFormat = present.eFormat;
	slot.eMode = present.eMode;
	slot.bAllowTearing = present.bAllowTearing;
	return importImages( slot );
}

sirGraphicsResult_t SIRD3D12Surfaces::createSurface( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& present,
                                                     sirGraphicsHandle_t *pSurface ) noexcept
{
	if ( pSurface == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pSurface = {};
	if ( m_pNative == nullptr || !m_pNative->isReady() ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const sirGraphicsResult_t eValid = validateWindow( native, present );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	uint32_t iFree = s_nSurfaceCapacity;
	for ( uint32_t iSlot = 0; iSlot < s_nSurfaceCapacity; ++iSlot ) {
		if ( m_szSlots[iSlot].bLive && m_szSlots[iSlot].hWindow == native.pWindow ) {
			return SIR_GRAPHICS_BUSY;
		}
		if ( !m_szSlots[iSlot].bLive && m_szSlots[iSlot].nGeneration < UINT32_MAX && iFree == s_nSurfaceCapacity ) {
			iFree = iSlot;
		}
	}
	if ( iFree == s_nSurfaceCapacity ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	sirD3D12SurfaceSlot_t& slot = m_szSlots[iFree];
	const uint32_t nGeneration = slot.nGeneration + 1;
	slot = {};
	slot.nGeneration = nGeneration;
	const sirGraphicsResult_t eCreate = createSwapchain( slot, native, present );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		slot.pSwapchain.Reset();
		slot = {};
		slot.nGeneration = nGeneration;
		return eCreate;
	}
	slot.bLive = true;
	*pSurface = { iFree, nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_SURFACE };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Surfaces::resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t& native,
                                                     const sirGraphicsPresentDesc_t& present ) noexcept
{
	sirD3D12SurfaceSlot_t *pSlot = findSlot( hSurface );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirGraphicsResult_t eValid = validateWindow( native, present );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( native.nWindowGeneration < pSlot->nWindowGeneration ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	if ( pSlot->bAcquired || !isDirectComplete( pSlot->nLastDirectFence ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsResult_t eRelease = releaseImages( *pSlot );
	if ( eRelease != SIR_GRAPHICS_SUCCESS ) {
		return eRelease;
	}
	if ( native.pWindow != pSlot->hWindow ) {
		pSlot->pSwapchain.Reset();
		return createSwapchain( *pSlot, native, present );
	}
	const DXGI_FORMAT eFormat = sirD3D12SurfaceFormat( present.eFormat );
	const UINT nFlags = present.bAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	const HRESULT nResize = pSlot->pSwapchain->ResizeBuffers( present.nBackbufferCount, present.nWidth, present.nHeight, eFormat, nFlags );
	if ( FAILED( nResize ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pSlot->nWidth = present.nWidth;
	pSlot->nHeight = present.nHeight;
	pSlot->nBufferCount = present.nBackbufferCount;
	pSlot->nWindowGeneration = native.nWindowGeneration;
	pSlot->eFormat = present.eFormat;
	pSlot->eMode = present.eMode;
	pSlot->bAllowTearing = present.bAllowTearing;
	pSlot->nLastDirectFence = 0;
	return importImages( *pSlot );
}

sirGraphicsResult_t SIRD3D12Surfaces::acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	if ( pImage == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pImage = {};
	sirD3D12SurfaceSlot_t *pSlot = findSlot( hSurface );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->bAcquired ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( pSlot->nAcquisitionCounter == UINT64_MAX || pSlot->pSwapchain == nullptr ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( !IsWindow( pSlot->hWindow ) ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	const uint32_t iImage = pSlot->pSwapchain->GetCurrentBackBufferIndex();
	if ( iImage >= pSlot->nBufferCount || pSlot->szImages[iImage].nGeneration == 0 ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	pSlot->iAcquiredImage = iImage;
	pSlot->nAcquisitionId = pSlot->nAcquisitionCounter++;
	pSlot->bAcquired = true;
	pSlot->bSubmitted = false;
	pSlot->renderCompletion = {};
	*pImage = { hSurface, pSlot->szImages[iImage], pSlot->nAcquisitionId, pSlot->nWindowGeneration };
	return SIR_GRAPHICS_SUCCESS;
}

bool SIRD3D12Surfaces::imageMatches( const sirD3D12SurfaceSlot_t& slot, const sirGraphicsSurfaceImage_t& image ) const noexcept
{
	const sirGraphicsHandle_t& hExpected = slot.szImages[slot.iAcquiredImage];
	return slot.bAcquired && slot.nAcquisitionId == image.nAcquisitionId && slot.nWindowGeneration == image.nWindowGeneration &&
	       hExpected.nSlot == image.hImage.nSlot && hExpected.nGeneration == image.hImage.nGeneration &&
	       hExpected.nDeviceGeneration == image.hImage.nDeviceGeneration && hExpected.eType == image.hImage.eType;
}

sirGraphicsResult_t SIRD3D12Surfaces::validateSubmission( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t& submission ) noexcept
{
	if ( submission.nAcquisitionCount == 0 ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( eQueue != SIR_GRAPHICS_QUEUE_DIRECT || submission.nAcquisitionCount > s_nSurfaceCapacity ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t iImage = 0; iImage < submission.nAcquisitionCount; ++iImage ) {
		const sirGraphicsSurfaceImage_t& image = submission.pAcquisitions[iImage];
		sirD3D12SurfaceSlot_t *pSlot = findSlot( image.hSurface );
		if ( pSlot == nullptr || pSlot->bSubmitted || !imageMatches( *pSlot, image ) ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		for ( uint32_t iPrior = 0; iPrior < iImage; ++iPrior ) {
			if ( submission.pAcquisitions[iPrior].hSurface.nSlot == image.hSurface.nSlot ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12Surfaces::completeSubmission( const sirGraphicsSubmission_t& submission, const sirGraphicsCompletion_t& completion ) noexcept
{
	for ( uint32_t iImage = 0; iImage < submission.nAcquisitionCount; ++iImage ) {
		sirD3D12SurfaceSlot_t *pSlot = findSlot( submission.pAcquisitions[iImage].hSurface );
		if ( pSlot != nullptr ) {
			pSlot->bSubmitted = true;
			pSlot->renderCompletion = completion;
			pSlot->nLastDirectFence = completion.nValue;
		}
	}
}

sirGraphicsResult_t SIRD3D12Surfaces::present( const sirGraphicsSurfaceImage_t& image,
                                               const sirGraphicsCompletion_t& renderComplete ) noexcept
{
	sirD3D12SurfaceSlot_t *pSlot = findSlot( image.hSurface );
	if ( pSlot == nullptr || !imageMatches( *pSlot, image ) || !pSlot->bSubmitted ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( renderComplete.eQueue != SIR_GRAPHICS_QUEUE_DIRECT || renderComplete.nDeviceGeneration != m_nDeviceGeneration ||
	     renderComplete.nValue != pSlot->renderCompletion.nValue ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !IsWindow( pSlot->hWindow ) ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	const UINT nSyncInterval = pSlot->eMode == SIR_GRAPHICS_PRESENT_FIFO ? 1U : 0U;
	const UINT nFlags = nSyncInterval == 0 && pSlot->bAllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
	const HRESULT nPresent = pSlot->pSwapchain->Present( nSyncInterval, nFlags );
	if ( nPresent == DXGI_ERROR_DEVICE_REMOVED || nPresent == DXGI_ERROR_DEVICE_RESET ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( FAILED( nPresent ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pSlot->bSubmitted = false;
	pSlot->bAcquired = false;
	pSlot->nAcquisitionId = 0;
	pSlot->renderCompletion = {};
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Surfaces::retireSurface( sirGraphicsHandle_t hSurface, const sirGraphicsCompletionSet_t& completion ) noexcept
{
	sirD3D12SurfaceSlot_t *pSlot = findSlot( hSurface );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D12CompletionSet_t native = {};
	native.nDeviceGeneration = completion.nDeviceGeneration;
	native.nGraphicsFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
	native.nComputeFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE];
	native.nCopyFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY];
	bool bComplete = false;
	if ( !m_pNative->pollCompletionSet( native, &bComplete ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSlot->bAcquired || !bComplete || !isDirectComplete( pSlot->nLastDirectFence ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsResult_t eRelease = releaseImages( *pSlot );
	if ( eRelease != SIR_GRAPHICS_SUCCESS ) {
		return eRelease;
	}
	pSlot->pSwapchain.Reset();
	const uint32_t nGeneration = pSlot->nGeneration;
	*pSlot = {};
	pSlot->nGeneration = nGeneration;
	return SIR_GRAPHICS_SUCCESS;
}
