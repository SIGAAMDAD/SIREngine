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

#ifndef __SIRENGINE_SIRD3D12GRAPHICSKERNEL_H__
#define __SIRENGINE_SIRD3D12GRAPHICSKERNEL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/D3D12/SIRD3D12Memory.h>
#include <wrl/client.h>

// Native headless graphics path: offline VS/PS DXIL, indexed geometry,
// RGBA8 render target, and fence-retired texture readback.
class SIRD3D12GraphicsKernel
{
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	D3D12MA::Allocation *m_pTarget = nullptr;
	D3D12MA::Allocation *m_pReadback = nullptr;
	D3D12MA::Allocation *m_pVertices = nullptr;
	D3D12MA::Allocation *m_pIndices = nullptr;
	const uint8_t *m_pReadbackMapped = nullptr;
	void *m_pVertexMapped = nullptr;
	void *m_pIndexMapped = nullptr;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_Footprint = {};
	uint32_t m_nCapacityWidth = 0;
	uint32_t m_nCapacityHeight = 0;
	bool m_bPending = false;
	uint64_t m_nSubmittedFence = 0;
	uint32_t m_nWidth = 0;
	uint32_t m_nHeight = 0;
	HRESULT m_nLastError = S_OK;

	void releasePending( void ) noexcept;

public:
	SIRD3D12GraphicsKernel( void ) noexcept;
	~SIRD3D12GraphicsKernel( void ) noexcept;
	SIRD3D12GraphicsKernel( const SIRD3D12GraphicsKernel & ) = delete;
	SIRD3D12GraphicsKernel &operator=( const SIRD3D12GraphicsKernel & ) = delete;

	bool initialize( ID3D12Device *pDevice, SIRD3D12Memory *pMemory, const void *pVertexDxil, size_t nVertexBytes, const void *pPixelDxil,
	                 size_t nPixelBytes, uint32_t nMaxWidth, uint32_t nMaxHeight ) noexcept;
	bool isReady( void ) const noexcept;
	bool hasPending( void ) const noexcept;
	bool queryCapacity( uint32_t *pWidth, uint32_t *pHeight ) const noexcept;
	bool record( uint32_t nWidth, uint32_t nHeight ) noexcept;
	ID3D12CommandList *getRecordedList( void ) const noexcept;
	void submitted( uint64_t nFence ) noexcept;
	bool readback( ID3D12Fence *pFence, uint64_t nFence, uint8_t *pOutput, uint32_t nRowPitch, size_t nOutputBytes ) noexcept;
	void cancelUnsubmitted( void ) noexcept;
	void shutdown( bool bGpuCompleted ) noexcept;
	HRESULT getLastError( void ) const noexcept;
};

#endif // __SIRENGINE_SIRD3D12GRAPHICSKERNEL_H__
