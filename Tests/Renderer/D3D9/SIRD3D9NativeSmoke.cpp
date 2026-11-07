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
#include <Renderer/Drivers/D3D9/SIRD3D9Native.h>
#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LRESULT CALLBACK sirD3D9TestWindowProc( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam )
{
	return DefWindowProcW( hWindow, nMessage, nWParam, nLParam );
}

int main( int argc, char **argv )
{
	HINSTANCE hInstance = GetModuleHandleW( nullptr );
	WNDCLASSEXW cls = {};
	cls.cbSize = sizeof( cls );
	cls.hInstance = hInstance;
	cls.lpszClassName = L"SIRD3D9NativeSmokeWindow";
	cls.lpfnWndProc = sirD3D9TestWindowProc;
	if ( RegisterClassExW( &cls ) == 0 ) {
		return 1;
	}
	HWND hWindow = CreateWindowExW( 0, cls.lpszClassName, L"SIRD3D9 Native Smoke", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320,
	                                240, nullptr, nullptr, hInstance, nullptr );
	if ( hWindow == nullptr ) {
		UnregisterClassW( cls.lpszClassName, hInstance );
		return 2;
	}
	ShowWindow( hWindow, SW_SHOW );
	sirD3D9Native_t native = {};
	const bool bRequireEx = argc > 1 && strcmp( argv[1], "--ex" ) == 0;
	sirGraphicsResult_t eResult = sirD3D9NativeInitialize( &native, hWindow, 0, bRequireEx );
	if ( eResult == SIR_GRAPHICS_SUCCESS && bRequireEx && !native.bEx ) {
		eResult = SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeClear( &native, 0xff203040U );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeDrawTriangle( &native );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		uint8_t *pPixels = ( uint8_t * )malloc( 320 * 240 * 4 );
		if ( pPixels == nullptr ) {
			eResult = SIR_GRAPHICS_OUT_OF_MEMORY;
		} else {
			sirD3D9Readback_t readback = {};
			readback.pPixels = pPixels;
			readback.nCapacityBytes = 320 * 240 * 4;
			readback.nRowStride = 320 * 4;
			eResult = sirD3D9NativeReadback( &native, &readback );
			if ( eResult == SIR_GRAPHICS_SUCCESS && ( readback.nWidth == 0 || readback.nHeight == 0 ) ) {
				eResult = SIR_GRAPHICS_DRIVER_ERROR;
			}
			free( pPixels );
		}
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativePresent( &native );
	}
	sirGraphicsHandle_t hOldBuffer = {};
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeConfigureResidentBudget( &native, 64 * 1024 * 1024 );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeCreateTrackedBuffer( &native, 1024, false, &hOldBuffer );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		void *pResource = nullptr;
		eResult = sirD3D9NativeLookupResource( &native, hOldBuffer, &pResource );
		if ( eResult == SIR_GRAPHICS_SUCCESS && pResource == nullptr ) {
			eResult = SIR_GRAPHICS_DRIVER_ERROR;
		}
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeResize( &native, 256, 192 );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		void *pResource = nullptr;
		if ( sirD3D9NativeLookupResource( &native, hOldBuffer, &pResource ) != SIR_GRAPHICS_INVALID_HANDLE ) {
			eResult = SIR_GRAPHICS_DRIVER_ERROR;
		}
	}
	sirGraphicsHandle_t hTexture = {};
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeCreateTrackedTexture( &native, 64, 64, &hTexture );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeRetireResource( &native, hTexture, 0 );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		sirD3D9NativeCollectResources( &native, 0 );
		if ( native.resources.nUsedBytes != 0 ) {
			eResult = SIR_GRAPHICS_DRIVER_ERROR;
		}
	}
	sirD3D9NativeShutdown( &native );
	DestroyWindow( hWindow );
	UnregisterClassW( cls.lpszClassName, hInstance );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		fprintf( stderr, "D3D9 hardware smoke failed with result %u\n", ( unsigned )eResult );
		return 3;
	}
	return 0;
}
#endif
