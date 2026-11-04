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

#include <Renderer/Drivers/D3D12/SIRD3D12Memory.h>

SIRD3D12Memory::SIRD3D12Memory( void ) noexcept
{
}

SIRD3D12Memory::~SIRD3D12Memory( void ) noexcept
{
	shutdown();
}

bool SIRD3D12Memory::initialize( IDXGIAdapter *pAdapter, ID3D12Device *pDevice ) noexcept
{
	if ( m_pAllocator != nullptr || pAdapter == nullptr || pDevice == nullptr ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	D3D12MA::ALLOCATOR_DESC desc = {};
	desc.pAdapter = pAdapter;
	desc.pDevice = pDevice;
	m_nLastError = D3D12MA::CreateAllocator( &desc, &m_pAllocator );
	return SUCCEEDED( m_nLastError ) && m_pAllocator != nullptr;
}

void SIRD3D12Memory::shutdown( void ) noexcept
{
	if ( m_pAllocator != nullptr ) {
		m_pAllocator->Release();
		m_pAllocator = nullptr;
	}
}

void SIRD3D12Memory::abandon( void ) noexcept
{
	m_pAllocator = nullptr;
}

bool SIRD3D12Memory::isReady( void ) const noexcept
{
	return m_pAllocator != nullptr;
}

HRESULT SIRD3D12Memory::createBuffer( D3D12_HEAP_TYPE eHeapType, D3D12_RESOURCE_STATES eInitialState, uint64_t nBytes,
                                      D3D12MA::Allocation **ppAllocation, D3D12_RESOURCE_FLAGS eResourceFlags ) noexcept
{
	if ( ppAllocation == nullptr ) {
		m_nLastError = E_POINTER;
		return m_nLastError;
	}
	*ppAllocation = nullptr;
	if ( m_pAllocator == nullptr || nBytes == 0 || nBytes > SIZE_MAX ) {
		m_nLastError = E_INVALIDARG;
		return m_nLastError;
	}

	D3D12_RESOURCE_DESC resource = {};
	resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource.Width = nBytes;
	resource.Height = 1;
	resource.DepthOrArraySize = 1;
	resource.MipLevels = 1;
	resource.Format = DXGI_FORMAT_UNKNOWN;
	resource.SampleDesc.Count = 1;
	resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource.Flags = eResourceFlags;
	D3D12MA::ALLOCATION_DESC allocation = {};
	allocation.HeapType = eHeapType;

	m_nLastError = m_pAllocator->CreateResource( &allocation, &resource, eInitialState, nullptr, ppAllocation, IID_NULL, nullptr );
	return m_nLastError;
}

HRESULT SIRD3D12Memory::createTexture2D( uint32_t nWidth, uint32_t nHeight, DXGI_FORMAT eFormat, D3D12_RESOURCE_FLAGS eFlags,
                                         D3D12_RESOURCE_STATES eInitialState, const D3D12_CLEAR_VALUE *pClearValue,
                                         D3D12MA::Allocation **ppAllocation ) noexcept
{
	if ( ppAllocation == nullptr ) {
		m_nLastError = E_POINTER;
		return m_nLastError;
	}
	*ppAllocation = nullptr;
	if ( m_pAllocator == nullptr || nWidth == 0 || nHeight == 0 || eFormat == DXGI_FORMAT_UNKNOWN ) {
		m_nLastError = E_INVALIDARG;
		return m_nLastError;
	}

	D3D12_RESOURCE_DESC resource = {};
	resource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource.Width = nWidth;
	resource.Height = nHeight;
	resource.DepthOrArraySize = 1;
	resource.MipLevels = 1;
	resource.Format = eFormat;
	resource.SampleDesc.Count = 1;
	resource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource.Flags = eFlags;

	D3D12MA::ALLOCATION_DESC allocation = {};
	allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	m_nLastError = m_pAllocator->CreateResource( &allocation, &resource, eInitialState, pClearValue, ppAllocation, IID_NULL, nullptr );
	return m_nLastError;
}

HRESULT SIRD3D12Memory::getLastError( void ) const noexcept
{
	return m_nLastError;
}
