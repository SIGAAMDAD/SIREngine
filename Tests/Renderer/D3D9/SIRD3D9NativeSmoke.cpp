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

static sirGraphicsResult_t sirD3D9TestCookedShaders( sirD3D9Native_t *pNative, const char *pszVertexFile, const char *pszPixelFile )
{
	if ( pNative == nullptr || pszVertexFile == nullptr || pszPixelFile == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const char *aFiles[] = { pszVertexFile, pszPixelFile };
	IUnknown *aShaders[2] = {};
	sirGraphicsResult_t eResult = SIR_GRAPHICS_SUCCESS;
	for ( uint32_t i = 0; i < 2; ++i ) {
		FILE *pFile = fopen( aFiles[i], "rb" );
		if ( pFile == nullptr || fseek( pFile, 0, SEEK_END ) != 0 ) {
			if ( pFile != nullptr ) {
				fclose( pFile );
			}
			eResult = SIR_GRAPHICS_INVALID_ARGUMENT;
			break;
		}
		const long nLength = ftell( pFile );
		if ( nLength <= 0 || nLength > 1024 * 1024 || fseek( pFile, 0, SEEK_SET ) != 0 ) {
			fclose( pFile );
			eResult = SIR_GRAPHICS_INVALID_ARGUMENT;
			break;
		}
		void *pBytes = malloc( ( size_t )nLength );
		if ( pBytes == nullptr ) {
			fclose( pFile );
			eResult = SIR_GRAPHICS_OUT_OF_MEMORY;
			break;
		}
		const bool bRead = fread( pBytes, 1, ( size_t )nLength, pFile ) == ( size_t )nLength;
		fclose( pFile );
		eResult = bRead ? sirD3D9NativeCreateShaderTokens( pNative, pBytes, ( uint64_t )nLength,
		                                                   i == 0 ? SIR_GRAPHICS_STAGE_VERTEX : SIR_GRAPHICS_STAGE_FRAGMENT, &aShaders[i] )
		                : SIR_GRAPHICS_INVALID_ARGUMENT;
		free( pBytes );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			break;
		}
	}
	IDirect3DVertexDeclaration9 *pDeclaration = nullptr;
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		const D3DVERTEXELEMENT9 elements[] = { { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		                                       D3DDECL_END() };
		if ( FAILED( pNative->pDevice->CreateVertexDeclaration( elements, &pDeclaration ) ) ) {
			eResult = SIR_GRAPHICS_DRIVER_ERROR;
		}
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		const float aPosition[] = { -0.75f, -0.75f, 0.5f, 1.0f, 0.75f, -0.75f, 0.5f, 1.0f, 0.0f, 0.75f, 0.5f, 1.0f };
		const float aConstant[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
		sirD3D9ShaderConstants_t constants = {};
		constants.nStage = SIR_GRAPHICS_STAGE_VERTEX;
		constants.nRegisterCount = 1;
		constants.pFloat4 = aConstant;
		eResult = sirD3D9NativeSetShaderConstants( pNative, &constants );
		if ( eResult == SIR_GRAPHICS_SUCCESS && ( FAILED( pNative->pDevice->SetVertexDeclaration( pDeclaration ) ) ||
		                                          FAILED( pNative->pDevice->SetVertexShader( ( IDirect3DVertexShader9 * )aShaders[0] ) ) ||
		                                          FAILED( pNative->pDevice->SetPixelShader( ( IDirect3DPixelShader9 * )aShaders[1] ) ) ||
		                                          FAILED( pNative->pDevice->BeginScene() ) ) ) {
			eResult = SIR_GRAPHICS_DRIVER_ERROR;
		} else if ( eResult == SIR_GRAPHICS_SUCCESS ) {
			const HRESULT hDraw = pNative->pDevice->DrawPrimitiveUP( D3DPT_TRIANGLELIST, 1, aPosition, 4 * sizeof( float ) );
			const HRESULT hEnd = pNative->pDevice->EndScene();
			if ( FAILED( hDraw ) || FAILED( hEnd ) ) {
				eResult = SIR_GRAPHICS_DRIVER_ERROR;
			}
		}
	}
	if ( pNative->pDevice != nullptr ) {
		( void )pNative->pDevice->SetVertexShader( nullptr );
		( void )pNative->pDevice->SetPixelShader( nullptr );
		( void )pNative->pDevice->SetVertexDeclaration( nullptr );
	}
	if ( pDeclaration != nullptr ) {
		pDeclaration->Release();
	}
	for ( IUnknown *pShader : aShaders ) {
		if ( pShader != nullptr ) {
			pShader->Release();
		}
	}
	return eResult;
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
	if ( eResult == SIR_GRAPHICS_SUCCESS && argc == 4 && strcmp( argv[1], "--shaders" ) == 0 ) {
		eResult = sirD3D9TestCookedShaders( &native, argv[2], argv[3] );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeDrawTriangle( &native );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		eResult = sirD3D9NativeBeginPass( &native );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		if ( sirD3D9NativeBeginPass( &native ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
			eResult = SIR_GRAPHICS_DRIVER_ERROR;
		}
	}
	if ( native.bSceneActive ) {
		const sirGraphicsResult_t eEnd = sirD3D9NativeEndPass( &native );
		if ( eResult == SIR_GRAPHICS_SUCCESS ) {
			eResult = eEnd;
		}
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
		bool bComplete = false;
		for ( uint32_t iPoll = 0; iPoll < 1000 && !bComplete; ++iPoll ) {
			eResult = sirD3D9NativePoll( &native, &bComplete );
			if ( eResult != SIR_GRAPHICS_SUCCESS ) {
				break;
			}
			if ( !bComplete ) {
				Sleep( 1 );
			}
		}
		if ( !bComplete && eResult == SIR_GRAPHICS_SUCCESS ) {
			eResult = SIR_GRAPHICS_BUSY;
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
		sirGraphicsTextureDesc_t desc = {};
		desc.nWidth = 64;
		desc.nHeight = 64;
		desc.nDepth = 1;
		desc.nArrayLayers = 1;
		desc.nMipLevels = 2;
		desc.nSampleCount = 1;
		desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
		desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
		desc.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
		desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
		eResult = sirD3D9NativeCreateTrackedTextureDesc( &native, &desc, &hTexture );
	}
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		void *pResource = nullptr;
		eResult = sirD3D9NativeLookupResource( &native, hTexture, &pResource );
		if ( eResult == SIR_GRAPHICS_SUCCESS ) {
			uint32_t szPixels[64 * 64] = {};
			for ( uint32_t& color : szPixels ) {
				color = 0xff306090U;
			}
			sirD3D9TextureUpload_t upload = {};
			upload.pTexture = ( IDirect3DTexture9 * )pResource;
			upload.pData = szPixels;
			upload.nBytes = sizeof( szPixels );
			upload.nMipLevel = 0;
			upload.nRowPitch = 64 * sizeof( uint32_t );
			eResult = sirD3D9NativeUploadTexture( &native, &upload );
			if ( eResult == SIR_GRAPHICS_SUCCESS ) {
				upload.nRowPitch = 1;
				if ( sirD3D9NativeUploadTexture( &native, &upload ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
					eResult = SIR_GRAPHICS_DRIVER_ERROR;
				}
			}
		}
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
