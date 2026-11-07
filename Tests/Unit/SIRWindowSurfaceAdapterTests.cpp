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
#include <Renderer/SIRWindowSurfaceAdapter.h>
#include <stdio.h>

class SIRWindowAdapterTestDriver final : public SIRWindowDriver
{
public:
	SIRWindowAdapterTestDriver( void ) noexcept = default;
	~SIRWindowAdapterTestDriver( void ) noexcept override = default;
	sirWindowResult_t initialize( void ) noexcept override
	{
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pView ) noexcept override
	{
		m_nSlot = nSlot;
		m_nWidth = desc.nWidth;
		m_nHeight = desc.nHeight;
		m_bPendingConfigure = m_ePlatform == SIR_WINDOW_PLATFORM_WAYLAND;
		*pView = { m_ePlatform, nSlot + 1, 0xAA, 0 };
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t destroyWindow( uint32_t nSlot ) noexcept override
	{
		( void )nSlot;
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept override
	{
		( void )nSlot;
		( void )mutation;
		return SIR_WINDOW_OK;
	}

	sirWindowResult_t pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept override
	{
		if ( m_bPendingConfigure && nBudget != 0 && pCallback != nullptr ) {
			m_bPendingConfigure = false;
			const sirWindowBackendEvent_t event = { m_nSlot, m_nSlot + 1, SIR_WINDOW_EVENT_READY, m_nWidth, m_nHeight };
			pCallback( pContext, event );
		}
		return SIR_WINDOW_OK;
	}
	void shutdown( void ) noexcept override
	{
	}
	sirWindowPlatform_t m_ePlatform = SIR_WINDOW_PLATFORM_X11;
	uint32_t m_nSlot = 0;
	uint32_t m_nWidth = 0;
	uint32_t m_nHeight = 0;
	bool m_bPendingConfigure = false;

protected:
private:
};

static bool sirTestWindowNativeAdapter( sirWindowPlatform_t eBackend, sirGraphicsSurfacePlatform_t eGraphics ) noexcept
{
	SIRWindowAdapterTestDriver driver;
	driver.m_ePlatform = eBackend;
	SIRWindowSystem system( driver );
	sirWindowHandle_t handle = {};
	sirGraphicsNativeSurface_t native = {};
	sirWindowNativeLease_t lease = {};
	const sirWindowCreateDesc_t desc = { "adapter", 640, 480 };
	if ( system.initialize() != SIR_SYSTEM_OK ||
	     sirGraphicsAcquireWindowSurface( system, handle, &native, &lease ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     system.createWindow( desc, &handle ) != SIR_WINDOW_OK ||
	     sirGraphicsAcquireWindowSurface( system, handle, nullptr, &lease ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     ( eBackend == SIR_WINDOW_PLATFORM_WAYLAND &&
	       sirGraphicsAcquireWindowSurface( system, handle, &native, &lease ) != SIR_GRAPHICS_SURFACE_CHANGED ) ||
	     ( eBackend == SIR_WINDOW_PLATFORM_WAYLAND && system.pumpEvents( 1 ) != SIR_WINDOW_OK ) ||
	     sirGraphicsAcquireWindowSurface( system, handle, &native, &lease ) != SIR_GRAPHICS_SUCCESS || native.ePlatform != eGraphics ||
	     native.pWindow == nullptr || native.pDisplayOrInstance == nullptr || native.nWindowGeneration == 0 ||
	     system.destroyWindow( handle ) != SIR_WINDOW_BUSY || system.releaseNativeLease( lease ) != SIR_WINDOW_OK ||
	     system.destroyWindow( handle ) != SIR_WINDOW_OK || system.shutdown() != SIR_SYSTEM_OK ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestWindowNativeAdapter( SIR_WINDOW_PLATFORM_X11, SIR_GRAPHICS_SURFACE_X11 ) ||
	     !sirTestWindowNativeAdapter( SIR_WINDOW_PLATFORM_WIN32, SIR_GRAPHICS_SURFACE_WIN32 ) ||
	     !sirTestWindowNativeAdapter( SIR_WINDOW_PLATFORM_WAYLAND, SIR_GRAPHICS_SURFACE_WAYLAND ) ) {
		fputs( "SIRWindowSurfaceAdapterTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowSurfaceAdapterTests PASS" );
	return 0;
}
