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

#include <Renderer/Drivers/D3D11/SIRD3D11Device.h>
#include <Renderer/Drivers/D3D11/SIRD3D11BufferValidation.h>

#include <d3d11.h>
#include <dxgi.h>

static SIRGraphicsResult sirD3D11BufferError( HRESULT hResult ) noexcept
{
	if ( hResult == E_OUTOFMEMORY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	if ( hResult == DXGI_ERROR_DEVICE_REMOVED || hResult == DXGI_ERROR_DEVICE_RESET || hResult == DXGI_ERROR_DEVICE_HUNG ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
}

SIRGraphicsResult SIRD3D11Device::createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_pNativeDevice == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D11BufferPlan_t plan {};
	const SIRGraphicsResult ePlan = sirD3D11PlanBuffer( &desc, m_handles.getAllocatedBytes(), m_nResidentBudgetBytes, &plan );
	if ( ePlan != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return ePlan;
	}
	if ( m_handles.getActiveCount() >= SIRD3D11Handles::CAPACITY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	D3D11_BUFFER_DESC nativeDesc {};
	nativeDesc.ByteWidth = plan.nSizeBytes;
	nativeDesc.Usage = plan.eStorage == SIRD3D11BufferStorage::SIR_D3D11_BUFFER_DYNAMIC   ? D3D11_USAGE_DYNAMIC
	                   : plan.eStorage == SIRD3D11BufferStorage::SIR_D3D11_BUFFER_STAGING ? D3D11_USAGE_STAGING
	                                                               : D3D11_USAGE_DEFAULT;
	nativeDesc.CPUAccessFlags = plan.nMappedAccess == 1 ? D3D11_CPU_ACCESS_WRITE : plan.nMappedAccess == 2 ? D3D11_CPU_ACCESS_READ : 0U;
	if ( ( plan.nNativeBindUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_VERTEX ) != 0 ) {
		nativeDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
	}
	if ( ( plan.nNativeBindUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_INDEX ) != 0 ) {
		nativeDesc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
	}
	if ( ( plan.nNativeBindUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_CONSTANT ) != 0 ) {
		nativeDesc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
	}

	ID3D11Buffer *pNativeBuffer = nullptr;
	const HRESULT hCreate = static_cast<ID3D11Device *>(m_pNativeDevice)->CreateBuffer( &nativeDesc, nullptr, &pNativeBuffer );
	if ( FAILED( hCreate ) || pNativeBuffer == nullptr ) {
		if ( pNativeBuffer != nullptr ) {
			pNativeBuffer->Release();
		}
		return FAILED( hCreate ) ? sirD3D11BufferError( hCreate ) : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	const sirD3D11ObjectInfo_t object = { pNativeBuffer, desc.nSizeBytes, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, desc.eMemory };
	const SIRGraphicsResult eRegister = m_handles.create( object, pHandle );
	if ( eRegister != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		pNativeBuffer->Release();
		return eRegister;
	}
	if ( m_handles.getAllocatedBytes() > m_nPeakCommittedBytes ) {
		m_nPeakCommittedBytes = m_handles.getAllocatedBytes();
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D11Device::mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes,
                                               sirGraphicsMappedBuffer_t *pMapped ) noexcept
{
	if ( pMapped == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pMapped = {};
	if ( m_pNativeDevice == nullptr || m_pImmediateContext == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	const SIRGraphicsResult eLookup = m_handles.lookup( hBuffer, &pSlot );
	if ( eLookup != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	if ( hBuffer.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( nBytes == 0 || nOffsetBytes > pSlot->object.nSizeBytes || nBytes > pSlot->object.nSizeBytes - nOffsetBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSlot->nMapped != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( pSlot->object.eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_UPLOAD && pSlot->object.eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_READBACK ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	// WRITE_DISCARD invalidates the entire dynamic allocation; until an
	// in-flight-safe NO_OVERWRITE ring exists, only full-buffer leases are legal.
	if ( pSlot->object.eMemory == SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_UPLOAD && ( nOffsetBytes != 0 || nBytes != pSlot->object.nSizeBytes ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	ID3D11DeviceContext *pContext = static_cast<ID3D11DeviceContext *>(m_pImmediateContext);
	D3D11_MAPPED_SUBRESOURCE nativeMap {};
	const D3D11_MAP eAccess = pSlot->object.eMemory == SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_UPLOAD ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_READ;
	const HRESULT hMap = pContext->Map( static_cast<ID3D11Buffer *>(pSlot->object.pNativeObject), 0, eAccess, 0, &nativeMap );
	if ( FAILED( hMap ) || nativeMap.pData == nullptr ) {
		return FAILED( hMap ) ? sirD3D11BufferError( hMap ) : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	pSlot->nMapped = 1;
	pSlot->nMappedOffsetBytes = nOffsetBytes;
	pSlot->nMappedBytes = nBytes;
	*pMapped = { static_cast<uint8_t *>(nativeMap.pData) + nOffsetBytes, nOffsetBytes, nBytes };
	if ( pSlot->object.eMemory == SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_UPLOAD ) {
		m_nUploadMappedBytes += nBytes;
	} else {
		m_nReadbackMappedBytes += nBytes;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D11Device::unmapBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept
{
	if ( m_pImmediateContext == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	const SIRGraphicsResult eLookup = m_handles.lookup( hBuffer, &pSlot );
	if ( eLookup != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	if ( hBuffer.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->nMapped == 0 || nOffsetBytes != pSlot->nMappedOffsetBytes || nBytes != pSlot->nMappedBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	static_cast<ID3D11DeviceContext *>(m_pImmediateContext)->Unmap( static_cast<ID3D11Buffer *>(pSlot->object.pNativeObject), 0 );
	if ( pSlot->object.eMemory == SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_UPLOAD ) {
		m_nUploadMappedBytes -= nBytes;
	} else {
		m_nReadbackMappedBytes -= nBytes;
	}
	pSlot->nMapped = 0;
	pSlot->nMappedOffsetBytes = 0;
	pSlot->nMappedBytes = 0;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D11Device::flushBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept
{
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	const SIRGraphicsResult eLookup = m_handles.lookup( hBuffer, &pSlot );
	if ( eLookup != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	if ( hBuffer.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->object.eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_UPLOAD ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pSlot->nMapped == 0 || nBytes == 0 || nOffsetBytes < pSlot->nMappedOffsetBytes ||
	     nOffsetBytes > pSlot->nMappedOffsetBytes + pSlot->nMappedBytes ||
	     nBytes > pSlot->nMappedOffsetBytes + pSlot->nMappedBytes - nOffsetBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// D3D11 dynamic-map writes become visible when Unmap publishes the range.
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D11Device::invalidateBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, uint64_t nBytes ) noexcept
{
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	const SIRGraphicsResult eLookup = m_handles.lookup( hBuffer, &pSlot );
	if ( eLookup != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	if ( hBuffer.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->object.eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_CPU_READBACK ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pSlot->nMapped == 0 || nBytes == 0 || nOffsetBytes < pSlot->nMappedOffsetBytes ||
	     nOffsetBytes > pSlot->nMappedOffsetBytes + pSlot->nMappedBytes ||
	     nBytes > pSlot->nMappedOffsetBytes + pSlot->nMappedBytes - nOffsetBytes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D11Device::retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept
{
	if ( m_pNativeDevice == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( completion.nDeviceGeneration != 0 && completion.nDeviceGeneration != m_nDeviceGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	// This bootstrap does not expose a submission/completion token yet. A
	// nonempty dependency must never be treated as already completed.
	if ( completion.nQueueMask != 0 || completion.szQueueValues[0] != 0 || completion.szQueueValues[1] != 0 ||
	     completion.szQueueValues[2] != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	sirD3D11ObjectSlot_t *pSlot = nullptr;
	const SIRGraphicsResult eLookup = m_handles.lookup( hResource, &pSlot );
	if ( eLookup != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eLookup;
	}
	if ( hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER && hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE &&
	     hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW && hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SAMPLER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	void *pNative = nullptr;
	const SIRGraphicsResult eRelease = m_handles.destroy( hResource, &pNative );
	if ( eRelease != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eRelease;
	}
	static_cast<IUnknown *>(pNative)->Release();
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
