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

#include <Renderer/Drivers/D3D12/SIRD3D12Commands.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Validation.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <limits.h>

static D3D12_COMMAND_LIST_TYPE sirD3D12CommandType( SIRGraphicsQueue eQueue ) noexcept
{
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ) {
		return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	}
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY ) {
		return D3D12_COMMAND_LIST_TYPE_COPY;
	}
	return D3D12_COMMAND_LIST_TYPE_DIRECT;
}

static D3D12_RESOURCE_STATES sirD3D12BarrierState( SIRGraphicsState eState, SIRGraphicsQueue eQueue ) noexcept
{
	switch ( eState ) {
	case SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED:
		// New default-heap resources are created in COMMON before first use.
		return D3D12_RESOURCE_STATE_COMMON;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COMMON:
		return D3D12_RESOURCE_STATE_COMMON;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_VERTEX_INDEX:
		return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_CONSTANT:
		return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ:
		if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ) {
			return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
		return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ:
		return D3D12_RESOURCE_STATE_DEPTH_READ;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE:
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT:
		return D3D12_RESOURCE_STATE_PRESENT;
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

static D3D12_BARRIER_SYNC sirD3D12EnhancedSync( SIRGraphicsState eState ) noexcept
{
	switch ( eState ) {
	case SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED:
		return D3D12_BARRIER_SYNC_NONE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_VERTEX_INDEX:
		return D3D12_BARRIER_SYNC_DRAW;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_CONSTANT:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_BARRIER_SYNC_ALL_SHADING;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_BARRIER_SYNC_RENDER_TARGET;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE:
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_BARRIER_SYNC_COPY;
	default:
		return D3D12_BARRIER_SYNC_ALL;
	}
}

static D3D12_BARRIER_ACCESS sirD3D12EnhancedAccess( SIRGraphicsState eState ) noexcept
{
	switch ( eState ) {
	case SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED:
		return D3D12_BARRIER_ACCESS_NO_ACCESS;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_VERTEX_INDEX:
		return D3D12_BARRIER_ACCESS_VERTEX_BUFFER | D3D12_BARRIER_ACCESS_INDEX_BUFFER;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_CONSTANT:
		return D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ:
		return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_BARRIER_ACCESS_RENDER_TARGET;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ:
		return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE:
		return D3D12_BARRIER_ACCESS_COPY_SOURCE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_BARRIER_ACCESS_COPY_DEST;
	default:
		return D3D12_BARRIER_ACCESS_COMMON;
	}
}

static D3D12_BARRIER_LAYOUT sirD3D12EnhancedLayout( SIRGraphicsState eState ) noexcept
{
	switch ( eState ) {
	case SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED:
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ:
		return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE:
		return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_BARRIER_LAYOUT_COPY_DEST;
	case SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT:
		return D3D12_BARRIER_LAYOUT_PRESENT;
	default:
		return D3D12_BARRIER_LAYOUT_COMMON;
	}
}

static bool sirD3D12ValidResourceState( SIRGraphicsHandleType eResource, SIRGraphicsState eState ) noexcept
{
	if ( eResource == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return eState != SIRGraphicsState::SIR_GRAPHICS_STATE_COLOR_ATTACHMENT && eState != SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ &&
		       eState != SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE && eState != SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT;
	}
	if ( eResource == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return eState != SIRGraphicsState::SIR_GRAPHICS_STATE_VERTEX_INDEX && eState != SIRGraphicsState::SIR_GRAPHICS_STATE_CONSTANT;
	}
	return false;
}

static bool sirD3D12ValidQueueState( SIRGraphicsQueue eQueue, SIRGraphicsState eState ) noexcept
{
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return true;
	}
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY ) {
		return eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COMMON || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE ||
		       eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION;
	}
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ) {
		return eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COMMON || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ ||
		       eState == SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE || eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_SOURCE ||
		       eState == SIRGraphicsState::SIR_GRAPHICS_STATE_COPY_DESTINATION;
	}
	return false;
}

static D3D_PRIMITIVE_TOPOLOGY sirD3D12PrimitiveTopology( SIRGraphicsTopology eTopology ) noexcept
{
	switch ( eTopology ) {
	case SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_POINTS:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_LINES:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_LINE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	default:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

static uint32_t sirD3D12TexelBytes( SIRGraphicsFormat eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R8_UNORM:
		return 1;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return 2;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_FLOAT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_UINT:
		return 4;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG32_FLOAT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return 8;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return 16;
	default:
		return 0;
	}
}

bool SIRD3D12CommandList::initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, SIRD3D12Pipelines *pPipelines,
                                      SIRD3D12Bindings *pBindings, SIRGraphicsQueue eQueue ) noexcept
{
	if ( m_pNative != nullptr || pNative == nullptr || pResources == nullptr || pPipelines == nullptr || pBindings == nullptr ||
	     eQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ) {
		return false;
	}
	ID3D12Device *pDevice = pNative->getNativeDevice();
	if ( pDevice == nullptr ) {
		return false;
	}
	const D3D12_COMMAND_LIST_TYPE eNativeQueue = sirD3D12CommandType( eQueue );
	if ( FAILED( pDevice->CreateCommandAllocator( eNativeQueue, IID_PPV_ARGS( m_pAllocator.GetAddressOf() ) ) ) ) {
		return false;
	}
	if ( FAILED(
	         pDevice->CreateCommandList( 0, eNativeQueue, m_pAllocator.Get(), nullptr, IID_PPV_ARGS( m_pCommands.GetAddressOf() ) ) ) ) {
		m_pAllocator.Reset();
		return false;
	}
	if ( FAILED( m_pCommands->Close() ) ) {
		m_pCommands.Reset();
		m_pAllocator.Reset();
		return false;
	}
	// Enhanced barriers are optional. All existing command paths retain the native
	// ResourceBarrier fallback; COPY queue execution stays on the legacy contract.
	if ( eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 features {};
		if ( SUCCEEDED( pDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS12, &features, sizeof( features ) ) ) &&
		     features.EnhancedBarriersSupported != FALSE ) {
			static_cast<void>(m_pCommands.As( &m_pEnhancedCommands ));
		}
	}
	m_pNative = pNative;
	m_pResources = pResources;
	m_pPipelines = pPipelines;
	m_pBindings = pBindings;
	m_eQueue = eQueue;
	// The newly created native list is closed, but no caller owns it yet.
	// Keep the public slot free until acquire() gives it to a caller.
	m_bClosed = false;
	return true;
}

void SIRD3D12CommandList::shutdown( bool bGpuComplete ) noexcept
{
	if ( bGpuComplete ) {
		releaseReferences();
		m_pEnhancedCommands.Reset();
		m_pCommands.Reset();
		m_pAllocator.Reset();
	} else {
		// Unknown queue completion: retain native objects and resource owners.
		static_cast<void>(m_pEnhancedCommands.Detach());
		static_cast<void>(m_pCommands.Detach());
		static_cast<void>(m_pAllocator.Detach());
	}
	m_pNative = nullptr;
	m_pResources = nullptr;
	m_pPipelines = nullptr;
	m_pBindings = nullptr;
	m_nReferenceCount = 0;
	m_nSubmittedFence = 0;
	m_bRecording = false;
	m_bClosed = false;
	m_bSubmitted = false;
	m_bRenderPass = false;
	m_bPipelineBound = false;
}

SIRGraphicsQueue SIRD3D12CommandList::getQueue( void ) const noexcept
{
	return m_eQueue;
}

bool SIRD3D12CommandList::validRecording( void ) const noexcept
{
	return m_pNative != nullptr && m_pNative->isReady() && m_bRecording && !m_bSubmitted && m_pCommands != nullptr;
}

ID3D12CommandList *SIRD3D12CommandList::getNativeList( void ) const noexcept
{
	return m_pCommands.Get();
}

void SIRD3D12CommandList::setSubmitted( uint64_t nFence ) noexcept
{
	m_bSubmitted = true;
	m_nSubmittedFence = nFence;
}

SIRGraphicsResult SIRD3D12CommandList::pin( sirGraphicsHandle_t hHandle ) noexcept
{
	for ( uint32_t iRef = 0; iRef < m_nReferenceCount; ++iRef ) {
		const sirGraphicsHandle_t& ref = m_szReferences[iRef];
		if ( ref.eType == hHandle.eType && ref.nSlot == hHandle.nSlot && ref.nGeneration == hHandle.nGeneration &&
		     ref.nDeviceGeneration == hHandle.nDeviceGeneration ) {
			return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
		}
	}
	if ( m_nReferenceCount == s_nReferenceCapacity ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const SIRGraphicsResult eResult = hHandle.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE ? m_pPipelines->retainCommandReference( hHandle )
	                                    : hHandle.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET
	                                        ? m_pBindings->retainCommandReference( hHandle )
	                                        : m_pResources->retainCommandReference( hHandle );
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	m_szReferences[m_nReferenceCount] = hHandle;
	++m_nReferenceCount;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12CommandList::releaseReferences( void ) noexcept
{
	for ( uint32_t iRef = 0; iRef < m_nReferenceCount; ++iRef ) {
		const sirGraphicsHandle_t& ref = m_szReferences[iRef];
		if ( ref.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE ) {
			m_pPipelines->releaseCommandReference( ref );
		} else if ( ref.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET ) {
			m_pBindings->releaseCommandReference( ref );
		} else {
			m_pResources->releaseCommandReference( ref );
		}
	}
	m_nReferenceCount = 0;
}

bool SIRD3D12CommandList::completeSubmission( void ) noexcept
{
	if ( !m_bSubmitted ) {
		return true;
	}
	sirD3D12CompletionSet_t completion {};
	completion.nDeviceGeneration = m_pNative->snapshotCompletionSet( &completion ) ? completion.nDeviceGeneration : 0;
	if ( completion.nDeviceGeneration == 0 ) {
		return false;
	}
	completion.nGraphicsFence = m_eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ? m_nSubmittedFence : 0;
	completion.nComputeFence = m_eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ? m_nSubmittedFence : 0;
	completion.nCopyFence = m_eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY ? m_nSubmittedFence : 0;
	bool bComplete = false;
	if ( !m_pNative->pollCompletionSet( completion, &bComplete ) || !bComplete ) {
		return false;
	}
	releaseReferences();
	m_bSubmitted = false;
	m_bClosed = false;
	m_nSubmittedFence = 0;
	return true;
}

SIRGraphicsResult SIRD3D12CommandList::reset( void ) noexcept
{
	if ( m_pNative == nullptr || !m_pNative->isReady() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !completeSubmission() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_bRecording && FAILED( m_pCommands->Close() ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bRecording = false;
	releaseReferences();
	if ( FAILED( m_pAllocator->Reset() ) || FAILED( m_pCommands->Reset( m_pAllocator.Get(), nullptr ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bRecording = true;
	m_bClosed = false;
	m_bRenderPass = false;
	m_bPipelineBound = false;
	m_nBindingCount = 0;
	m_hBoundPipeline = {};
	memset( m_szBoundRoots, 0, sizeof( m_szBoundRoots ) );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( nCount == 0 || nCount > 64 || pBarriers == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Prevalidate the entire batch before retaining references or encoding GPU work.
	// A later failure must not leave earlier transitions recorded.
	sirGraphicsHandle_t szPending[s_nReferenceCapacity];
	memset( szPending, 0, sizeof( szPending ) );
	const sirD3D12ResourceSlot_t *szSlots[64] = {};
	const sirD3D12ResourceSlot_t *szAliasedSlots[64] = {};
	uint32_t szPlaneCounts[64];
	memset( szPlaneCounts, 0, sizeof( szPlaneCounts ) );
	uint32_t nPendingCount = 0;
	for ( uint32_t iPreflight = 0; iPreflight < nCount; ++iPreflight ) {
		const sirGraphicsBarrier_t& source = pBarriers[iPreflight];
		const sirD3D12ResourceSlot_t *pSlot = m_pResources->querySlot( source.hResource );
		if ( pSlot == nullptr || pSlot->getNativeResource() == nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( source.eType > SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_ALIAS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szSlots[iPreflight] = pSlot;
		if ( m_eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY && source.eType != SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION ) {
			if ( source.eAfter == SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED || source.eBefore > SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT ||
			     source.eAfter > SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT || !sirD3D12ValidResourceState( pSlot->eType, source.eBefore ) ||
			     !sirD3D12ValidResourceState( pSlot->eType, source.eAfter ) ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( ( source.eBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED && !sirD3D12ValidQueueState( m_eQueue, source.eBefore ) ) ||
			     !sirD3D12ValidQueueState( m_eQueue, source.eAfter ) ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
			const uint32_t nMips = pSlot->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ? pSlot->nMips : 1;
			const uint32_t nLayers =
			    pSlot->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE && pSlot->eDimension != SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D ? pSlot->nLayers : 1;
			if ( pSlot->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
				if ( source.nBaseMip != 0 || source.nMipCount != 0 || source.nBaseLayer != 0 || source.nLayerCount != 0 ) {
					return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
				}
			} else if ( !sirGraphicsTextureBarrierRangeValid( source, nMips, nLayers ) ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			// Discard-first-use of owned default-heap resources maps their
			// native COMMON creation state in legacy D3D12; enhanced barriers
			// admit NO_ACCESS/UNDEFINED before their first native use.
			if ( source.eBefore == SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED &&
			     ( pSlot->eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL || pSlot->pExternalResource != nullptr ) ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
		} else if ( source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_UAV ) {
			if ( ( pSlot->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE ) == 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		} else {
			const sirD3D12ResourceSlot_t *pAliased = m_pResources->querySlot( source.hAliasedResource );
			if ( pAliased == nullptr || pAliased->getNativeResource() == nullptr ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
			szAliasedSlots[iPreflight] = pAliased;
		}
		const bool bEnhancedTexture = m_pEnhancedCommands != nullptr && pSlot->eMemory == SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL &&
		                              ( source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION || source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_UAV );
		const bool bPartialLegacyTexture =
		    source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION &&
		    ( source.nBaseMip != 0 || source.nBaseLayer != 0 || ( source.nMipCount != 0 && source.nMipCount != pSlot->nMips ) ||
			  ( source.nLayerCount != 0 && source.nLayerCount != pSlot->nLayers ) );
		if ( pSlot->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE && ( bEnhancedTexture || bPartialLegacyTexture ) ) {
			D3D12_FEATURE_DATA_FORMAT_INFO formatInfo {};
			formatInfo.Format = pSlot->getNativeResource()->GetDesc().Format;
			if ( FAILED(
			         m_pNative->getNativeDevice()->CheckFeatureSupport( D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof( formatInfo ) ) ) ||
			     formatInfo.PlaneCount == 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
			szPlaneCounts[iPreflight] = formatInfo.PlaneCount;
		}
		const uint32_t nBarrierReferences = source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_ALIAS ? 2 : 1;
		for ( uint32_t iHandle = 0; iHandle < nBarrierReferences; ++iHandle ) {
			const sirGraphicsHandle_t hHandle = iHandle == 0 ? source.hResource : source.hAliasedResource;
			bool bKnown = false;
			for ( uint32_t iOld = 0; iOld < m_nReferenceCount && !bKnown; ++iOld ) {
				const sirGraphicsHandle_t& ref = m_szReferences[iOld];
				bKnown = ref.eType == hHandle.eType && ref.nSlot == hHandle.nSlot && ref.nGeneration == hHandle.nGeneration &&
				         ref.nDeviceGeneration == hHandle.nDeviceGeneration;
			}
			for ( uint32_t iPending = 0; iPending < nPendingCount && !bKnown; ++iPending ) {
				const sirGraphicsHandle_t& ref = szPending[iPending];
				bKnown = ref.eType == hHandle.eType && ref.nSlot == hHandle.nSlot && ref.nGeneration == hHandle.nGeneration &&
				         ref.nDeviceGeneration == hHandle.nDeviceGeneration;
			}
			if ( !bKnown ) {
				if ( m_nReferenceCount + nPendingCount >= s_nReferenceCapacity ) {
					return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
				}
				szPending[nPendingCount++] = hHandle;
			}
		}
	}
	const uint32_t nOriginalReferenceCount = m_nReferenceCount;
	for ( uint32_t iPending = 0; iPending < nPendingCount; ++iPending ) {
		const SIRGraphicsResult ePinned = pin( szPending[iPending] );
		if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			// Retention failed before encoding. Roll back only this batch's pins.
			while ( m_nReferenceCount > nOriginalReferenceCount ) {
				const sirGraphicsHandle_t& ref = m_szReferences[--m_nReferenceCount];
				if ( ref.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE ) {
					m_pPipelines->releaseCommandReference( ref );
				} else if ( ref.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET ) {
					m_pBindings->releaseCommandReference( ref );
				} else {
					m_pResources->releaseCommandReference( ref );
				}
			}
			return ePinned;
		}
	}
	// Every native call below is void. The entire batch and its resource lifetimes
	// have been validated and pinned, so encoding cannot return partial success.
	for ( uint32_t iBarrier = 0; iBarrier < nCount; ++iBarrier ) {
		const sirGraphicsBarrier_t& source = pBarriers[iBarrier];
		const sirD3D12ResourceSlot_t *pSlot = szSlots[iBarrier];
		D3D12_RESOURCE_BARRIER native {};
		native.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		if ( source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION ) {
			native.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			native.Transition.pResource = pSlot->getNativeResource();
			native.Transition.StateBefore = sirD3D12BarrierState( source.eBefore, m_eQueue );
			native.Transition.StateAfter = sirD3D12BarrierState( source.eAfter, m_eQueue );
			native.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		} else if ( source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_UAV ) {
			native.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			native.UAV.pResource = pSlot->getNativeResource();
		} else {
			native.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			native.Aliasing.pResourceBefore = pSlot->getNativeResource();
			native.Aliasing.pResourceAfter = szAliasedSlots[iBarrier]->getNativeResource();
		}
		// A resource created in a legacy non-COMMON heap state must continue using
		// the legacy barrier API. Only DEFAULT GPU memory uses this enhanced path.
		if ( m_pEnhancedCommands != nullptr &&
		     ( source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION || source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_UAV ) &&
		     pSlot->eMemory == SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL ) {
			// UAV ordering preserves the current UAV access/layout; transitions
			// use the explicit before/after states supplied by the caller.
			const SIRGraphicsState eBefore = source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_UAV ? SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE : source.eBefore;
			const SIRGraphicsState eAfter = source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_UAV ? SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE : source.eAfter;
			const D3D12_BARRIER_SYNC eSyncBefore = sirD3D12EnhancedSync( eBefore );
			const D3D12_BARRIER_SYNC eSyncAfter = sirD3D12EnhancedSync( eAfter );
			const D3D12_BARRIER_ACCESS eAccessBefore = sirD3D12EnhancedAccess( eBefore );
			const D3D12_BARRIER_ACCESS eAccessAfter = sirD3D12EnhancedAccess( eAfter );
			D3D12_BARRIER_GROUP group {};
			group.NumBarriers = 1;
			if ( pSlot->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
				D3D12_TEXTURE_BARRIER barrier {};
				barrier.SyncBefore = eSyncBefore;
				barrier.SyncAfter = eSyncAfter;
				barrier.AccessBefore = eAccessBefore;
				barrier.AccessAfter = eAccessAfter;
				barrier.LayoutBefore = sirD3D12EnhancedLayout( eBefore );
				barrier.LayoutAfter = sirD3D12EnhancedLayout( eAfter );
				barrier.pResource = pSlot->getNativeResource();
				const bool bUav = source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_UAV;
				barrier.Subresources.IndexOrFirstMipLevel = bUav ? 0 : source.nBaseMip;
				barrier.Subresources.NumMipLevels = bUav                    ? pSlot->nMips
				                                    : source.nMipCount == 0 ? pSlot->nMips - source.nBaseMip
				                                                            : source.nMipCount;
				barrier.Subresources.FirstArraySlice = bUav ? 0 : source.nBaseLayer;
				barrier.Subresources.NumArraySlices =
				    bUav                      ? ( pSlot->eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D ? 1 : pSlot->nLayers )
					: source.nLayerCount == 0 ? ( pSlot->eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D ? 1 : pSlot->nLayers ) - source.nBaseLayer
					                          : source.nLayerCount;
				barrier.Subresources.FirstPlane = 0;
				barrier.Subresources.NumPlanes = szPlaneCounts[iBarrier];
				group.Type = D3D12_BARRIER_TYPE_TEXTURE;
				group.pTextureBarriers = &barrier;
				m_pEnhancedCommands->Barrier( 1, &group );
			} else {
				D3D12_BUFFER_BARRIER barrier {};
				barrier.SyncBefore = eSyncBefore;
				barrier.SyncAfter = eSyncAfter;
				barrier.AccessBefore = eAccessBefore;
				barrier.AccessAfter = eAccessAfter;
				barrier.pResource = pSlot->getNativeResource();
				barrier.Offset = 0;
				barrier.Size = pSlot->nSizeBytes;
				group.Type = D3D12_BARRIER_TYPE_BUFFER;
				group.pBufferBarriers = &barrier;
				m_pEnhancedCommands->Barrier( 1, &group );
			}
			continue;
		}
		if ( source.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION && pSlot->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE &&
		     ( source.nBaseMip != 0 || source.nBaseLayer != 0 || ( source.nMipCount != 0 && source.nMipCount != pSlot->nMips ) ||
		       ( source.nLayerCount != 0 && source.nLayerCount != pSlot->nLayers ) ) ) {
			const uint32_t nMips = pSlot->nMips;
			const uint32_t nLayers = pSlot->eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D ? 1 : pSlot->nLayers;
			const uint32_t nMipCount = source.nMipCount == 0 ? nMips - source.nBaseMip : source.nMipCount;
			const uint32_t nLayerCount = source.nLayerCount == 0 ? nLayers - source.nBaseLayer : source.nLayerCount;
			for ( uint32_t iPlane = 0; iPlane < szPlaneCounts[iBarrier]; ++iPlane ) {
				for ( uint32_t iLayer = source.nBaseLayer; iLayer < source.nBaseLayer + nLayerCount; ++iLayer ) {
					for ( uint32_t iMip = source.nBaseMip; iMip < source.nBaseMip + nMipCount; ++iMip ) {
						native.Transition.Subresource = iMip + iLayer * nMips + iPlane * nMips * nLayers;
						m_pCommands->ResourceBarrier( 1, &native );
					}
				}
			}
		} else {
			m_pCommands->ResourceBarrier( 1, &native );
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource,
                                                     uint64_t nSrcOffset, uint64_t nBytes ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	const sirD3D12ResourceSlot_t *pDst = m_pResources->querySlot( hDestination );
	const sirD3D12ResourceSlot_t *pSrc = m_pResources->querySlot( hSource );
	if ( pDst == nullptr || pSrc == nullptr || pDst->eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER || pSrc->eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( nBytes == 0 || nDstOffset > pDst->nSizeBytes || nSrcOffset > pSrc->nSizeBytes || nBytes > pDst->nSizeBytes - nDstOffset ||
	     nBytes > pSrc->nSizeBytes - nSrcOffset ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pDst->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_DESTINATION ) == 0 || ( pSrc->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_SOURCE ) == 0 ||
	     hDestination.nSlot == hSource.nSlot ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nOriginalReferenceCount = m_nReferenceCount;
	SIRGraphicsResult ePinned = pin( hDestination );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	ePinned = pin( hSource );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		// Retaining the source may fail at the bounded command-reference capacity.
		// Do not keep a newly pinned destination if no copy was recorded.
		while ( m_nReferenceCount > nOriginalReferenceCount ) {
			const sirGraphicsHandle_t& ref = m_szReferences[--m_nReferenceCount];
			m_pResources->releaseCommandReference( ref );
		}
		return ePinned;
	}
	m_pCommands->CopyBufferRegion( pDst->getNativeResource(), nDstOffset, pSrc->getNativeResource(), nSrcOffset, nBytes );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::copyBufferTexture( const sirGraphicsBufferTextureCopy_t& copy ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	const sirD3D12ResourceSlot_t *pBuffer = m_pResources->querySlot( copy.hBuffer );
	const sirD3D12ResourceSlot_t *pTexture = m_pResources->querySlot( copy.hTexture );
	if ( pBuffer == nullptr || pTexture == nullptr || pBuffer->eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ||
	     pTexture->eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pTexture->eDimension != SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_2D || pTexture->nSamples != 1 || copy.nDepth != 1 ||
	     copy.nLayer >= pTexture->nLayers || copy.nMip >= pTexture->nMips ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nTexelBytes = sirD3D12TexelBytes( pTexture->eFormat );
	if ( nTexelBytes == 0 || copy.nWidth == 0 || copy.nHeight == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nMipWidth = pTexture->nWidth >> copy.nMip ? pTexture->nWidth >> copy.nMip : 1;
	const uint32_t nMipHeight = pTexture->nHeight >> copy.nMip ? pTexture->nHeight >> copy.nMip : 1;
	const sirD3D12CopyLimits_t limits = { nMipWidth, nMipHeight, nTexelBytes, pBuffer->nSizeBytes };
	const SIRGraphicsResult eLayout = sirD3D12ValidateCopyRegion( copy, limits );
	if ( eLayout != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eLayout;
	}
	if ( copy.bBufferToTexture ) {
		if ( ( pBuffer->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_SOURCE ) == 0 ||
		     ( pTexture->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_DESTINATION ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( ( pTexture->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_SOURCE ) == 0 ||
	            ( pBuffer->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COPY_DESTINATION ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nOriginalReferenceCount = m_nReferenceCount;
	SIRGraphicsResult ePinned = pin( copy.hBuffer );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	ePinned = pin( copy.hTexture );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		// A failed texture pin leaves no buffer-to-texture copy encoded.
		while ( m_nReferenceCount > nOriginalReferenceCount ) {
			const sirGraphicsHandle_t& ref = m_szReferences[--m_nReferenceCount];
			m_pResources->releaseCommandReference( ref );
		}
		return ePinned;
	}
	D3D12_TEXTURE_COPY_LOCATION placed {};
	placed.pResource = pBuffer->getNativeResource();
	placed.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	placed.PlacedFootprint.Offset = copy.nBufferOffset;
	placed.PlacedFootprint.Footprint.Format = pTexture->getNativeResource()->GetDesc().Format;
	placed.PlacedFootprint.Footprint.Width = copy.nWidth;
	placed.PlacedFootprint.Footprint.Height = copy.nHeight;
	placed.PlacedFootprint.Footprint.Depth = 1;
	placed.PlacedFootprint.Footprint.RowPitch = copy.nBytesPerRow;
	D3D12_TEXTURE_COPY_LOCATION subresource {};
	subresource.pResource = pTexture->getNativeResource();
	subresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	subresource.SubresourceIndex = copy.nMip + copy.nLayer * pTexture->nMips;
	if ( copy.bBufferToTexture ) {
		m_pCommands->CopyTextureRegion( &subresource, copy.nX, copy.nY, 0, &placed, nullptr );
	} else {
		D3D12_BOX box = { copy.nX, copy.nY, 0, copy.nX + copy.nWidth, copy.nY + copy.nHeight, 1 };
		m_pCommands->CopyTextureRegion( &placed, 0, 0, 0, &subresource, &box );
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::beginRenderPass( const sirGraphicsRenderPass_t& pass ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pass.nColorCount > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT || ( pass.nColorCount != 0 && pass.pColors == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const bool bDepth = pass.hDepthView.nGeneration != 0;
	if ( pass.nColorCount == 0 && !bDepth ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pass.bClearDepth && ( !bDepth || !( pass.fClearDepth >= 0.0f && pass.fClearDepth <= 1.0f ) || pass.nClearStencil > UINT8_MAX ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Validate every color and depth descriptor before retaining any view. An
	// invalid later attachment must not leave an earlier view pinned.
	D3D12_CPU_DESCRIPTOR_HANDLE szColors[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	memset( szColors, 0, sizeof( szColors ) );
	D3D12_CPU_DESCRIPTOR_HANDLE depth {};
	for ( uint32_t iColor = 0; iColor < pass.nColorCount; ++iColor ) {
		if ( !m_pResources->queryDescriptor( pass.pColors[iColor].hView, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, &szColors[iColor] ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	if ( bDepth && !m_pResources->queryDescriptor( pass.hDepthView, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, &depth ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	bool bStencil = false;
	if ( bDepth ) {
		const sirD3D12ResourceSlot_t *pDepthView = m_pResources->querySlot( pass.hDepthView );
		const sirD3D12ResourceSlot_t *pDepthResource = pDepthView != nullptr ? m_pResources->querySlot( pDepthView->hParent ) : nullptr;
		if ( pDepthResource == nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		bStencil = pDepthResource->eFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
		if ( pass.bClearDepth && !bStencil && pass.nClearStencil != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	const uint32_t nOriginalReferenceCount = m_nReferenceCount;
	SIRGraphicsResult ePinned = SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	for ( uint32_t iColor = 0; iColor < pass.nColorCount; ++iColor ) {
		ePinned = pin( pass.pColors[iColor].hView );
		if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			break;
		}
	}
	if ( ePinned == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS && bDepth ) {
		ePinned = pin( pass.hDepthView );
	}
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		// All failing retention paths precede native command encoding.
		while ( m_nReferenceCount > nOriginalReferenceCount ) {
			const sirGraphicsHandle_t& ref = m_szReferences[--m_nReferenceCount];
			m_pResources->releaseCommandReference( ref );
		}
		return ePinned;
	}
	m_pCommands->OMSetRenderTargets( pass.nColorCount, pass.nColorCount == 0 ? nullptr : szColors, FALSE, bDepth ? &depth : nullptr );
	for ( uint32_t iColor = 0; iColor < pass.nColorCount; ++iColor ) {
		const sirGraphicsColorAttachment_t& attachment = pass.pColors[iColor];
		if ( attachment.bClear ) {
			m_pCommands->ClearRenderTargetView( szColors[iColor], attachment.szClearColor, 0, nullptr );
		}
	}
	if ( bDepth && pass.bClearDepth ) {
		const D3D12_CLEAR_FLAGS eClearFlags =
		    bStencil ? static_cast<D3D12_CLEAR_FLAGS>( D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL ) : D3D12_CLEAR_FLAG_DEPTH;
		m_pCommands->ClearDepthStencilView( depth, eClearFlags, pass.fClearDepth, static_cast<UINT8>(pass.nClearStencil), 0, nullptr );
	}
	m_bRenderPass = true;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::endRenderPass( void ) noexcept
{
	if ( !validRecording() || !m_bRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	m_bRenderPass = false;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept
{
	if ( !validRecording() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirD3D12PipelineSlot_t *pPipeline = m_pPipelines->querySlot( hPipeline );
	if ( pPipeline == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pPipeline->eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS && m_eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( m_bRenderPass && pPipeline->eType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const SIRGraphicsResult ePinned = pin( hPipeline );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	m_pCommands->SetPipelineState( pPipeline->pPipelineState.Get() );
	if ( pPipeline->eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		m_pCommands->SetGraphicsRootSignature( pPipeline->pRootSignature.Get() );
		m_pCommands->IASetPrimitiveTopology( sirD3D12PrimitiveTopology( pPipeline->eTopology ) );
	} else {
		m_pCommands->SetComputeRootSignature( pPipeline->pRootSignature.Get() );
	}
	m_ePipelineType = pPipeline->eType;
	m_eTopology = pPipeline->eTopology;
	m_nBindingCount = pPipeline->nBindingCount;
	m_hBoundPipeline = hPipeline;
	memset( m_szBoundRoots, 0, sizeof( m_szBoundRoots ) );
	m_bPipelineBound = true;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept
{
	if ( !validRecording() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( !m_bPipelineBound || m_eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirD3D12BindingSetSlot_t *pSet = m_pBindings->querySlot( hBindingSet );
	if ( pSet == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSet->nSetIndex != nSetIndex || pSet->hPipeline.nSlot != m_hBoundPipeline.nSlot ||
	     pSet->hPipeline.nGeneration != m_hBoundPipeline.nGeneration ||
	     pSet->hPipeline.nDeviceGeneration != m_hBoundPipeline.nDeviceGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !m_pBindings->isComplete( *pSet ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	ID3D12DescriptorHeap *szHeaps[2] = { m_pBindings->getHeap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ),
	                                     m_pBindings->getHeap( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) };
	if ( szHeaps[0] == nullptr || szHeaps[1] == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	const SIRGraphicsResult ePinned = pin( hBindingSet );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	m_pCommands->SetDescriptorHeaps( 2, szHeaps );
	for ( uint32_t iRange = 0; iRange < pSet->nRangeCount; ++iRange ) {
		const sirD3D12BindingRange_t& range = pSet->szRanges[iRange];
		if ( m_ePipelineType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
			m_pCommands->SetGraphicsRootDescriptorTable( range.nRootIndex, range.hGpu );
		} else {
			m_pCommands->SetComputeRootDescriptorTable( range.nRootIndex, range.hGpu );
		}
		m_szBoundRoots[range.nRootIndex] = 1;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes,
                                                           uint32_t nStrideBytes ) noexcept
{
	if ( !validRecording() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirD3D12ResourceSlot_t *pBuffer = m_pResources->querySlot( hBuffer );
	if ( pBuffer == nullptr || pBuffer->eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( nSlot >= 16 || nStrideBytes == 0 || nOffsetBytes >= pBuffer->nSizeBytes || pBuffer->nSizeBytes - nOffsetBytes > UINT32_MAX ||
	     ( pBuffer->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_VERTEX ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const SIRGraphicsResult ePinned = pin( hBuffer );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	D3D12_VERTEX_BUFFER_VIEW native {};
	native.BufferLocation = pBuffer->getNativeResource()->GetGPUVirtualAddress() + nOffsetBytes;
	native.SizeInBytes = static_cast<UINT>( pBuffer->nSizeBytes - nOffsetBytes );
	native.StrideInBytes = nStrideBytes;
	m_pCommands->IASetVertexBuffers( nSlot, 1, &native );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, bool bIndex32 ) noexcept
{
	if ( !validRecording() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirD3D12ResourceSlot_t *pBuffer = m_pResources->querySlot( hBuffer );
	if ( pBuffer == nullptr || pBuffer->eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint32_t nIndexBytes = bIndex32 ? 4U : 2U;
	if ( nOffsetBytes >= pBuffer->nSizeBytes || nOffsetBytes % nIndexBytes != 0 || pBuffer->nSizeBytes - nOffsetBytes > UINT32_MAX ||
	     ( pBuffer->nUsage & SIRGraphicsUsage::SIR_GRAPHICS_USAGE_INDEX ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const SIRGraphicsResult ePinned = pin( hBuffer );
	if ( ePinned != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	D3D12_INDEX_BUFFER_VIEW native {};
	native.BufferLocation = pBuffer->getNativeResource()->GetGPUVirtualAddress() + nOffsetBytes;
	native.SizeInBytes = static_cast<UINT>( pBuffer->nSizeBytes - nOffsetBytes );
	native.Format = bIndex32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	m_pCommands->IASetIndexBuffer( &native );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::setViewport( const sirGraphicsViewport_t& viewport, const sirGraphicsRect_t& scissor ) noexcept
{
	if ( !validRecording() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const SIRGraphicsResult eViewport = sirD3D12ValidateViewport( viewport, scissor );
	if ( eViewport != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eViewport;
	}
	D3D12_VIEWPORT native {};
	native.TopLeftX = viewport.fX;
	native.TopLeftY = viewport.fY;
	native.Width = viewport.fWidth;
	native.Height = viewport.fHeight;
	native.MinDepth = viewport.fMinimumDepth;
	native.MaxDepth = viewport.fMaximumDepth;
	D3D12_RECT rect = { scissor.nLeft, scissor.nTop, scissor.nRight, scissor.nBottom };
	m_pCommands->RSSetViewports( 1, &native );
	m_pCommands->RSSetScissorRects( 1, &rect );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex,
                                               uint32_t nFirstInstance ) noexcept
{
	if ( !validRecording() || !m_bRenderPass || !m_bPipelineBound || m_ePipelineType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nVertices == 0 || nInstances == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < m_nBindingCount; ++iBinding ) {
		if ( m_szBoundRoots[iBinding] == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	m_pCommands->DrawInstanced( nVertices, nInstances, nFirstVertex, nFirstInstance );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset,
                                                      uint32_t nFirstInstance ) noexcept
{
	if ( !validRecording() || !m_bRenderPass || !m_bPipelineBound || m_ePipelineType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nIndices == 0 || nInstances == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < m_nBindingCount; ++iBinding ) {
		if ( m_szBoundRoots[iBinding] == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	m_pCommands->DrawIndexedInstanced( nIndices, nInstances, nFirstIndex, nVertexOffset, nFirstInstance );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::dispatch( uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept
{
	if ( !validRecording() || m_bRenderPass || !m_bPipelineBound || m_ePipelineType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nX == 0 || nY == 0 || nZ == 0 || nX > 65535 || nY > 65535 || nZ > 65535 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < m_nBindingCount; ++iBinding ) {
		if ( m_szBoundRoots[iBinding] == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	m_pCommands->Dispatch( nX, nY, nZ );
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12CommandList::close( void ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	if ( FAILED( m_pCommands->Close() ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bRecording = false;
	m_bClosed = true;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRD3D12Commands::~SIRD3D12Commands( void ) noexcept
{
	shutdown( false );
}

bool SIRD3D12Commands::initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, SIRD3D12Pipelines *pPipelines,
                                   SIRD3D12Bindings *pBindings ) noexcept
{
	if ( m_pNative != nullptr || pNative == nullptr || pResources == nullptr || pPipelines == nullptr || pBindings == nullptr ) {
		return false;
	}
	for ( uint32_t iQueue = 0; iQueue < SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			if ( !m_szLists[iQueue][iSlot].initialize( pNative, pResources, pPipelines, pBindings, static_cast<SIRGraphicsQueue>(iQueue) ) ) {
				for ( uint32_t iCleanup = 0; iCleanup < SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT; ++iCleanup ) {
					for ( uint32_t jCleanup = 0; jCleanup < s_nListsPerQueue; ++jCleanup ) {
						m_szLists[iCleanup][jCleanup].shutdown( true );
					}
				}
				return false;
			}
		}
	}
	m_pNative = pNative;
	return true;
}

void SIRD3D12Commands::shutdown( bool bGpuComplete ) noexcept
{
	for ( uint32_t iQueue = 0; iQueue < SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			m_szLists[iQueue][iSlot].shutdown( bGpuComplete );
		}
	}
	m_pNative = nullptr;
}

void SIRD3D12Commands::collectCompleted( void ) noexcept
{
	if ( m_pNative == nullptr ) {
		return;
	}
	for ( uint32_t iQueue = 0; iQueue < SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			static_cast<void>(m_szLists)[iQueue][iSlot].completeSubmission();
		}
	}
}

SIRGraphicsResult SIRD3D12Commands::acquire( SIRGraphicsQueue eQueue, SIRGraphicsCommandList **ppCommands ) noexcept
{
	if ( ppCommands == nullptr || eQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppCommands = nullptr;
	if ( m_pNative == nullptr || !m_pNative->isReady() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	collectCompleted();
	for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
		SIRD3D12CommandList& commands = m_szLists[eQueue][iSlot];
		// A closed, unsubmitted list still belongs to its recording caller.
		// Only a completed submission (or explicit reset) releases that claim.
		if ( commands.m_bRecording || commands.m_bClosed || commands.m_bSubmitted ) {
			continue;
		}
		const SIRGraphicsResult eReset = commands.reset();
		if ( eReset != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eReset;
		}
		*ppCommands = &commands;
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

SIRGraphicsResult SIRD3D12Commands::submit( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission,
                                              sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	if ( m_pNative == nullptr || !m_pNative->isReady() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D12CompletionSet_t snapshot {};
	if ( !m_pNative->snapshotCompletionSet( &snapshot ) || snapshot.nDeviceGeneration > UINT32_MAX ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	const SIRGraphicsResult eValid = sirGraphicsValidateSubmission( eQueue, submission, static_cast<uint32_t>(snapshot.nDeviceGeneration) );
	if ( eValid != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	// Image token ownership is validated by SIRD3D12Surfaces before submission.
	if ( submission.nCommandListCount > s_nListsPerQueue ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	ID3D12CommandList *szLists[s_nListsPerQueue] = {};
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		SIRD3D12CommandList *pMatched = nullptr;
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			if ( submission.ppCommandLists[iList] == &m_szLists[eQueue][iSlot] ) {
				pMatched = &m_szLists[eQueue][iSlot];
				break;
			}
		}
		if ( pMatched == nullptr || !pMatched->m_bClosed || pMatched->m_bRecording || pMatched->m_bSubmitted ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iList; ++iPrevious ) {
			if ( submission.ppCommandLists[iPrevious] == submission.ppCommandLists[iList] ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		// External swapchain resources may be referenced only in a submission
		// carrying the matching one-use image acquisition.
		for ( uint32_t iRef = 0; iRef < pMatched->m_nReferenceCount; ++iRef ) {
			const sirGraphicsHandle_t hRef = pMatched->m_szReferences[iRef];
			const sirD3D12ResourceSlot_t *pResource = pMatched->m_pResources->querySlot( hRef );
			sirGraphicsHandle_t hTexture = hRef;
			if ( pResource != nullptr && pResource->eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW ) {
				hTexture = pResource->hParent;
				pResource = pMatched->m_pResources->querySlot( hTexture );
			}
			if ( pResource == nullptr || pResource->pExternalResource == nullptr ) {
				continue;
			}
			bool bNamed = false;
			for ( uint32_t iAcquire = 0; iAcquire < submission.nAcquisitionCount; ++iAcquire ) {
				const sirGraphicsHandle_t& hImage = submission.pAcquisitions[iAcquire].hImage;
				if ( hImage.nSlot == hTexture.nSlot && hImage.nGeneration == hTexture.nGeneration &&
				     hImage.nDeviceGeneration == hTexture.nDeviceGeneration ) {
					bNamed = true;
				}
			}
			if ( !bNamed ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		szLists[iList] = pMatched->getNativeList();
	}
	const SIRGraphicsResult eResult =
	    m_pNative->submitRecorded( eQueue, szLists, submission.nCommandListCount, submission.pWaits, submission.nWaitCount, pCompletion );
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			if ( submission.ppCommandLists[iList] == &m_szLists[eQueue][iSlot] ) {
				m_szLists[eQueue][iSlot].setSubmitted( pCompletion->nValue );
			}
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
