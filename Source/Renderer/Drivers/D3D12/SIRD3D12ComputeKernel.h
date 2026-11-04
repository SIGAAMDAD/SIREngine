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

#ifndef __SIRENGINE_SIRD3D12COMPUTEKERNEL_H__
#define __SIRENGINE_SIRD3D12COMPUTEKERNEL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/D3D12/SIRD3D12Memory.h>
#include <wrl/client.h>

// Native smoke kernel. This owns its pipeline and an at-most-one-in-flight
// UAV/readback pair. The shared shader cooker will replace its test-specific
// binding contract without changing the compute queue's fence ownership.
class SIRD3D12ComputeKernel
{
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pUavHeap;
	D3D12MA::Allocation *m_pOutput = nullptr;
	D3D12MA::Allocation *m_pReadback = nullptr;
	uint64_t m_nSubmissionFence = 0;
	uint32_t m_nElementCount = 0;
	HRESULT m_nLastError = S_OK;

	void clearPending( void ) noexcept;

public:
	SIRD3D12ComputeKernel( void ) noexcept;
	~SIRD3D12ComputeKernel( void ) noexcept;
	SIRD3D12ComputeKernel( const SIRD3D12ComputeKernel & ) = delete;
	SIRD3D12ComputeKernel &operator=( const SIRD3D12ComputeKernel & ) = delete;

	bool initialize( ID3D12Device *pDevice, const void *pDxil, size_t nDxilBytes ) noexcept;
	bool isReady( void ) const noexcept;
	bool hasPending( void ) const noexcept;
	bool record( ID3D12Device *pDevice, SIRD3D12Memory *pMemory, ID3D12GraphicsCommandList *pCommands, uint32_t nElements,
	             uint32_t nSeed ) noexcept;
	void submitted( uint64_t nFence ) noexcept;
	bool readback( ID3D12Fence *pFence, uint64_t nFence, uint32_t *pOutput, uint32_t nCapacity ) noexcept;
	void cancelUnsubmitted( void ) noexcept;
	void shutdown( bool bGpuCompleted ) noexcept;
	HRESULT getLastError( void ) const noexcept;
};

#endif // __SIRENGINE_SIRD3D12COMPUTEKERNEL_H__
