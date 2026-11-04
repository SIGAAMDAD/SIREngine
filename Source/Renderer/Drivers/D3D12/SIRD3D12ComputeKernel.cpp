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

#include <Renderer/Drivers/D3D12/SIRD3D12ComputeKernel.h>

#include <d3dcompiler.h>
#include <string.h>

SIRD3D12ComputeKernel::SIRD3D12ComputeKernel( void ) noexcept
{
}

SIRD3D12ComputeKernel::~SIRD3D12ComputeKernel( void ) noexcept
{
	shutdown( false );
}

bool SIRD3D12ComputeKernel::initialize( ID3D12Device *pDevice, const void *pDxil, size_t nDxilBytes ) noexcept
{
	if ( pDevice == nullptr || pDxil == nullptr || nDxilBytes < 16 || nDxilBytes > 4 * 1024 * 1024 ||
	     m_pPipelineState != nullptr ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {};
	shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_0;
	m_nLastError = pDevice->CheckFeatureSupport( D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof( shaderModel ) );
	if ( FAILED( m_nLastError ) || shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0 ) {
		m_nLastError = E_NOTIMPL;
		return false;
	}

	D3D12_DESCRIPTOR_RANGE range = {};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	range.NumDescriptors = 1;
	range.BaseShaderRegister = 0;
	range.RegisterSpace = 0;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER roots[2] = {};
	roots[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	roots[0].DescriptorTable.NumDescriptorRanges = 1;
	roots[0].DescriptorTable.pDescriptorRanges = &range;
	roots[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	roots[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	roots[1].Constants.ShaderRegister = 0;
	roots[1].Constants.RegisterSpace = 0;
	roots[1].Constants.Num32BitValues = 2;
	roots[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.NumParameters = 2;
	rootDesc.pParameters = roots;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> pRootBinary;
	Microsoft::WRL::ComPtr<ID3DBlob> pErrors;
	m_nLastError = D3D12SerializeRootSignature( &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, pRootBinary.GetAddressOf(),
	                                            pErrors.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	m_nLastError = pDevice->CreateRootSignature( 0, pRootBinary->GetBufferPointer(), pRootBinary->GetBufferSize(),
	                                            IID_PPV_ARGS( m_pRootSignature.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline = {};
	pipeline.pRootSignature = m_pRootSignature.Get();
	pipeline.CS.pShaderBytecode = pDxil;
	pipeline.CS.BytecodeLength = nDxilBytes;
	m_nLastError = pDevice->CreateComputePipelineState( &pipeline, IID_PPV_ARGS( m_pPipelineState.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		m_pRootSignature.Reset();
		return false;
	}

	m_nLastError = S_OK;
	return true;
}

bool SIRD3D12ComputeKernel::isReady( void ) const noexcept
{
	return m_pRootSignature != nullptr && m_pPipelineState != nullptr;
}

bool SIRD3D12ComputeKernel::hasPending( void ) const noexcept
{
	return m_pOutput != nullptr || m_pReadback != nullptr;
}

void SIRD3D12ComputeKernel::clearPending( void ) noexcept
{
	if ( m_pReadback != nullptr ) {
		m_pReadback->Release();
		m_pReadback = nullptr;
	}
	if ( m_pOutput != nullptr ) {
		m_pOutput->Release();
		m_pOutput = nullptr;
	}
	m_pUavHeap.Reset();
	m_nSubmissionFence = 0;
	m_nElementCount = 0;
}

bool SIRD3D12ComputeKernel::record( ID3D12Device *pDevice, SIRD3D12Memory *pMemory,
                                    ID3D12GraphicsCommandList *pCommands, uint32_t nElements, uint32_t nSeed ) noexcept
{
	constexpr uint32_t MAX_DISPATCH_ELEMENTS = 4096;
	if ( !isReady() || hasPending() || pDevice == nullptr || pMemory == nullptr || pCommands == nullptr ||
	     nElements == 0 || nElements > MAX_DISPATCH_ELEMENTS ) {
		m_nLastError = E_INVALIDARG;
		return false;
	}

	const uint64_t nBytes = ( uint64_t )nElements * sizeof( uint32_t );
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pOutput;
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> pReadback;
	m_nLastError = pMemory->createBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nBytes,
	                                      pOutput.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}
	m_nLastError = pMemory->createBuffer( D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, nBytes,
	                                      pReadback.GetAddressOf() );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heap = {};
	heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heap.NumDescriptors = 1;
	heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	m_nLastError = pDevice->CreateDescriptorHeap( &heap, IID_PPV_ARGS( m_pUavHeap.GetAddressOf() ) );
	if ( FAILED( m_nLastError ) ) {
		return false;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
	uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uav.Format = DXGI_FORMAT_UNKNOWN;
	uav.Buffer.FirstElement = 0;
	uav.Buffer.NumElements = nElements;
	uav.Buffer.StructureByteStride = sizeof( uint32_t );
	uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	pDevice->CreateUnorderedAccessView( pOutput->GetResource(), nullptr, &uav,
	                                    m_pUavHeap->GetCPUDescriptorHandleForHeapStart() );

	ID3D12DescriptorHeap *pHeaps[] = { m_pUavHeap.Get() };
	pCommands->SetDescriptorHeaps( 1, pHeaps );
	pCommands->SetComputeRootSignature( m_pRootSignature.Get() );
	pCommands->SetPipelineState( m_pPipelineState.Get() );
	pCommands->SetComputeRootDescriptorTable( 0, m_pUavHeap->GetGPUDescriptorHandleForHeapStart() );
	const uint32_t szConstants[2] = { nElements, nSeed };
	pCommands->SetComputeRoot32BitConstants( 1, 2, szConstants, 0 );
	pCommands->Dispatch( ( nElements + 63 ) / 64, 1, 1 );

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = pOutput->GetResource();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	pCommands->ResourceBarrier( 1, &barrier );
	pCommands->CopyBufferRegion( pReadback->GetResource(), 0, pOutput->GetResource(), 0, nBytes );

	m_pOutput = pOutput.Detach();
	m_pReadback = pReadback.Detach();
	m_nElementCount = nElements;
	m_nLastError = S_OK;
	return true;
}

void SIRD3D12ComputeKernel::submitted( uint64_t nFence ) noexcept
{
	m_nSubmissionFence = nFence;
}

bool SIRD3D12ComputeKernel::readback( ID3D12Fence *pFence, uint64_t nFence, uint32_t *pOutput,
                                     uint32_t nCapacity ) noexcept
{
	if ( pFence == nullptr || pOutput == nullptr || !hasPending() || m_nSubmissionFence == 0 ||
	     nFence != m_nSubmissionFence || nCapacity < m_nElementCount ) {
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

	D3D12_RANGE read = { 0, ( SIZE_T )( ( uint64_t )m_nElementCount * sizeof( uint32_t ) ) };
	D3D12_RANGE noWrite = { 0, 0 };
	void *pBytes = nullptr;
	m_nLastError = m_pReadback->GetResource()->Map( 0, &read, &pBytes );
	if ( FAILED( m_nLastError ) || pBytes == nullptr ) {
		return false;
	}
	memcpy( pOutput, pBytes, read.End );
	m_pReadback->GetResource()->Unmap( 0, &noWrite );
	clearPending();
	m_nLastError = S_OK;
	return true;
}

void SIRD3D12ComputeKernel::cancelUnsubmitted( void ) noexcept
{
	if ( m_nSubmissionFence == 0 ) {
		clearPending();
	}
}

void SIRD3D12ComputeKernel::shutdown( bool bGpuCompleted ) noexcept
{
	if ( bGpuCompleted ) {
		clearPending();
		m_pPipelineState.Reset();
		m_pRootSignature.Reset();
	} else {
		// Preserve every native reference if queue submission/completion is
		// uncertain; the owning D3D12MA allocator must remain alive too.
		m_pOutput = nullptr;
		m_pReadback = nullptr;
		( void )m_pUavHeap.Detach();
		( void )m_pPipelineState.Detach();
		( void )m_pRootSignature.Detach();
	}
	m_nSubmissionFence = 0;
	m_nElementCount = 0;
}

HRESULT SIRD3D12ComputeKernel::getLastError( void ) const noexcept
{
	return m_nLastError;
}
