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

#include <Renderer/Drivers/D3D12/SIRD3D12Driver.h>
#include <stdio.h>

static LRESULT CALLBACK sirD3D12SurfaceWindowProc( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam )
{
	return DefWindowProcW( hWindow, nMessage, nWParam, nLParam );
}

static sirGraphicsCompletionSet_t sirD3D12SurfaceImmediate( uint32_t nGeneration ) noexcept
{
	sirGraphicsCompletionSet_t completion = {};
	completion.nDeviceGeneration = nGeneration;
	return completion;
}

static int sirTestDXGISurface( SIRRenderingDevice *pDevice, HWND hWindow ) noexcept
{
	const uint32_t nGeneration = pDevice->getDeviceGeneration();
	sirGraphicsNativeSurface_t native = {};
	native.ePlatform = SIR_GRAPHICS_SURFACE_WIN32;
	native.pWindow = hWindow;
	native.nWindowGeneration = 1;
	sirGraphicsPresentDesc_t config = {};
	config.nWidth = 64;
	config.nHeight = 64;
	config.nBackbufferCount = 2;
	config.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	config.eMode = SIR_GRAPHICS_PRESENT_FIFO;
	sirGraphicsHandle_t hSurface = {};
	if ( pDevice->createSurface( native, config, &hSurface ) != SIR_GRAPHICS_SUCCESS || hSurface.eType != SIR_GRAPHICS_HANDLE_SURFACE ||
	     hSurface.nDeviceGeneration != nGeneration ) {
		return 10;
	}
	sirGraphicsHandle_t hDuplicate = {};
	if ( pDevice->createSurface( native, config, &hDuplicate ) != SIR_GRAPHICS_BUSY || hDuplicate.nGeneration != 0 ) {
		return 11;
	}
	sirGraphicsSurfaceImage_t image = {};
	if ( pDevice->acquireSurfaceImage( hSurface, &image ) != SIR_GRAPHICS_SUCCESS || image.hImage.eType != SIR_GRAPHICS_HANDLE_TEXTURE ||
	     image.nAcquisitionId == 0 || image.nWindowGeneration != native.nWindowGeneration ) {

		return 12;
	}
	sirGraphicsSurfaceImage_t duplicateImage = {};
	if ( pDevice->acquireSurfaceImage( hSurface, &duplicateImage ) != SIR_GRAPHICS_BUSY || duplicateImage.nAcquisitionId != 0 ) {
		return 13;
	}
	if ( pDevice->resizeSurface( hSurface, native, config ) != SIR_GRAPHICS_BUSY ) {
		return 14;
	}
	sirGraphicsViewDesc_t view = {};
	view.hResource = image.hImage;
	view.eFormat = config.eFormat;
	view.nMipLevels = 1;
	view.nLayers = 1;
	view.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirGraphicsHandle_t hRtv = {};
	if ( pDevice->createView( view, &hRtv ) != SIR_GRAPHICS_SUCCESS ) {
		return 15;
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCommands ) != SIR_GRAPHICS_SUCCESS ) {
		return 16;
	}
	sirGraphicsBarrier_t transition = {};
	transition.eType = SIR_GRAPHICS_BARRIER_TRANSITION;
	transition.hResource = image.hImage;
	transition.eBefore = SIR_GRAPHICS_STATE_PRESENT;
	transition.eAfter = SIR_GRAPHICS_STATE_COLOR_ATTACHMENT;
	transition.nMipCount = 1;
	transition.nLayerCount = 1;
	if ( pCommands->barrier( &transition, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return 17;
	}
	sirGraphicsColorAttachment_t color = {};
	color.hView = hRtv;
	color.bClear = true;
	color.bStore = true;
	color.szClearColor[0] = 1.0f;
	color.szClearColor[3] = 1.0f;
	sirGraphicsRenderPass_t pass = {};
	pass.pColors = &color;
	pass.nColorCount = 1;
	if ( pCommands->beginRenderPass( pass ) != SIR_GRAPHICS_SUCCESS || pCommands->endRenderPass() != SIR_GRAPHICS_SUCCESS ) {
		return 18;
	}
	transition.eBefore = SIR_GRAPHICS_STATE_COLOR_ATTACHMENT;
	transition.eAfter = SIR_GRAPHICS_STATE_PRESENT;
	if ( pCommands->barrier( &transition, 1 ) != SIR_GRAPHICS_SUCCESS || pCommands->close() != SIR_GRAPHICS_SUCCESS ) {
		return 19;
	}
	SIRGraphicsCommandList *ppCommands[] = { pCommands };
	sirGraphicsSubmission_t submission = {};
	submission.ppCommandLists = ppCommands;
	submission.nCommandListCount = 1;
	sirGraphicsCompletion_t rendered = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &rendered ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return 20;
	}
	sirGraphicsSurfaceImage_t stale = image;
	++stale.nAcquisitionId;
	submission.pAcquisitions = &stale;
	submission.nAcquisitionCount = 1;
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &rendered ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 21;
	}
	submission.pAcquisitions = &image;
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &rendered ) != SIR_GRAPHICS_SUCCESS ||
	     rendered.nDeviceGeneration != nGeneration || rendered.eQueue != SIR_GRAPHICS_QUEUE_DIRECT || rendered.nValue == 0 ) {
		return 22;
	}
	sirGraphicsCompletion_t forged = rendered;
	++forged.nValue;
	if ( pDevice->present( image, forged ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     pDevice->present( image, rendered ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->present( image, rendered ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 23;
	}
	if ( pDevice->wait( rendered, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 24;
	}
	sirGraphicsCompletionSet_t immediate = sirD3D12SurfaceImmediate( nGeneration );
	if ( pDevice->retireResource( image.hImage, immediate ) != SIR_GRAPHICS_UNSUPPORTED ||
	     pDevice->retireResource( hRtv, immediate ) != SIR_GRAPHICS_SUCCESS ) {
		return 25;
	}
	native.nWindowGeneration = 2;
	config.nWidth = 96;
	config.nHeight = 80;
	if ( pDevice->resizeSurface( hSurface, native, config ) != SIR_GRAPHICS_SUCCESS ) {
		return 26;
	}
	sirGraphicsSurfaceImage_t resized = {};
	if ( pDevice->acquireSurfaceImage( hSurface, &resized ) != SIR_GRAPHICS_SUCCESS || resized.nWindowGeneration != 2 ||
	     resized.hImage.nGeneration == 0 ) {
		return 27;
	}
	if ( pDevice->present( resized, rendered ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     pDevice->retireResource( hSurface, immediate ) != SIR_GRAPHICS_BUSY ) {
		return 28;
	}
	SIRGraphicsCommandList *pEmpty = nullptr;
	if ( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pEmpty ) != SIR_GRAPHICS_SUCCESS ||
	     pEmpty->close() != SIR_GRAPHICS_SUCCESS ) {
		return 29;
	}
	SIRGraphicsCommandList *ppEmpty[] = { pEmpty };
	submission = {};
	submission.ppCommandLists = ppEmpty;
	submission.nCommandListCount = 1;
	submission.pAcquisitions = &resized;
	submission.nAcquisitionCount = 1;
	sirGraphicsCompletion_t completed = {};
	if ( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->present( resized, completed ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->wait( completed, 10000000000ULL ) != SIR_GRAPHICS_SUCCESS ) {
		return 30;
	}
	if ( pDevice->retireResource( hSurface, immediate ) != SIR_GRAPHICS_SUCCESS ||
	     pDevice->acquireSurfaceImage( hSurface, &resized ) != SIR_GRAPHICS_INVALID_HANDLE ) {
		return 31;
	}
	return 0;
}

int main( void )
{
	const HINSTANCE hInstance = GetModuleHandleW( nullptr );
	WNDCLASSW wndClass = {};
	wndClass.lpfnWndProc = sirD3D12SurfaceWindowProc;
	wndClass.hInstance = hInstance;
	wndClass.lpszClassName = L"SIRD3D12SurfaceTestWindow";
	if ( RegisterClassW( &wndClass ) == 0 ) {
		return 1;
	}
	HWND hWindow = CreateWindowExW( 0, wndClass.lpszClassName, L"SIREngine D3D12 WARP Surface Test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
	                                CW_USEDEFAULT, 96, 96, nullptr, nullptr, hInstance, nullptr );
	if ( hWindow == nullptr ) {
		UnregisterClassW( wndClass.lpszClassName, hInstance );
		return 2;
	}
	ShowWindow( hWindow, SW_SHOWNOACTIVATE );
	SIRD3D12Driver driver;
	uint32_t nAdapterCount = 0;
	uint32_t nWarpIndex = UINT32_MAX;
	if ( driver.getAdapterCount( &nAdapterCount ) != SIR_GRAPHICS_SUCCESS ) {
		return 3;
	}
	for ( uint32_t iAdapter = 0; iAdapter < nAdapterCount; ++iAdapter ) {
		sirGraphicsAdapterInfo_t info = {};
		if ( driver.queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS ) {
			return 4;
		}
		if ( info.bSoftwareAdapter ) {
			nWarpIndex = iAdapter;
		}
	}
	if ( nWarpIndex == UINT32_MAX ) {
		return 5;
	}
	sirGraphicsDeviceCreateDesc_t config = {};
	config.nAdapterIndex = nWarpIndex;
	config.nFramesInFlight = 3;
	config.nRequiredFeatures = SIR_GRAPHICS_FEATURE_PRESENT;
	config.nUploadBudgetBytes = 3 * 64 * 1024;
	config.nReadbackBudgetBytes = config.nUploadBudgetBytes;
	config.bAllowSoftware = true;
	SIRRenderingDevice *pDevice = nullptr;
	if ( driver.createDevice( config, &pDevice ) != SIR_GRAPHICS_SUCCESS || pDevice == nullptr ) {
		return 6;
	}
	const int nTest = sirTestDXGISurface( pDevice, hWindow );
	const sirGraphicsResult_t eShutdown = driver.destroyDevice( pDevice );
	DestroyWindow( hWindow );
	UnregisterClassW( wndClass.lpszClassName, hInstance );
	if ( nTest != 0 ) {
		fprintf( stderr, "D3D12 DXGI surface test failed at stage %d\\n", nTest );
		return nTest;
	}
	return eShutdown == SIR_GRAPHICS_SUCCESS ? 0 : 7;
}
