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

#ifndef __SIRENGINE_SIRD3D12DIAGNOSTICS_H__
#define __SIRENGINE_SIRD3D12DIAGNOSTICS_H__

#include <SIRPCH.hpp>

// clang-format off


#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <d3d12.h>
#include <dxgi1_6.h>
#include <Renderer/SIRGraphicsTypes.h>

typedef struct sirD3D12DiagnosticConfig_s
{
	bool bEnableDebugLayer;
	bool bEnableGpuValidation;
	bool bEnableDred;
} sirD3D12DiagnosticConfig_t;

typedef struct sirD3D12DiagnosticSnapshot_s
{
	uint64_t nStoredDebugMessages;
	uint64_t nPageFaultAddress;
	uint32_t nBreadcrumbNodes;
	uint32_t nBreadcrumbOperations;
	uint32_t nCompletedBreadcrumbOperations;
	uint32_t nExistingAllocationNodes;
	uint32_t nRecentlyFreedAllocationNodes;
	int32_t nDeviceRemovedReason;
	int32_t nLastHRESULT;
	bool bDebugLayerEnabled;
	bool bGpuValidationEnabled;
	bool bDredEnabled;
	bool bDeviceRemoved;
	bool bDredCaptured;
} sirD3D12DiagnosticSnapshot_t;

// Single-device diagnostic owner. DRED configuration is process-global and
// is restored after device creation; debug layer configuration is process
// global and cannot be undone while devices remain alive.
class SIRD3D12Diagnostics
{
private:
	ID3D12Device *m_pDevice = nullptr;
	sirD3D12DiagnosticSnapshot_t m_Snapshot = {};
	bool m_bDredConfigurationPending = false;

	void captureDred( void ) noexcept;
	void resetDredConfiguration( void ) noexcept;

public:
	SIRD3D12Diagnostics( void ) noexcept = default;
	SIRD3D12Diagnostics( const SIRD3D12Diagnostics& ) = delete;
	SIRD3D12Diagnostics& operator=( const SIRD3D12Diagnostics& ) = delete;
	~SIRD3D12Diagnostics( void ) noexcept;

	sirGraphicsResult_t configureBeforeDevice( const sirD3D12DiagnosticConfig_t& config ) noexcept;
	void attachDevice( ID3D12Device *pDevice ) noexcept;
	void detachDevice( void ) noexcept;
	void captureStatus( HRESULT nResult ) noexcept;
	bool checkHealth( void ) noexcept;
	bool querySnapshot( sirD3D12DiagnosticSnapshot_t *pSnapshot ) noexcept;
};

// Pure status translation, also used by tests without forcing real GPU loss.
sirGraphicsResult_t sirD3D12ClassifyStatus( HRESULT nResult ) noexcept;

#endif // __SIRENGINE_SIRD3D12DIAGNOSTICS_H__
