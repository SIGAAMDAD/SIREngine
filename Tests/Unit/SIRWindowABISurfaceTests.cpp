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
#include <Renderer/SIRGraphicsDriverABI.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

// Window service changes must never silently modify the existing public
// graphics native-surface ABI, binary version, or presentation descriptor.
static_assert( SIR_GRAPHICS_DRIVER_ABI_MAJOR == 1U && SIR_GRAPHICS_DRIVER_ABI_MINOR == 0U, "Graphics ABI v1 must stay frozen" );
static_assert( sizeof( sirGraphicsNativeSurface_t ) == 32, "Graphics native surface layout changed" );
static_assert( sizeof( sirGraphicsABINativeSurface_t ) == 32, "Graphics ABI native surface layout changed" );
static_assert( offsetof( sirGraphicsABINativeSurface_t, nWindowGeneration ) == 24, "Graphics ABI generation layout changed" );
static_assert( sizeof( sirGraphicsABIPresentDesc_t ) == 32, "Graphics ABI present layout changed" );
static_assert( sizeof( sirWindowHandle_t ) == 16, "Window handle identity layout changed" );
static_assert( sizeof( sirWindowNativeLease_t ) == 24, "Window lease identity layout changed" );

static bool s_bFailCreate = false;
static bool s_bFailResize = false;
static bool s_bBusyRetire = false;
static uint32_t s_nLastWidth = 0;
static uint32_t s_nLastHeight = 0;
static uint32_t s_nCreates = 0;
static uint32_t s_nRetires = 0;
static uint32_t s_nAcquires = 0;
static uint32_t s_nPresents = 0;
static uint32_t s_nResizes = 0;
static bool s_bBusyPresent = false;
static uint64_t s_nNativeGeneration = 0;

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
	s_nNativeGeneration = pNative->nWindowGeneration;
	*pSurface = { 1, 2, 3, SIR_GRAPHICS_HANDLE_SURFACE };
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
	++s_nResizes;
	s_nLastWidth = pPresent->nWidth;
	s_nLastHeight = pPresent->nHeight;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsAcquireABISurfaceImage( const sirGraphicsABIDevice_t *pDevice, sirGraphicsHandle_t hSurface,
                                                          sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	if ( pDevice == nullptr || pImage == nullptr || hSurface.eType != SIR_GRAPHICS_HANDLE_SURFACE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	++s_nAcquires;
	*pImage = { hSurface, { 2, 3, 3, SIR_GRAPHICS_HANDLE_TEXTURE }, ( uint64_t )s_nAcquires, s_nNativeGeneration };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsPresentABISurfaceImage( const sirGraphicsABIDevice_t *pDevice, const sirGraphicsSurfaceImage_t *pImage,
                                                          const sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pDevice == nullptr || pImage == nullptr || pCompletion == nullptr || pImage->nAcquisitionId == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( s_bBusyPresent ) {
		return SIR_GRAPHICS_BUSY;
	}
	++s_nPresents;
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
	sirGraphicsSurfaceImage_t acquired = {};
	sirGraphicsCompletion_t rendered = {};
	sirGraphicsCompletionSet_t empty = {};
	if ( surface.presentImage( &rendered ) != SIR_GRAPHICS_INVALID_HANDLE || surface.acquireImage( &acquired ) != SIR_GRAPHICS_SUCCESS ||
	     acquired.nWindowGeneration != s_nNativeGeneration || surface.acquireImage( &acquired ) != SIR_GRAPHICS_BUSY ||
	     surface.resize( &present ) != SIR_GRAPHICS_BUSY || surface.retire( &empty ) != SIR_GRAPHICS_BUSY ) {
		return false;
	}
	s_bBusyPresent = true;
	if ( surface.presentImage( &rendered ) != SIR_GRAPHICS_BUSY ) {
		return false;
	}
	s_bBusyPresent = false;
	if ( surface.presentImage( &rendered ) != SIR_GRAPHICS_SUCCESS || surface.presentImage( &rendered ) != SIR_GRAPHICS_INVALID_HANDLE ||
	     s_nAcquires != 1 || s_nPresents != 1 ) {
		return false;
	}
	// Same-size present-mode changes are substantive and must call the backend.
	if ( surface.resize( &present ) != SIR_GRAPHICS_SUCCESS || s_nResizes != 0 ) {
		return false;
	}
	sirGraphicsABIPresentDesc_t changed = present;
	changed.nMode = 1;
	if ( surface.resize( &changed ) != SIR_GRAPHICS_SUCCESS || s_nResizes != 1 || surface.resize( &changed ) != SIR_GRAPHICS_SUCCESS ||
	     s_nResizes != 1 ) {
		return false;
	}
	present = changed;
	const uint64_t nOldMetrics = surface.getMetricsRevision();
	driver.queueResize( 1280, 720 );
	if ( windows.pumpEvents( 1 ) != SIR_WINDOW_OK || surface.getMetricsRevision() != nOldMetrics ) {
		return false;
	}
	s_bFailResize = true;
	if ( surface.acquireImage( &acquired ) != SIR_GRAPHICS_SURFACE_CHANGED || surface.resize( &present ) != SIR_GRAPHICS_BUSY ||
	     surface.getMetricsRevision() != nOldMetrics ) {
		return false;
	}
	s_bFailResize = false;
	if ( surface.resize( &present ) != SIR_GRAPHICS_SUCCESS || s_nLastWidth != 1280 || s_nLastHeight != 720 ||
	     surface.getMetricsRevision() <= nOldMetrics ) {
		return false;
	}
	if ( surface.acquireImage( &acquired ) != SIR_GRAPHICS_SUCCESS || surface.presentImage( &rendered ) != SIR_GRAPHICS_SUCCESS ||
	     s_nAcquires != 2 || s_nPresents != 2 ) {
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
