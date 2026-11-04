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

#include <Renderer/Drivers/D3D12/SIRD3D12GraphicsKernel.h>

#include <d3dcompiler.h>
#include <string.h>

typedef struct sirD3D12TestVertex_s
{
	float fPosition[3];
	float fColor[4];
} sirD3D12TestVertex_t;

SIRD3D12GraphicsKernel::SIRD3D12GraphicsKernel( void ) noexcept
{
}

SIRD3D12GraphicsKernel::~SIRD3D12GraphicsKernel( void ) noexcept
{
	shutdown( false );
}

bool SIRD3D12GraphicsKernel::initialize( ID3D12Device *pDevice, SIRD3D12Memory *pMemory,
                                         const void *pVertexDxil, size_t nVertexBytes, const void *pPixelDxil,
                                         size_t nPixelBytes, uint32_t nMaxWidth, uint32_t nMaxHeight ) noexcept
{
	if ( pDevice == nullptr || pMemory == nullptr || !pMemory->isReady() || pVertexDxil == nullptr || pPixelDxil == nullptr ||
	     nVertexBytes < 16 || nPixelBytes < 16 || nVertexBytes > 4 * 1024 * 1024 ||
	     nPixelBytes > 4 * 1024 * 1024 || nMaxWidth == 0 || nMaxHeight == 0 ||
	     nMaxWidth > 4096 || nMaxHeight > 4096 || m_pPipelineState != nullptr ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	Microsoft::WRL::ComPtr<ID3DBlob> pRootBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pErrors;
	m_nLastError = D3D12SerializeRootSignature( &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, pRootBlob.GetAddressOf(), pErrors.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = pDevice->CreateRootSignature( 0, pRootBlob->GetBufferPointer(), pRootBlob->GetBufferSize(),
	                                             IID_PPV_ARGS( m_pRootSignature.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	const D3D12_INPUT_ELEMENT_DESC szLayout[] = {
	    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } };

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline = {};
	pipeline.pRootSignature = m_pRootSignature.Get();
	pipeline.VS = { pVertexDxil, nVertexBytes };
	pipeline.PS = { pPixelDxil, nPixelBytes };
	pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pipeline.SampleMask = UINT_MAX;
	pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipeline.RasterizerState.DepthClipEnable = TRUE;
	pipeline.DepthStencilState.DepthEnable = FALSE;
	pipeline.DepthStencilState.StencilEnable = FALSE;
	pipeline.InputLayout = { szLayout, 2 };
	pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipeline.NumRenderTargets = 1;
	pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipeline.SampleDesc.Count = 1;

	m_nLastError = pDevice->CreateGraphicsPipelineState( &pipeline, IID_PPV_ARGS( m_pPipelineState.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		m_pRootSignature.Reset();
		return false;
	}
	D3D12_CLEAR_VALUE clear = {};
	clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clear.Color[3] = 1.0f;
	m_nLastError = pMemory->createTexture2D( nMaxWidth, nMaxHeight, DXGI_FORMAT_R8G8B8A8_UNORM,
	                                         D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET,
	                                         &clear, &m_pTarget );
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}

	const D3D12_RESOURCE_DESC targetDesc = m_pTarget->GetResource()->GetDesc();
	UINT64 nReadbackBytes = 0;
	pDevice->GetCopyableFootprints( &targetDesc, 0, 1, 0, &m_Footprint, nullptr, nullptr, &nReadbackBytes );
	if ( nReadbackBytes == 0 || nReadbackBytes > SIZE_MAX ) {
		m_nLastError = E_FAIL;
		shutdown( true );
		return false;
	}
	m_nLastError = pMemory->createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, nReadbackBytes,
	                                      &m_pReadback );
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}

	const sirD3D12TestVertex_t szVertices[3] = {
	    { { -0.80f, -0.80f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	    { { 0.0f, 0.80f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	    { { 0.80f, -0.80f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }
	};
	const uint16_t szIndices[3] = { 0, 1, 2 };
	m_nLastError = pMemory->createBuffer( D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ,
	                                      sizeof( szVertices ), &m_pVertices );
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}
	m_nLastError = pMemory->createBuffer( D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ,
	                                      sizeof( szIndices ), &m_pIndices );
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}

	D3D12_RANGE noRead = { 0, 0 };
	m_nLastError = m_pVertices->GetResource()->Map( 0, &noRead, &m_pVertexMapped );
	if ( FAILED( m_nLastError ) || m_pVertexMapped == nullptr ) {
		shutdown( true );
		return false;
	}
	memcpy( m_pVertexMapped, szVertices, sizeof( szVertices ) );
	m_nLastError = m_pIndices->GetResource()->Map( 0, &noRead, &m_pIndexMapped );
	if ( FAILED( m_nLastError ) || m_pIndexMapped == nullptr ) {
		shutdown( true );
		return false;
	}
	memcpy( m_pIndexMapped, szIndices, sizeof( szIndices ) );
	void *pMappedReadback = nullptr;
	m_nLastError = m_pReadback->GetResource()->Map( 0, nullptr, &pMappedReadback );
	if ( FAILED( m_nLastError ) || pMappedReadback == nullptr ) {
		shutdown( true );
		return false;
	}
	m_pReadbackMapped = ( const uint8_t * )pMappedReadback;

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeap = {};
	rtvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeap.NumDescriptors = 1;
	m_nLastError = pDevice->CreateDescriptorHeap( &rtvHeap, IID_PPV_ARGS( m_pRtvHeap.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}
	pDevice->CreateRenderTargetView( m_pTarget->GetResource(), nullptr,
	                                 m_pRtvHeap->GetCPUDescriptorHandleForHeapStart() );

	m_nLastError = pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT,
	                                                IID_PPV_ARGS( m_pCommandAllocator.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}
	m_nLastError = pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(),
	                                          m_pPipelineState.Get(), IID_PPV_ARGS( m_pCommandList.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}
	m_nLastError = m_pCommandList->Close();
	if ( FAILED( m_nLastError ) ) {
		shutdown( true );
		return false;
	}

	m_nCapacityWidth = nMaxWidth;
	m_nCapacityHeight = nMaxHeight;
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12GraphicsKernel::isReady( void ) const noexcept
{
	return m_pRootSignature != nullptr && m_pPipelineState != nullptr && m_pTarget != nullptr &&
	       m_pReadback != nullptr && m_pVertices != nullptr && m_pIndices != nullptr &&
	       m_pReadbackMapped != nullptr && m_pCommandList != nullptr && m_pRtvHeap != nullptr;
}

bool SIRD3D12GraphicsKernel::hasPending( void ) const noexcept
{
	return m_bPending;
}

bool SIRD3D12GraphicsKernel::queryCapacity( uint32_t *pWidth, uint32_t *pHeight ) const noexcept
{
	if ( pWidth == nullptr || pHeight == nullptr || !isReady() ) {
		return false;
	}
	*pWidth = m_nCapacityWidth;
	*pHeight = m_nCapacityHeight;
	return true;
}

void SIRD3D12GraphicsKernel::releasePending( void ) noexcept
{
	m_bPending = false;
	m_nSubmittedFence = 0;
	m_nWidth = 0;
	m_nHeight = 0;
}

bool SIRD3D12GraphicsKernel::record( uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( !isReady() || hasPending() || nWidth == 0 || nHeight == 0 ||
	     nWidth > m_nCapacityWidth || nHeight > m_nCapacityHeight ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	m_nLastError = m_pCommandAllocator->Reset();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = m_pCommandList->Reset( m_pCommandAllocator.Get(), m_pPipelineState.Get() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, ( float )nWidth, ( float )nHeight, 0.0f, 1.0f };
	const D3D12_RECT scissor = { 0, 0, ( LONG )nWidth, ( LONG )nHeight };
	const float szClear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const D3D12_CPU_DESCRIPTOR_HANDLE hRtv = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
	m_pCommandList->RSSetViewports( 1, &viewport );
	m_pCommandList->RSSetScissorRects( 1, &scissor );
	m_pCommandList->SetGraphicsRootSignature( m_pRootSignature.Get() );
	m_pCommandList->OMSetRenderTargets( 1, &hRtv, FALSE, nullptr );
	m_pCommandList->ClearRenderTargetView( hRtv, szClear, 0, nullptr );

	const D3D12_VERTEX_BUFFER_VIEW vbv = { m_pVertices->GetResource()->GetGPUVirtualAddress(),
	                                       ( UINT )( sizeof( sirD3D12TestVertex_t ) * 3 ),
	                                       ( UINT )sizeof( sirD3D12TestVertex_t ) };
	const D3D12_INDEX_BUFFER_VIEW ibv = { m_pIndices->GetResource()->GetGPUVirtualAddress(),
	                                      ( UINT )( sizeof( uint16_t ) * 3 ), DXGI_FORMAT_R16_UINT };
	m_pCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	m_pCommandList->IASetVertexBuffers( 0, 1, &vbv );
	m_pCommandList->IASetIndexBuffer( &ibv );
	m_pCommandList->DrawIndexedInstanced( 3, 1, 0, 0, 0 );

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = m_pTarget->GetResource();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	m_pCommandList->ResourceBarrier( 1, &barrier );

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = m_pReadback->GetResource();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint = m_Footprint;
	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = m_pTarget->GetResource();
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;
	const D3D12_BOX sourceBox = { 0, 0, 0, nWidth, nHeight, 1 };
	m_pCommandList->CopyTextureRegion( &dst, 0, 0, 0, &src, &sourceBox );

	// The same resident RTV must begin the next draw in RENDER_TARGET.
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_pCommandList->ResourceBarrier( 1, &barrier );
	m_nLastError = m_pCommandList->Close();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	m_bPending = true;
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nLastError = S_OK;
	return true;
}

ID3D12CommandList *SIRD3D12GraphicsKernel::getRecordedList( void ) const noexcept
{
	return m_pCommandList.Get();
}

void SIRD3D12GraphicsKernel::submitted( uint64_t nFence ) noexcept
{
	m_nSubmittedFence = nFence;
}

bool SIRD3D12GraphicsKernel::readback( ID3D12Fence *pFence, uint64_t nFence, uint8_t *pOutput, uint32_t nRowPitch,
                                       size_t nOutputBytes ) noexcept
{
	if ( pFence == nullptr || pOutput == nullptr || !hasPending() || nFence == 0 || nFence != m_nSubmittedFence ||
	     nRowPitch < m_nWidth * 4 || nOutputBytes < ( uint64_t )nRowPitch * ( m_nHeight - 1 ) + ( uint64_t )m_nWidth * 4 ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}
	const uint64_t nCompleted = pFence->GetCompletedValue();
	if ( nCompleted == UINT64_MAX ) {
		m_nLastError = DXGI_ERROR_DEVICE_REMOVED;
		return false;
	}
	if ( nCompleted < nFence ) {
		m_nLastError = HRESULT_FROM_WIN32( ERROR_BUSY );
		return false;
	}

	const uint8_t *pSource = m_pReadbackMapped + m_Footprint.Offset;
	for ( uint32_t iRow = 0; iRow < m_nHeight; ++iRow ) {
		memcpy( pOutput + ( size_t )iRow * nRowPitch, pSource + ( size_t )iRow * m_Footprint.Footprint.RowPitch, ( size_t )m_nWidth * 4 );
	}
	releasePending();
	m_nLastError = S_OK;
	return true;
}

void SIRD3D12GraphicsKernel::cancelUnsubmitted( void ) noexcept
{
	if ( m_nSubmittedFence == 0 ) {
		releasePending();
	}
}

void SIRD3D12GraphicsKernel::shutdown( bool bGpuCompleted ) noexcept
{
	if ( bGpuCompleted ) {
		D3D12_RANGE noWrite = { 0, 0 };
		if ( m_pReadbackMapped != nullptr && m_pReadback != nullptr ) {
			m_pReadback->GetResource()->Unmap( 0, &noWrite );
		}
		if ( m_pVertexMapped != nullptr && m_pVertices != nullptr ) {
			m_pVertices->GetResource()->Unmap( 0, nullptr );
		}
		if ( m_pIndexMapped != nullptr && m_pIndices != nullptr ) {
			m_pIndices->GetResource()->Unmap( 0, nullptr );
		}
		if ( m_pReadback != nullptr ) {
			m_pReadback->Release();
			m_pReadback = nullptr;
		}
		if ( m_pTarget != nullptr ) {
			m_pTarget->Release();
			m_pTarget = nullptr;
		}
		if ( m_pVertices != nullptr ) {
			m_pVertices->Release();
			m_pVertices = nullptr;
		}
		if ( m_pIndices != nullptr ) {
			m_pIndices->Release();
			m_pIndices = nullptr;
		}
		m_pRtvHeap.Reset();
		m_pCommandList.Reset();
		m_pCommandAllocator.Reset();
		m_pPipelineState.Reset();
		m_pRootSignature.Reset();
	} else {
		// Preserve native allocations and their persistent mappings when
		// the graphics queue's completion cannot be proven.
		m_pTarget = nullptr;
		m_pReadback = nullptr;
		m_pVertices = nullptr;
		m_pIndices = nullptr;
		( void )m_pRtvHeap.Detach();
		( void )m_pCommandList.Detach();
		( void )m_pCommandAllocator.Detach();
		( void )m_pPipelineState.Detach();
		( void )m_pRootSignature.Detach();
	}
	m_pReadbackMapped = nullptr;
	m_pVertexMapped = nullptr;
	m_pIndexMapped = nullptr;
	m_Footprint = {};
	m_nCapacityWidth = 0;
	m_nCapacityHeight = 0;
	releasePending();
}

HRESULT SIRD3D12GraphicsKernel::getLastError( void ) const noexcept
{
	return m_nLastError;
}
