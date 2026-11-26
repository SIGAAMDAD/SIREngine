/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_D3_D12_SURFACES_H__
#define __SIR_D3_D12_SURFACES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/D3D12/SIRD3D12Resources.h>

// The platform owns HWND lifetime and pumps its own events. The backend
// owns only a bounded DXGI swapchain and imported, non-D3D12MA backbuffers.
typedef struct sirD3D12SurfaceSlot_s
{
	Microsoft::WRL::ComPtr<IDXGISwapChain3> pSwapchain;
	sirGraphicsHandle_t szImages[3];
	sirGraphicsCompletion_t renderCompletion;
	HWND hWindow = nullptr;
	uint64_t nWindowGeneration = 0;
	uint64_t nAcquisitionCounter = 1;
	uint64_t nAcquisitionId = 0;
	uint64_t nLastDirectFence = 0;
	uint32_t nGeneration = 0;
	uint32_t nBufferCount = 0;
	uint32_t nWidth = 0;
	uint32_t nHeight = 0;
	uint32_t iAcquiredImage = 0;
	SIRGraphicsFormat eFormat = SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN;
	SIRGraphicsPresentMode eMode = SIRGraphicsPresentMode::SIR_GRAPHICS_PRESENT_FIFO;
	bool bAllowTearing = false;
	bool bLive = false;
	bool bAcquired = false;
	bool bSubmitted = false;
} sirD3D12SurfaceSlot_t;

class SIRD3D12Surfaces
{
private:
	static constexpr uint32_t s_nSurfaceCapacity = 2;
	sirD3D12SurfaceSlot_t m_szSlots[s_nSurfaceCapacity];
	SIRD3D12Device *m_pNative = nullptr;
	SIRD3D12Resources *m_pResources = nullptr;
	IDXGIFactory6 *m_pFactory = nullptr;
	ID3D12CommandQueue *m_pDirectQueue = nullptr;
	uint32_t m_nDeviceGeneration = 0;
	bool m_bTearingSupported = false;

	sirD3D12SurfaceSlot_t *findSlot( sirGraphicsHandle_t hSurface ) noexcept;
	SIRGraphicsResult validateWindow( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& present ) const noexcept;
	SIRGraphicsResult releaseImages( sirD3D12SurfaceSlot_t& slot ) noexcept;
	SIRGraphicsResult importImages( sirD3D12SurfaceSlot_t& slot ) noexcept;
	SIRGraphicsResult createSwapchain( sirD3D12SurfaceSlot_t& slot, const sirGraphicsNativeSurface_t& native,
	                                     const sirGraphicsPresentDesc_t& present ) noexcept;
	bool isDirectComplete( uint64_t nFence ) noexcept;
	bool imageMatches( const sirD3D12SurfaceSlot_t& slot, const sirGraphicsSurfaceImage_t& image ) const noexcept;

public:
	SIRD3D12Surfaces( void ) noexcept = default;
	~SIRD3D12Surfaces( void ) noexcept;
	SIRD3D12Surfaces( const SIRD3D12Surfaces& ) = delete;
	SIRD3D12Surfaces& operator=( const SIRD3D12Surfaces& ) = delete;
	bool initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, uint32_t nGeneration ) noexcept;
	void shutdown( bool bGpuComplete ) noexcept;
	SIRGraphicsResult createSurface( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& present,
	                                   sirGraphicsHandle_t *pSurface ) noexcept;
	SIRGraphicsResult resizeSurface( sirGraphicsHandle_t hSurface, const sirGraphicsNativeSurface_t& native,
	                                   const sirGraphicsPresentDesc_t& present ) noexcept;
	SIRGraphicsResult acquireSurfaceImage( sirGraphicsHandle_t hSurface, sirGraphicsSurfaceImage_t *pImage ) noexcept;
	SIRGraphicsResult validateSubmission( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission ) noexcept;
	void completeSubmission( const sirGraphicsSubmission_t& submission, const sirGraphicsCompletion_t& completion ) noexcept;
	SIRGraphicsResult present( const sirGraphicsSurfaceImage_t& image, const sirGraphicsCompletion_t& renderComplete ) noexcept;
	SIRGraphicsResult retireSurface( sirGraphicsHandle_t hSurface, const sirGraphicsCompletionSet_t& completion ) noexcept;
};

#endif // __SIR_D3_D12_SURFACES_H__
