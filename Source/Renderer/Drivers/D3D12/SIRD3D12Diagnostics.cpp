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

#include <Renderer/Drivers/D3D12/SIRD3D12Diagnostics.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>

sirGraphicsResult_t sirD3D12ClassifyStatus( HRESULT nResult ) noexcept
{
	if ( SUCCEEDED( nResult ) ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( nResult == DXGI_ERROR_DEVICE_REMOVED || nResult == DXGI_ERROR_DEVICE_RESET || nResult == DXGI_ERROR_DEVICE_HUNG ||
	     nResult == DXGI_ERROR_DRIVER_INTERNAL_ERROR ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( nResult == E_INVALIDARG || nResult == E_POINTER ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nResult == E_OUTOFMEMORY ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	if ( nResult == HRESULT_FROM_WIN32( ERROR_TIMEOUT ) || nResult == DXGI_ERROR_WAIT_TIMEOUT ) {
		return SIR_GRAPHICS_TIMEOUT;
	}
	return SIR_GRAPHICS_DRIVER_ERROR;
}

SIRD3D12Diagnostics::~SIRD3D12Diagnostics( void ) noexcept
{
	detachDevice();
}

void SIRD3D12Diagnostics::resetDredConfiguration( void ) noexcept
{
	if ( !m_bDredConfigurationPending ) {
		return;
	}
	Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pSettings;
	if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( pSettings.GetAddressOf() ) ) ) ) {
		pSettings->SetAutoBreadcrumbsEnablement( D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED );
		pSettings->SetPageFaultEnablement( D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED );
	}
	m_bDredConfigurationPending = false;
}

sirGraphicsResult_t SIRD3D12Diagnostics::configureBeforeDevice( const sirD3D12DiagnosticConfig_t& config ) noexcept
{
	if ( m_pDevice != nullptr || m_bDredConfigurationPending ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( config.bEnableGpuValidation && !config.bEnableDebugLayer ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	m_Snapshot = {};
	if ( config.bEnableDred ) {
		Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pSettings;
		if ( FAILED( D3D12GetDebugInterface( IID_PPV_ARGS( pSettings.GetAddressOf() ) ) ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		pSettings->SetAutoBreadcrumbsEnablement( D3D12_DRED_ENABLEMENT_FORCED_ON );
		pSettings->SetPageFaultEnablement( D3D12_DRED_ENABLEMENT_FORCED_ON );
		m_bDredConfigurationPending = true;
		m_Snapshot.bDredEnabled = true;
	}
	if ( config.bEnableDebugLayer ) {
		Microsoft::WRL::ComPtr<ID3D12Debug> pDebug;
		if ( FAILED( D3D12GetDebugInterface( IID_PPV_ARGS( pDebug.GetAddressOf() ) ) ) ) {
			resetDredConfiguration();
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		pDebug->EnableDebugLayer();
		m_Snapshot.bDebugLayerEnabled = true;
		if ( config.bEnableGpuValidation ) {
			Microsoft::WRL::ComPtr<ID3D12Debug1> pDebug1;
			if ( FAILED( pDebug.As( &pDebug1 ) ) ) {
				resetDredConfiguration();
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			pDebug1->SetEnableGPUBasedValidation( TRUE );
			m_Snapshot.bGpuValidationEnabled = true;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12Diagnostics::attachDevice( ID3D12Device *pDevice ) noexcept
{
	m_pDevice = pDevice;
	resetDredConfiguration();
}

void SIRD3D12Diagnostics::detachDevice( void ) noexcept
{
	resetDredConfiguration();
	m_pDevice = nullptr;
}

void SIRD3D12Diagnostics::captureDred( void ) noexcept
{
	if ( m_pDevice == nullptr || !m_Snapshot.bDredEnabled || m_Snapshot.bDredCaptured ) {
		return;
	}
	Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
	if ( FAILED( m_pDevice->QueryInterface( IID_PPV_ARGS( pDred.GetAddressOf() ) ) ) ) {
		return;
	}
	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs = {};
	if ( SUCCEEDED( pDred->GetAutoBreadcrumbsOutput( &breadcrumbs ) ) ) {
		const D3D12_AUTO_BREADCRUMB_NODE *pNode = breadcrumbs.pHeadAutoBreadcrumbNode;
		while ( pNode != nullptr && m_Snapshot.nBreadcrumbNodes < 128 ) {
			++m_Snapshot.nBreadcrumbNodes;
			m_Snapshot.nBreadcrumbOperations += pNode->BreadcrumbCount;
			if ( pNode->pLastBreadcrumbValue != nullptr ) {
				const uint32_t nDone = *pNode->pLastBreadcrumbValue;
				m_Snapshot.nCompletedBreadcrumbOperations += nDone < pNode->BreadcrumbCount ? nDone : pNode->BreadcrumbCount;
			}
			pNode = pNode->pNext;
		}
		m_Snapshot.bDredCaptured = true;
	}
	D3D12_DRED_PAGE_FAULT_OUTPUT faults = {};
	if ( SUCCEEDED( pDred->GetPageFaultAllocationOutput( &faults ) ) ) {
		m_Snapshot.nPageFaultAddress = faults.PageFaultVA;
		for ( const D3D12_DRED_ALLOCATION_NODE *pNode = faults.pHeadExistingAllocationNode;
		      pNode != nullptr && m_Snapshot.nExistingAllocationNodes < 128; pNode = pNode->pNext ) {
			++m_Snapshot.nExistingAllocationNodes;
		}
		for ( const D3D12_DRED_ALLOCATION_NODE *pNode = faults.pHeadRecentFreedAllocationNode;
		      pNode != nullptr && m_Snapshot.nRecentlyFreedAllocationNodes < 128; pNode = pNode->pNext ) {
			++m_Snapshot.nRecentlyFreedAllocationNodes;
		}
		m_Snapshot.bDredCaptured = true;
	}
}

void SIRD3D12Diagnostics::captureStatus( HRESULT nResult ) noexcept
{
	m_Snapshot.nLastHRESULT = ( int32_t )nResult;
	if ( m_pDevice == nullptr ) {
		return;
	}
	const HRESULT nReason = m_pDevice->GetDeviceRemovedReason();
	m_Snapshot.nDeviceRemovedReason = ( int32_t )nReason;
	if ( FAILED( nReason ) ) {
		m_Snapshot.bDeviceRemoved = true;
		captureDred();
	} else if ( sirD3D12ClassifyStatus( nResult ) == SIR_GRAPHICS_DEVICE_LOST ) {
		m_Snapshot.bDeviceRemoved = true;
		captureDred();
	}
}

bool SIRD3D12Diagnostics::checkHealth( void ) noexcept
{
	if ( m_pDevice == nullptr ) {
		return false;
	}
	const HRESULT nReason = m_pDevice->GetDeviceRemovedReason();
	if ( FAILED( nReason ) ) {
		captureStatus( nReason );
		return false;
	}
	return !m_Snapshot.bDeviceRemoved;
}

bool SIRD3D12Diagnostics::querySnapshot( sirD3D12DiagnosticSnapshot_t *pSnapshot ) noexcept
{
	if ( pSnapshot == nullptr ) {
		return false;
	}
	if ( m_pDevice != nullptr ) {
		( void )checkHealth();
		if ( m_Snapshot.bDebugLayerEnabled ) {
			Microsoft::WRL::ComPtr<ID3D12InfoQueue> pMessages;
			if ( SUCCEEDED( m_pDevice->QueryInterface( IID_PPV_ARGS( pMessages.GetAddressOf() ) ) ) ) {
				m_Snapshot.nStoredDebugMessages = pMessages->GetNumStoredMessages();
			}
		}
	}
	*pSnapshot = m_Snapshot;
	return m_pDevice != nullptr || m_Snapshot.bDeviceRemoved;
}
