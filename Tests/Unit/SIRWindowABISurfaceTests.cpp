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
#include <Renderer/SIRWindowABISurface.h>
#include <stdio.h>

static bool s_bFailCreate = false;
static bool s_bFailResize = false;
static bool s_bBusyRetire = false;
static uint32_t s_nLastWidth = 0;
static uint32_t s_nLastHeight = 0;
static uint32_t s_nCreates = 0;
static uint32_t s_nRetires = 0;

sirGraphicsABIResult_t sirGraphicsCreateABISurface( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsABINativeSurface_t *pNative,
                                                    const sirGraphicsABIPresentDesc_t *pPresent, sirGraphicsHandle_t *pSurface ) noexcept
{
	if ( pDevice == nullptr || pNative->nWindowGeneration == 0 || pNative->pWindow == nullptr || pNative->pDisplayOrInstance == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_bFailCreate ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	++s_nCreates;
	s_nLastWidth = pPresent->nWidth;
	s_nLastHeight = pPresent->nHeight;
	*pSurface = { 1, 2, 3, 4 };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsResizeABISurface( const sirGraphicsABIDevice_t *pDevice, sirGraphicsHandle_t hSurface,
                                                    const sirGraphicsABINativeSurface_t *pNative,
                                                    const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	if ( pDevice == nullptr || hSurface.nGeneration == 0 || pNative->nWindowGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_bFailResize ) {
		return SIR_GRAPHICS_BUSY;
	}
	s_nLastWidth = pPresent->nWidth;
	s_nLastHeight = pPresent->nHeight;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsRetireABIResource( const sirGraphicsABIDevice_t *pDevice, sirGraphicsHandle_t hSurface,
                                                     const sirGraphicsCompletionSet_t *pComplete ) noexcept
{
	if ( pDevice == nullptr || hSurface.nGeneration == 0 || pComplete == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_bBusyRetire ) {
		return SIR_GRAPHICS_BUSY;
	}
	++s_nRetires;
	return SIR_GRAPHICS_SUCCESS;
}

class SIRWindowABIStubDriver final : public SIRWindowDriver
{
public:
	SIRWindowABIStubDriver( void ) noexcept = default;
	~SIRWindowABIStubDriver( void ) noexcept override = default;

	sirWindowResult_t initialize( void ) noexcept override
	{
		return SIR_WINDOW_OK;
	}
	sirWindowResult_t createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot, sirWindowNativeView_t *pNative ) noexcept override
	{
		( void )desc;
		*pNative = { SIR_WINDOW_PLATFORM_X11, nSlot + 1, 0x12, 0 };
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
		if ( m_bResize && nBudget != 0 && pCallback != nullptr ) {
			m_bResize = false;
			pCallback( pContext, { 0, 1, SIR_WINDOW_EVENT_RESIZED, m_nWidth, m_nHeight } );
		}
		return SIR_WINDOW_OK;
	}
	void shutdown( void ) noexcept override
	{
	}

	void queueResize( uint32_t nWidth, uint32_t nHeight ) noexcept
	{
		m_bResize = true;
		m_nWidth = nWidth;
		m_nHeight = nHeight;
	}

protected:
private:
	uint32_t m_nWidth = 0;
	uint32_t m_nHeight = 0;
	bool m_bResize = false;
};

static bool sirTestGPUWindowLeaseLifetime( void ) noexcept
{
	SIRWindowABIStubDriver driver;
	SIRWindowSystem windows( driver );
	if ( windows.initialize() != SIR_SYSTEM_OK ) {
		return false;
	}
	sirWindowHandle_t hWindow = {};
	if ( windows.createWindow( { "GPU", 640, 480 }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	SIRWindowABISurface surface;
	sirGraphicsABIDevice_t device = {};
	sirGraphicsABIPresentDesc_t present = {};
	present.nSize = sizeof( present );
	present.nWidth = 100;
	present.nHeight = 100;
	s_bFailCreate = true;
	if ( surface.create( windows, hWindow, &device, &present ) != SIR_GRAPHICS_DRIVER_ERROR || surface.isActive() ||
	     windows.destroyWindow( hWindow ) != SIR_WINDOW_OK || windows.createWindow( { "GPU2", 640, 480 }, &hWindow ) != SIR_WINDOW_OK ) {
		return false;
	}
	s_bFailCreate = false;
	if ( surface.create( windows, hWindow, &device, &present ) != SIR_GRAPHICS_SUCCESS || !surface.isActive() || s_nLastWidth != 640 ||
	     s_nLastHeight != 480 || windows.destroyWindow( hWindow ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	const uint64_t nOldMetrics = surface.getMetricsRevision();
	driver.queueResize( 1280, 720 );
	if ( windows.pumpEvents( 1 ) != SIR_WINDOW_OK || surface.getMetricsRevision() != nOldMetrics ) {
		return false;
	}
	s_bFailResize = true;
	if ( surface.resize( &present ) != SIR_GRAPHICS_BUSY || surface.getMetricsRevision() != nOldMetrics ) {
		return false;
	}
	s_bFailResize = false;
	if ( surface.resize( &present ) != SIR_GRAPHICS_SUCCESS || s_nLastWidth != 1280 || s_nLastHeight != 720 ||
	     surface.getMetricsRevision() <= nOldMetrics ) {
		return false;
	}
	sirGraphicsCompletionSet_t completed = {};
	s_bBusyRetire = true;
	if ( surface.retire( &completed ) != SIR_GRAPHICS_BUSY || !surface.isActive() || windows.destroyWindow( hWindow ) != SIR_WINDOW_BUSY ) {
		return false;
	}
	s_bBusyRetire = false;
	if ( surface.retire( &completed ) != SIR_GRAPHICS_SUCCESS || surface.isActive() ||
	     surface.retire( &completed ) != SIR_GRAPHICS_INVALID_HANDLE || windows.destroyWindow( hWindow ) != SIR_WINDOW_OK ||
	     windows.shutdown() != SIR_SYSTEM_OK || s_nCreates != 1 || s_nRetires != 1 ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !sirTestGPUWindowLeaseLifetime() ) {
		fputs( "SIRWindowABISurfaceTests FAIL\n", stderr );
		return 1;
	}
	puts( "SIRWindowABISurfaceTests PASS" );
	return 0;
}
