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

#include <Renderer/Drivers/D3D12/SIRD3D12Transfers.h>

#include <string.h>

SIRD3D12Transfers::SIRD3D12Transfers( void ) noexcept
{
}

SIRD3D12Transfers::~SIRD3D12Transfers( void ) noexcept
{
	shutdown();
}

bool SIRD3D12Transfers::initialize( ID3D12Device *pDevice, SIRD3D12Memory *pMemory, const sirD3D12TransferPoolConfig_t &config ) noexcept
{
	if ( isReady() || m_pQueue != nullptr || m_bSubmissionUnsafe || pDevice == nullptr || pMemory == nullptr || !pMemory->isReady() ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC queue = {};
	queue.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	queue.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	m_nLastError = pDevice->CreateCommandQueue( &queue, IID_PPV_ARGS( m_pQueue.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}

	m_nLastError = pDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( m_pFence.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown();
		return false;
	}

	m_hEvent = CreateEventW( nullptr, FALSE, FALSE, nullptr );
	if ( m_hEvent == nullptr ) {
		m_nLastError = HRESULT_FROM_WIN32( GetLastError() );
		shutdown();
		return false;
	}

	m_pMemory = pMemory;
	if ( !m_pMemory->initializeTransferArena( config, s_nSlotCount ) || !m_pMemory->queryTransferArena( &m_Arena ) ) {
		m_nLastError = m_pMemory->getLastError();
		shutdown();
		return false;
	}

	for ( uint32_t iSlot = 0; iSlot < s_nSlotCount; ++iSlot ) {
		sirD3D12TransferSlot_t *pSlot = &m_szSlots[iSlot];
		m_nLastError = pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS( pSlot->pAllocator.GetAddressOf() ) );
		if ( FAILED( m_nLastError ) ) {
			shutdown();
			return false;
		}
		m_nLastError = pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_COPY, pSlot->pAllocator.Get(), nullptr,
		                                           IID_PPV_ARGS( pSlot->pCommands.GetAddressOf() ) );
		if ( FAILED( m_nLastError ) ) {
			shutdown();
			return false;
		}
		m_nLastError = pSlot->pCommands->Close();
		if ( FAILED( m_nLastError ) ) {
			shutdown();
			return false;
		}
		if ( m_Arena.ePath == SIR_D3D12_TRANSFER_STAGED ) {
			m_nLastError =
			    pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS( pSlot->pReadbackAllocator.GetAddressOf() ) );
			if ( FAILED( m_nLastError ) ) {
				shutdown();
				return false;
			}
			m_nLastError = pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_COPY, pSlot->pReadbackAllocator.Get(), nullptr,
			                                           IID_PPV_ARGS( pSlot->pReadbackCommands.GetAddressOf() ) );
			if ( FAILED( m_nLastError ) ) {
				shutdown();
				return false;
			}
			m_nLastError = pSlot->pReadbackCommands->Close();
			if ( FAILED( m_nLastError ) ) {
				shutdown();
				return false;
			}
		}
	}

	m_nNextFence = 1;
	m_nLastSubmittedFence = 0;
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Transfers::shutdown( void ) noexcept
{
	bool bSafe = !m_bSubmissionUnsafe;
	if ( bSafe && m_nLastSubmittedFence != 0 && m_pFence != nullptr ) {
		bSafe = waitForFence( m_nLastSubmittedFence, 15000 );
	}

	for ( uint32_t iSlot = 0; iSlot < s_nSlotCount; ++iSlot ) {
		sirD3D12TransferSlot_t *pSlot = &m_szSlots[iSlot];
		if ( bSafe ) {
			pSlot->pCommands.Reset();
			pSlot->pAllocator.Reset();
			pSlot->pReadbackCommands.Reset();
			pSlot->pReadbackAllocator.Reset();
		} else {
			// Retain native command objects and the resident arena when the
			// GPU's last completion cannot be proved.
			( void )pSlot->pCommands.Detach();
			( void )pSlot->pAllocator.Detach();
			( void )pSlot->pReadbackCommands.Detach();
			( void )pSlot->pReadbackAllocator.Detach();
		}
		pSlot->bOccupied = false;
		pSlot->nFence = 0;
	}

	if ( bSafe ) {
		m_pFence.Reset();
		m_pQueue.Reset();
		if ( m_hEvent != nullptr ) {
			CloseHandle( m_hEvent );
		}
	} else {
		( void )m_pFence.Detach();
		( void )m_pQueue.Detach();
	}
	m_hEvent = nullptr;
	if ( bSafe && m_pMemory != nullptr ) {
		m_pMemory->releaseTransferArena( true );
	}
	m_Arena = {};
	m_pMemory = nullptr;
	m_nLastSubmittedFence = 0;
	m_bSubmissionUnsafe = !bSafe;
	return bSafe;
}

bool SIRD3D12Transfers::isReady( void ) const noexcept
{
	return m_pQueue != nullptr && m_pFence != nullptr && m_hEvent != nullptr && m_pMemory != nullptr && m_Arena.pWriteBytes != nullptr &&
	       m_Arena.pReadbackBytes != nullptr && !m_bSubmissionUnsafe;
}

uint32_t SIRD3D12Transfers::getSlotCount( void ) const noexcept
{
	return s_nSlotCount;
}

uint64_t SIRD3D12Transfers::getCompletedCopyCount( void ) const noexcept
{
	return m_nCompletedCopies;
}

bool SIRD3D12Transfers::queryResidentArena( sirD3D12ResidentTransferView_t *pView ) const noexcept
{
	if ( pView == nullptr || !isReady() || m_Arena.pWriteBytes == nullptr ) {
		return false;
	}
	*pView = m_Arena;
	return true;
}

uint64_t SIRD3D12Transfers::getLastSubmittedFence( void ) const noexcept
{
	return m_nLastSubmittedFence;
}

ID3D12CommandQueue *SIRD3D12Transfers::getSubmissionQueue( void ) const noexcept
{
	return isReady() ? m_pQueue.Get() : nullptr;
}

ID3D12Fence *SIRD3D12Transfers::getSubmissionFence( void ) const noexcept
{
	return isReady() ? m_pFence.Get() : nullptr;
}

bool SIRD3D12Transfers::submitRecorded( ID3D12CommandList *const *ppLists, uint32_t nCount, uint64_t *pFence ) noexcept
{
	if ( pFence == nullptr ) {
		return false;
	}
	*pFence = 0;
	if ( !isReady() || ppLists == nullptr || nCount == 0 || m_nNextFence == UINT64_MAX ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	m_pQueue->ExecuteCommandLists( nCount, ppLists );
	m_nLastError = m_pQueue->Signal( m_pFence.Get(), m_nNextFence );
	if ( FAILED( m_nLastError ) ) {
		m_bSubmissionUnsafe = true;
		return false;
	}
	m_nLastSubmittedFence = m_nNextFence;
	*pFence = m_nNextFence;
	++m_nNextFence;
	return true;
}

void SIRD3D12Transfers::markSubmissionUnsafe( void ) noexcept
{
	m_bSubmissionUnsafe = true;
}

bool SIRD3D12Transfers::getCompletedFence( uint64_t *pCompletedFence ) const noexcept
{
	if ( pCompletedFence == nullptr || !isReady() ) {
		return false;
	}
	*pCompletedFence = m_pFence->GetCompletedValue();
	return *pCompletedFence != UINT64_MAX;
}

bool SIRD3D12Transfers::queryDependency( sirD3D12TransferTicket_t ticket, ID3D12Fence **ppFence, uint64_t *pFenceValue ) const noexcept
{
	if ( ppFence == nullptr || pFenceValue == nullptr ) {
		return false;
	}
	*ppFence = nullptr;
	*pFenceValue = 0;

	if ( !isReady() || ticket.nSerial == 0 || ticket.iSlot >= s_nSlotCount ) {
		return false;
	}

	const sirD3D12TransferSlot_t *pSlot = &m_szSlots[ticket.iSlot];
	if ( !pSlot->bOccupied || pSlot->nSerial != ticket.nSerial || pSlot->nFence == 0 || pSlot->nFence > m_nLastSubmittedFence ) {
		return false;
	}

	*ppFence = m_pFence.Get();
	*pFenceValue = pSlot->nFence;
	return true;
}

HRESULT SIRD3D12Transfers::getLastError( void ) const noexcept
{
	return m_nLastError;
}

sirD3D12TransferSlot_t *SIRD3D12Transfers::findSlot( sirD3D12TransferTicket_t ticket ) noexcept
{
	if ( ticket.iSlot >= s_nSlotCount || ticket.nSerial == 0 ) {
		return nullptr;
	}
	sirD3D12TransferSlot_t *pSlot = &m_szSlots[ticket.iSlot];
	if ( !pSlot->bOccupied || pSlot->nSerial != ticket.nSerial ) {
		return nullptr;
	}
	return pSlot;
}

bool SIRD3D12Transfers::submit( const void *pSource, uint64_t nBytes, sirD3D12TransferTicket_t *pTicket ) noexcept
{
	if ( pTicket == nullptr || pSource == nullptr || nBytes == 0 || !isReady() || m_nNextFence == UINT64_MAX ||
	     m_nNextSerial == UINT64_MAX ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	*pTicket = {};

	uint32_t iFree = s_nSlotCount;
	for ( uint32_t iSlot = 0; iSlot < s_nSlotCount; ++iSlot ) {
		if ( !m_szSlots[iSlot].bOccupied ) {
			iFree = iSlot;
			break;
		}
	}
	if ( iFree == s_nSlotCount ) {
		m_nLastError = HRESULT_FROM_WIN32( ERROR_BUSY );
		return false;
	}
	sirD3D12TransferSlot_t *pSlot = &m_szSlots[iFree];
	if ( nBytes > m_Arena.nSlotBytes ) {
		m_nLastError = HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );
		return false;
	}

	const uint64_t nOffset = ( uint64_t )iFree * m_Arena.nSlotBytes;
	// CPU writes into the persistently mapped, preallocated slice. Slot
	// ownership excludes any overwrite until its copy fence completes.
	memcpy( m_Arena.pWriteBytes + nOffset, pSource, ( size_t )nBytes );

	m_nLastError = pSlot->pAllocator->Reset();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = pSlot->pCommands->Reset( pSlot->pAllocator.Get(), nullptr );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	if ( m_Arena.ePath == SIR_D3D12_TRANSFER_STAGED ) {
		pSlot->pCommands->CopyBufferRegion( m_Arena.pGpuResource, nOffset, m_Arena.pWriteResource, nOffset, nBytes );
	} else {
		pSlot->pCommands->CopyBufferRegion( m_Arena.pReadbackResource, nOffset, m_Arena.pWriteResource, nOffset, nBytes );
	}
	m_nLastError = pSlot->pCommands->Close();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	// Staged discrete memory needs a separate submission so the DEFAULT
	// buffer can decay to COMMON before its COPY_SOURCE promotion. UMA and
	// GPU_UPLOAD use the directly GPU-readable mapped source in one list.
	if ( m_Arena.ePath == SIR_D3D12_TRANSFER_STAGED ) {
		m_nLastError = pSlot->pReadbackAllocator->Reset();
		if ( FAILED( m_nLastError ) ) {
			return false;
		}
		m_nLastError = pSlot->pReadbackCommands->Reset( pSlot->pReadbackAllocator.Get(), nullptr );
		if ( FAILED( m_nLastError ) ) {
			return false;
		}
		pSlot->pReadbackCommands->CopyBufferRegion( m_Arena.pReadbackResource, nOffset, m_Arena.pGpuResource, nOffset, nBytes );
		m_nLastError = pSlot->pReadbackCommands->Close();
		if ( FAILED( m_nLastError ) ) {
			return false;
		}
	}

	pSlot->bOccupied = true;
	pSlot->nSerial = m_nNextSerial;
	pSlot->nFence = m_nNextFence;
	pSlot->nBytes = nBytes;
	ID3D12CommandList *pUploadLists[] = { pSlot->pCommands.Get() };
	ID3D12CommandList *pReadbackLists[] = { pSlot->pReadbackCommands.Get() };
	m_pQueue->ExecuteCommandLists( 1, pUploadLists );
	if ( m_Arena.ePath == SIR_D3D12_TRANSFER_STAGED ) {
		m_pQueue->ExecuteCommandLists( 1, pReadbackLists );
	}
	m_nLastError = m_pQueue->Signal( m_pFence.Get(), m_nNextFence );
	if ( FAILED( m_nLastError ) ) {
		m_bSubmissionUnsafe = true;
		return false;
	}
	m_nLastSubmittedFence = m_nNextFence;
	++m_nNextFence;
	++m_nNextSerial;
	pTicket->nSerial = pSlot->nSerial;
	pTicket->iSlot = iFree;
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Transfers::poll( sirD3D12TransferTicket_t ticket, bool *pCompleted ) noexcept
{
	if ( pCompleted == nullptr ) {
		m_nLastError = E_POINTER;
		return false;
	}
	*pCompleted = false;
	sirD3D12TransferSlot_t *pSlot = findSlot( ticket );
	if ( !isReady() || pSlot == nullptr || pSlot->nFence == 0 ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	const uint64_t nCompleted = m_pFence->GetCompletedValue();
	if ( nCompleted == UINT64_MAX ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	*pCompleted = nCompleted >= pSlot->nFence;
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Transfers::waitForFence( uint64_t nFence, uint32_t nTimeoutMs ) noexcept
{
	if ( m_pFence == nullptr || m_hEvent == nullptr ) {
		m_nLastError = E_FAIL;
		return false;
	}
	const uint64_t nCompleted = m_pFence->GetCompletedValue();
	if ( nCompleted == UINT64_MAX ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	if ( nCompleted >= nFence ) {
		m_nLastError = S_OK;
		return true;
	}

	m_nLastError = m_pFence->SetEventOnCompletion( nFence, m_hEvent );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	const DWORD nWait = WaitForSingleObject( m_hEvent, nTimeoutMs );
	if ( nWait != WAIT_OBJECT_0 ) {
		m_nLastError = nWait == WAIT_TIMEOUT ? HRESULT_FROM_WIN32( ERROR_TIMEOUT ) : HRESULT_FROM_WIN32( GetLastError() );
		return false;
	}
	if ( m_pFence->GetCompletedValue() < nFence ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Transfers::wait( sirD3D12TransferTicket_t ticket, uint32_t nTimeoutMs ) noexcept
{
	sirD3D12TransferSlot_t *pSlot = findSlot( ticket );
	if ( !isReady() || pSlot == nullptr || pSlot->nFence == 0 ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	return waitForFence( pSlot->nFence, nTimeoutMs );
}

bool SIRD3D12Transfers::receive( sirD3D12TransferTicket_t ticket, void *pDestination, uint64_t nCapacityBytes ) noexcept
{
	sirD3D12TransferSlot_t *pSlot = findSlot( ticket );
	if ( !isReady() || pSlot == nullptr || pDestination == nullptr || nCapacityBytes < pSlot->nBytes ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	bool bCompleted = false;
	if ( !poll( ticket, &bCompleted ) || !bCompleted ) {
		return false;
	}

	const uint64_t nOffset = ( uint64_t )ticket.iSlot * m_Arena.nSlotBytes;
	memcpy( pDestination, m_Arena.pReadbackBytes + nOffset, ( size_t )pSlot->nBytes );
	pSlot->bOccupied = false;
	pSlot->nFence = 0;
	++m_nCompletedCopies;
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12Transfers::discard( sirD3D12TransferTicket_t ticket ) noexcept
{
	sirD3D12TransferSlot_t *pSlot = findSlot( ticket );
	bool bCompleted = false;
	if ( !isReady() || pSlot == nullptr || !poll( ticket, &bCompleted ) || !bCompleted ) {
		return false;
	}
	pSlot->bOccupied = false;
	pSlot->nFence = 0;
	m_nLastError = S_OK;
	return true;
}
