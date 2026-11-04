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

bool SIRD3D12GraphicsKernel::initialize( ID3D12Device *pDevice, const void *pVertexDxil, size_t nVertexBytes, const void *pPixelDxil,
                                         size_t nPixelBytes ) noexcept
{
	if ( pDevice == nullptr || pVertexDxil == nullptr || pPixelDxil == nullptr || nVertexBytes < 16 || nPixelBytes < 16 ||
	     nVertexBytes > 4 * 1024 * 1024 || nPixelBytes > 4 * 1024 * 1024 || m_pPipelineState != nullptr ) {
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
	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12GraphicsKernel::isReady( void ) const noexcept
{
	return m_pRootSignature != nullptr && m_pPipelineState != nullptr;
}

bool SIRD3D12GraphicsKernel::hasPending( void ) const noexcept
{
	return m_pTarget != nullptr || m_pReadback != nullptr || m_pVertices != nullptr || m_pIndices != nullptr;
}

void SIRD3D12GraphicsKernel::releasePending( void ) noexcept
{
	if ( m_pTarget != nullptr ) {
		m_pTarget->Release();
		m_pTarget = nullptr;
	}
	if ( m_pReadback != nullptr ) {
		m_pReadback->Release();
		m_pReadback = nullptr;
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
	m_Footprint = {};
	m_nSubmittedFence = 0;
	m_nWidth = 0;
	m_nHeight = 0;
}

bool SIRD3D12GraphicsKernel::record( ID3D12Device *pDevice, SIRD3D12Memory *pMemory, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	constexpr uint32_t MAX_DRAW_DIMENSION = 128;
	if ( !isReady() || hasPending() || pDevice == nullptr || pMemory == nullptr || nWidth == 0 || nHeight == 0 ||
	     nWidth > MAX_DRAW_DIMENSION || nHeight > MAX_DRAW_DIMENSION ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pTarget;
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pReadback;
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pVertices;
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pIndices;

	D3D12_CLEAR_VALUE clear = {};
	clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clear.Color[3] = 1.0f;
	m_nLastError = pMemory->createTexture2D( nWidth, nHeight, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
	                                         D3D12_RESOURCE_STATE_RENDER_TARGET, &clear, pTarget.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	const D3D12_RESOURCE_DESC targetDesc = pTarget->GetResource()->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT64 nReadbackBytes = 0;
	pDevice->GetCopyableFootprints( &targetDesc, 0, 1, 0, &footprint, nullptr, nullptr, &nReadbackBytes );
	if ( nReadbackBytes == 0 || nReadbackBytes > SIZE_MAX ) {
		m_nLastError = E_FAIL;
		return false;
	}
	m_nLastError =
	    pMemory->createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, nReadbackBytes, pReadback.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	const sirD3D12TestVertex_t szVertices[3] = { { { -0.80f, -0.80f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	                                             { { 0.0f, 0.80f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	                                             { { 0.80f, -0.80f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } } };
	const uint16_t szIndices[3] = { 0, 1, 2 };

	m_nLastError =
	    pMemory->createBuffer( D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, sizeof( szVertices ), pVertices.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError =
	    pMemory->createBuffer( D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, sizeof( szIndices ), pIndices.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	D3D12_RANGE noRead = { 0, 0 };
	void *pMapped = nullptr;
	m_nLastError = pVertices->GetResource()->Map( 0, &noRead, &pMapped );
	if ( FAILED( m_nLastError ) || pMapped == nullptr ) {
		return false;
	}
	memcpy( pMapped, szVertices, sizeof( szVertices ) );
	pVertices->GetResource()->Unmap( 0, nullptr );

	pMapped = nullptr;
	m_nLastError = pIndices->GetResource()->Map( 0, &noRead, &pMapped );
	if ( FAILED( m_nLastError ) || pMapped == nullptr ) {
		return false;
	}
	memcpy( pMapped, szIndices, sizeof( szIndices ) );
	pIndices->GetResource()->Unmap( 0, nullptr );

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pRtvHeap;
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeap = {};
	rtvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeap.NumDescriptors = 1;
	m_nLastError = pDevice->CreateDescriptorHeap( &rtvHeap, IID_PPV_ARGS( pRtvHeap.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE hRtv = pRtvHeap->GetCPUDescriptorHandleForHeapStart();
	pDevice->CreateRenderTargetView( pTarget->GetResource(), nullptr, hRtv );

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommands;
	m_nLastError = pDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( pAllocator.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = pDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator.Get(), m_pPipelineState.Get(),
	                                           IID_PPV_ARGS( pCommands.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, ( float )nWidth, ( float )nHeight, 0.0f, 1.0f };
	const D3D12_RECT scissor = { 0, 0, ( LONG )nWidth, ( LONG )nHeight };
	const float szClear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	pCommands->RSSetViewports( 1, &viewport );
	pCommands->RSSetScissorRects( 1, &scissor );
	pCommands->SetGraphicsRootSignature( m_pRootSignature.Get() );
	pCommands->OMSetRenderTargets( 1, &hRtv, FALSE, nullptr );
	pCommands->ClearRenderTargetView( hRtv, szClear, 0, nullptr );

	const D3D12_VERTEX_BUFFER_VIEW vbv = { pVertices->GetResource()->GetGPUVirtualAddress(), ( UINT )sizeof( szVertices ),
	                                       ( UINT )sizeof( sirD3D12TestVertex_t ) };
	const D3D12_INDEX_BUFFER_VIEW ibv = { pIndices->GetResource()->GetGPUVirtualAddress(), ( UINT )sizeof( szIndices ),
	                                      DXGI_FORMAT_R16_UINT };
	pCommands->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	pCommands->IASetVertexBuffers( 0, 1, &vbv );
	pCommands->IASetIndexBuffer( &ibv );
	pCommands->DrawIndexedInstanced( 3, 1, 0, 0, 0 );

	D3D12_RESOURCE_BARRIER transition = {};
	transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transition.Transition.pResource = pTarget->GetResource();
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	pCommands->ResourceBarrier( 1, &transition );

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = pReadback->GetResource();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint = footprint;

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = pTarget->GetResource();
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;
	pCommands->CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );

	m_nLastError = pCommands->Close();
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	m_pTarget = pTarget.Detach();
	m_pReadback = pReadback.Detach();
	m_pVertices = pVertices.Detach();
	m_pIndices = pIndices.Detach();
	m_pRtvHeap = pRtvHeap;
	m_pCommandAllocator = pAllocator;
	m_pCommandList = pCommands;
	m_Footprint = footprint;
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

	const SIZE_T nEnd =
	    ( SIZE_T )( m_Footprint.Offset + ( uint64_t )m_Footprint.Footprint.RowPitch * ( m_nHeight - 1 ) + ( uint64_t )m_nWidth * 4 );
	D3D12_RANGE read = { 0, nEnd };
	D3D12_RANGE noWrite = { 0, 0 };
	void *pMapped = nullptr;
	m_nLastError = m_pReadback->GetResource()->Map( 0, &read, &pMapped );
	if ( FAILED( m_nLastError ) || pMapped == nullptr ) {
		return false;
	}

	const uint8_t *pSource = ( const uint8_t * )pMapped + m_Footprint.Offset;
	for ( uint32_t iRow = 0; iRow < m_nHeight; ++iRow ) {
		memcpy( pOutput + ( size_t )iRow * nRowPitch, pSource + ( size_t )iRow * m_Footprint.Footprint.RowPitch, ( size_t )m_nWidth * 4 );
	}
	m_pReadback->GetResource()->Unmap( 0, &noWrite );
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
		releasePending();
		m_pPipelineState.Reset();
		m_pRootSignature.Reset();
	} else {
		// On uncertain GPU completion, preserve every native reference
		// and the owning D3D12MA allocator until device teardown.
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
	m_nSubmittedFence = 0;
	m_nWidth = 0;
	m_nHeight = 0;
}

HRESULT SIRD3D12GraphicsKernel::getLastError( void ) const noexcept
{
	return m_nLastError;
}
