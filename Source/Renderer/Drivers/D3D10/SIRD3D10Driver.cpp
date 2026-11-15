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

#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRGraphicsTypes.h>
#include <new>
#include <stdint.h>
#include <string.h>

#if defined( _WIN32 ) && defined( SIR_D3D10_EXPORTS )
#define SIR_D3D10_PUBLIC __declspec( dllexport )
#else
#define SIR_D3D10_PUBLIC
#endif

// This bootstrap advertises no adapters until native DXGI and device creation
// are implemented. It must not be selected as a working graphics provider.
// The module ABI exposes an opaque pointer; native state stays owned by the
// host allocator even though the implementation uses C++ object lifetimes.
class SIRD3D10DriverState final
{
public:
	explicit SIRD3D10DriverState( const sirGraphicsABIHost_t& host ) noexcept : m_host( host )
	{
	}
	SIRD3D10DriverState( const SIRD3D10DriverState& ) = delete;
	SIRD3D10DriverState& operator=( const SIRD3D10DriverState& ) = delete;
	SIRD3D10DriverState( SIRD3D10DriverState&& ) = delete;
	SIRD3D10DriverState& operator=( SIRD3D10DriverState&& ) = delete;
	~SIRD3D10DriverState( void ) noexcept = default;

	void release( void ) noexcept
	{
		const sirGraphicsABIHost_t host = m_host;
		this->~SIRD3D10DriverState();
		host.pFree( host.pContext, this );
	}

private:
	sirGraphicsABIHost_t m_host;
};

static_assert( sizeof( SIRD3D10DriverState ) == sizeof( sirGraphicsABIHost_t ), "D3D10 state allocation size drift" );
static_assert( alignof( SIRD3D10DriverState ) == alignof( sirGraphicsABIHost_t ), "D3D10 state alignment drift" );

// Owns an uncommitted host allocation until the state has been constructed
// and successfully published through the existing ABI output table.
class SIRD3D10PendingAllocation final
{
public:
	SIRD3D10PendingAllocation( const sirGraphicsABIHost_t& host, void *pMemory ) noexcept : m_host( host ), m_pMemory( pMemory )
	{
	}
	SIRD3D10PendingAllocation( const SIRD3D10PendingAllocation& ) = delete;
	SIRD3D10PendingAllocation& operator=( const SIRD3D10PendingAllocation& ) = delete;
	SIRD3D10PendingAllocation( SIRD3D10PendingAllocation&& ) = delete;
	SIRD3D10PendingAllocation& operator=( SIRD3D10PendingAllocation&& ) = delete;

	~SIRD3D10PendingAllocation( void ) noexcept
	{
		if ( m_pMemory != nullptr ) {
			m_host.pFree( m_host.pContext, m_pMemory );
		}
	}

	void release( void ) noexcept
	{
		m_pMemory = nullptr;
	}

private:
	const sirGraphicsABIHost_t& m_host;
	void *m_pMemory;
};

static uint32_t sirD3D10AdapterCount( void *pDriver, uint32_t *pCount ) noexcept
{
	if ( pDriver == nullptr || pCount == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCount = 0;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirD3D10QueryAdapter( void *pDriver, sirGraphicsABIAdapter_t *pAdapter ) noexcept
{
	if ( pDriver == nullptr || pAdapter == nullptr || pAdapter->nSize < sizeof( sirGraphicsABIAdapter_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirD3D10CreateDevice( void *pDriver, const sirGraphicsABIDeviceRequest_t *pRequest,
                                      sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDriver == nullptr || pRequest == nullptr || pDevice == nullptr || pRequest->nSize < sizeof( sirGraphicsABIDeviceRequest_t ) ||
	     pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) || pRequest->nReserved != 0 || pRequest->nFramesInFlight == 0 ||
	     pRequest->nAllowSoftware > 1 || pRequest->nHeadless > 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint64_t SIR_D3D10_FEATURES = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_GEOMETRY_SHADER |
	                                        SIR_GRAPHICS_FEATURE_MRT | SIR_GRAPHICS_FEATURE_MULTISAMPLE | SIR_GRAPHICS_FEATURE_PRESENT;
	if ( ( pRequest->nRequiredFeatures & ~SIR_D3D10_FEATURES ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	// No native device is returned until D10-01 supplies a complete callback table.
	return SIR_GRAPHICS_UNSUPPORTED;
}

static uint32_t sirD3D10DestroyDevice( void *pDriver, sirGraphicsABIDevice_t *pDevice ) noexcept
{
	if ( pDriver == nullptr || pDevice == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}

static void sirD3D10ReleaseDriver( void *pDriver ) noexcept
{
	if ( pDriver == nullptr ) {
		return;
	}
	SIRD3D10DriverState *pState = ( SIRD3D10DriverState * )pDriver;
	pState->release();
}

extern "C" SIR_D3D10_PUBLIC uint32_t SIRGetGraphicsDriverABI( uint32_t nMajor, uint32_t nMinor, const sirGraphicsABIHost_t *pHost,
                                                              sirGraphicsABIDriver_t *pDriver ) noexcept
{
	if ( pDriver == nullptr || pDriver->nSize < sizeof( sirGraphicsABIDriver_t ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nMajor != SIR_GRAPHICS_DRIVER_ABI_MAJOR || nMinor > SIR_GRAPHICS_DRIVER_ABI_MINOR ) {
		return SIR_GRAPHICS_VERSION_MISMATCH;
	}
	const sirGraphicsABIResult_t eHostStatus = sirGraphicsValidateABIHost( pHost );
	if ( eHostStatus != SIR_GRAPHICS_SUCCESS ) {
		return eHostStatus;
	}
	void *pMemory = pHost->pAllocate( pHost->pContext, sizeof( SIRD3D10DriverState ), alignof( SIRD3D10DriverState ) );
	if ( pMemory == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	SIRD3D10PendingAllocation allocation( *pHost, pMemory );
	if ( ( ( uintptr_t )pMemory % alignof( SIRD3D10DriverState ) ) != 0 ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	SIRD3D10DriverState *pState = new ( pMemory ) SIRD3D10DriverState( *pHost );

	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	driver.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	driver.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	driver.nBackend = SIR_GRAPHICS_BACKEND_D3D10;
	driver.pDriver = pState;
	driver.pAdapterCount = sirD3D10AdapterCount;
	driver.pQueryAdapter = sirD3D10QueryAdapter;
	driver.pCreateDevice = sirD3D10CreateDevice;
	driver.pDestroyDevice = sirD3D10DestroyDevice;
	driver.pReleaseDriver = sirD3D10ReleaseDriver;
	memcpy( pDriver, &driver, sizeof( driver ) );
	allocation.release();
	return SIR_GRAPHICS_SUCCESS;
}
