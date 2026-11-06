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
#include <Renderer/SIRGraphicsValidation.h>
#include <limits.h>

static D3D12_COMMAND_LIST_TYPE sirD3D12CommandType( sirGraphicsQueue_t eQueue ) noexcept
{
	if ( eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ) {
		return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	}
	if ( eQueue == SIR_GRAPHICS_QUEUE_COPY ) {
		return D3D12_COMMAND_LIST_TYPE_COPY;
	}
	return D3D12_COMMAND_LIST_TYPE_DIRECT;
}

static D3D12_RESOURCE_STATES sirD3D12BarrierState( sirGraphicsState_t eState, sirGraphicsQueue_t eQueue ) noexcept
{
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_UNDEFINED:
		// New default-heap resources are created in COMMON before first use.
		return D3D12_RESOURCE_STATE_COMMON;
	case SIR_GRAPHICS_STATE_COMMON:
		return D3D12_RESOURCE_STATE_COMMON;
	case SIR_GRAPHICS_STATE_VERTEX_INDEX:
		return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
	case SIR_GRAPHICS_STATE_CONSTANT:
		return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	case SIR_GRAPHICS_STATE_SHADER_READ:
		if ( eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ) {
			return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
		return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case SIR_GRAPHICS_STATE_DEPTH_READ:
		return D3D12_RESOURCE_STATE_DEPTH_READ;
	case SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case SIR_GRAPHICS_STATE_COPY_SOURCE:
		return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case SIR_GRAPHICS_STATE_PRESENT:
		return D3D12_RESOURCE_STATE_PRESENT;
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

static D3D12_BARRIER_SYNC sirD3D12EnhancedSync( sirGraphicsState_t eState ) noexcept
{
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_UNDEFINED:
		return D3D12_BARRIER_SYNC_NONE;
	case SIR_GRAPHICS_STATE_VERTEX_INDEX:
		return D3D12_BARRIER_SYNC_DRAW;
	case SIR_GRAPHICS_STATE_CONSTANT:
	case SIR_GRAPHICS_STATE_SHADER_READ:
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_BARRIER_SYNC_ALL_SHADING;
	case SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_BARRIER_SYNC_RENDER_TARGET;
	case SIR_GRAPHICS_STATE_DEPTH_READ:
	case SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
	case SIR_GRAPHICS_STATE_COPY_SOURCE:
	case SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_BARRIER_SYNC_COPY;
	default:
		return D3D12_BARRIER_SYNC_ALL;
	}
}

static D3D12_BARRIER_ACCESS sirD3D12EnhancedAccess( sirGraphicsState_t eState ) noexcept
{
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_UNDEFINED:
		return D3D12_BARRIER_ACCESS_NO_ACCESS;
	case SIR_GRAPHICS_STATE_VERTEX_INDEX:
		return D3D12_BARRIER_ACCESS_VERTEX_BUFFER | D3D12_BARRIER_ACCESS_INDEX_BUFFER;
	case SIR_GRAPHICS_STATE_CONSTANT:
		return D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
	case SIR_GRAPHICS_STATE_SHADER_READ:
		return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	case SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_BARRIER_ACCESS_RENDER_TARGET;
	case SIR_GRAPHICS_STATE_DEPTH_READ:
		return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
	case SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
	case SIR_GRAPHICS_STATE_COPY_SOURCE:
		return D3D12_BARRIER_ACCESS_COPY_SOURCE;
	case SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_BARRIER_ACCESS_COPY_DEST;
	default:
		return D3D12_BARRIER_ACCESS_COMMON;
	}
}

static D3D12_BARRIER_LAYOUT sirD3D12EnhancedLayout( sirGraphicsState_t eState ) noexcept
{
	switch ( eState ) {
	case SIR_GRAPHICS_STATE_UNDEFINED:
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	case SIR_GRAPHICS_STATE_SHADER_READ:
		return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
	case SIR_GRAPHICS_STATE_SHADER_WRITE:
		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
	case SIR_GRAPHICS_STATE_COLOR_ATTACHMENT:
		return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	case SIR_GRAPHICS_STATE_DEPTH_READ:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
	case SIR_GRAPHICS_STATE_DEPTH_WRITE:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
	case SIR_GRAPHICS_STATE_COPY_SOURCE:
		return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
	case SIR_GRAPHICS_STATE_COPY_DESTINATION:
		return D3D12_BARRIER_LAYOUT_COPY_DEST;
	case SIR_GRAPHICS_STATE_PRESENT:
		return D3D12_BARRIER_LAYOUT_PRESENT;
	default:
		return D3D12_BARRIER_LAYOUT_COMMON;
	}
}

static bool sirD3D12ValidResourceState( sirGraphicsHandleType_t eResource, sirGraphicsState_t eState ) noexcept
{
	if ( eResource == SIR_GRAPHICS_HANDLE_BUFFER ) {
		return eState != SIR_GRAPHICS_STATE_COLOR_ATTACHMENT && eState != SIR_GRAPHICS_STATE_DEPTH_READ &&
		       eState != SIR_GRAPHICS_STATE_DEPTH_WRITE && eState != SIR_GRAPHICS_STATE_PRESENT;
	}
	if ( eResource == SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return eState != SIR_GRAPHICS_STATE_VERTEX_INDEX && eState != SIR_GRAPHICS_STATE_CONSTANT;
	}
	return false;
}

static bool sirD3D12ValidQueueState( sirGraphicsQueue_t eQueue, sirGraphicsState_t eState ) noexcept
{
	if ( eQueue == SIR_GRAPHICS_QUEUE_DIRECT ) {
		return true;
	}
	if ( eQueue == SIR_GRAPHICS_QUEUE_COPY ) {
		return eState == SIR_GRAPHICS_STATE_COMMON || eState == SIR_GRAPHICS_STATE_COPY_SOURCE ||
		       eState == SIR_GRAPHICS_STATE_COPY_DESTINATION;
	}
	if ( eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ) {
		return eState == SIR_GRAPHICS_STATE_COMMON || eState == SIR_GRAPHICS_STATE_SHADER_READ ||
		       eState == SIR_GRAPHICS_STATE_SHADER_WRITE || eState == SIR_GRAPHICS_STATE_COPY_SOURCE ||
		       eState == SIR_GRAPHICS_STATE_COPY_DESTINATION;
	}
	return false;
}

static D3D_PRIMITIVE_TOPOLOGY sirD3D12PrimitiveTopology( sirGraphicsTopology_t eTopology ) noexcept
{
	switch ( eTopology ) {
	case SIR_GRAPHICS_TOPOLOGY_POINTS:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case SIR_GRAPHICS_TOPOLOGY_LINES:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case SIR_GRAPHICS_TOPOLOGY_LINE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	default:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

static uint32_t sirD3D12TexelBytes( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		return 1;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return 2;
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
	case SIR_GRAPHICS_FORMAT_R32_UINT:
		return 4;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return 8;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return 16;
	default:
		return 0;
	}
}

bool SIRD3D12CommandList::initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, SIRD3D12Pipelines *pPipelines,
                                      SIRD3D12Bindings *pBindings, sirGraphicsQueue_t eQueue ) noexcept
{
	if ( m_pNative != nullptr || pNative == nullptr || pResources == nullptr || pPipelines == nullptr || pBindings == nullptr ||
	     eQueue >= SIR_GRAPHICS_QUEUE_COUNT ) {
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
	if ( eQueue == SIR_GRAPHICS_QUEUE_DIRECT ) {
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 features = {};
		if ( SUCCEEDED( pDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS12, &features, sizeof( features ) ) ) &&
		     features.EnhancedBarriersSupported != FALSE ) {
			( void )m_pCommands.As( &m_pEnhancedCommands );
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
		( void )m_pEnhancedCommands.Detach();
		( void )m_pCommands.Detach();
		( void )m_pAllocator.Detach();
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

sirGraphicsQueue_t SIRD3D12CommandList::getQueue( void ) const noexcept
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

sirGraphicsResult_t SIRD3D12CommandList::pin( sirGraphicsHandle_t hHandle ) noexcept
{
	for ( uint32_t iRef = 0; iRef < m_nReferenceCount; ++iRef ) {
		const sirGraphicsHandle_t& ref = m_szReferences[iRef];
		if ( ref.eType == hHandle.eType && ref.nSlot == hHandle.nSlot && ref.nGeneration == hHandle.nGeneration &&
		     ref.nDeviceGeneration == hHandle.nDeviceGeneration ) {
			return SIR_GRAPHICS_SUCCESS;
		}
	}
	if ( m_nReferenceCount == s_nReferenceCapacity ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const sirGraphicsResult_t eResult = hHandle.eType == SIR_GRAPHICS_HANDLE_PIPELINE ? m_pPipelines->retainCommandReference( hHandle )
	                                    : hHandle.eType == SIR_GRAPHICS_HANDLE_BINDING_SET
	                                        ? m_pBindings->retainCommandReference( hHandle )
	                                        : m_pResources->retainCommandReference( hHandle );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	m_szReferences[m_nReferenceCount] = hHandle;
	++m_nReferenceCount;
	return SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12CommandList::releaseReferences( void ) noexcept
{
	for ( uint32_t iRef = 0; iRef < m_nReferenceCount; ++iRef ) {
		const sirGraphicsHandle_t& ref = m_szReferences[iRef];
		if ( ref.eType == SIR_GRAPHICS_HANDLE_PIPELINE ) {
			m_pPipelines->releaseCommandReference( ref );
		} else if ( ref.eType == SIR_GRAPHICS_HANDLE_BINDING_SET ) {
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
	sirD3D12CompletionSet_t completion = {};
	completion.nDeviceGeneration = m_pNative->snapshotCompletionSet( &completion ) ? completion.nDeviceGeneration : 0;
	if ( completion.nDeviceGeneration == 0 ) {
		return false;
	}
	completion.nGraphicsFence = m_eQueue == SIR_GRAPHICS_QUEUE_DIRECT ? m_nSubmittedFence : 0;
	completion.nComputeFence = m_eQueue == SIR_GRAPHICS_QUEUE_COMPUTE ? m_nSubmittedFence : 0;
	completion.nCopyFence = m_eQueue == SIR_GRAPHICS_QUEUE_COPY ? m_nSubmittedFence : 0;
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

sirGraphicsResult_t SIRD3D12CommandList::reset( void ) noexcept
{
	if ( m_pNative == nullptr || !m_pNative->isReady() ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( !completeSubmission() ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_bRecording && FAILED( m_pCommands->Close() ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bRecording = false;
	releaseReferences();
	if ( FAILED( m_pAllocator->Reset() ) || FAILED( m_pCommands->Reset( m_pAllocator.Get(), nullptr ) ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bRecording = true;
	m_bClosed = false;
	m_bRenderPass = false;
	m_bPipelineBound = false;
	m_nBindingCount = 0;
	m_hBoundPipeline = {};
	memset( m_szBoundRoots, 0, sizeof( m_szBoundRoots ) );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( nCount == 0 || nCount > 64 || pBarriers == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// Prevalidate the entire batch before retaining references or encoding GPU work.
	// A later failure must not leave earlier transitions recorded.
	sirGraphicsHandle_t szPending[s_nReferenceCapacity] = {};
	uint32_t nPendingCount = 0;
	for ( uint32_t iPreflight = 0; iPreflight < nCount; ++iPreflight ) {
		const sirGraphicsBarrier_t& source = pBarriers[iPreflight];
		const sirD3D12ResourceSlot_t *pSlot = m_pResources->querySlot( source.hResource );
		if ( pSlot == nullptr || pSlot->getNativeResource() == nullptr ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( source.eType > SIR_GRAPHICS_BARRIER_ALIAS ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( m_eQueue == SIR_GRAPHICS_QUEUE_COPY && source.eType != SIR_GRAPHICS_BARRIER_TRANSITION ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( source.eType == SIR_GRAPHICS_BARRIER_TRANSITION ) {
			if ( source.eAfter == SIR_GRAPHICS_STATE_UNDEFINED ||
			     source.eBefore > SIR_GRAPHICS_STATE_PRESENT || source.eAfter > SIR_GRAPHICS_STATE_PRESENT ||
			     !sirD3D12ValidResourceState( pSlot->eType, source.eBefore ) ||
			     !sirD3D12ValidResourceState( pSlot->eType, source.eAfter ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( ( source.eBefore != SIR_GRAPHICS_STATE_UNDEFINED && !sirD3D12ValidQueueState( m_eQueue, source.eBefore ) ) ||
			     !sirD3D12ValidQueueState( m_eQueue, source.eAfter ) ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			const uint32_t nMips = pSlot->eType == SIR_GRAPHICS_HANDLE_TEXTURE ? pSlot->nMips : 1;
			const uint32_t nLayers =
			    pSlot->eType == SIR_GRAPHICS_HANDLE_TEXTURE && pSlot->eDimension != SIR_GRAPHICS_TEXTURE_3D ? pSlot->nLayers : 1;
			if ( pSlot->eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
				if ( source.nBaseMip != 0 || source.nMipCount != 0 || source.nBaseLayer != 0 || source.nLayerCount != 0 ) {
					return SIR_GRAPHICS_INVALID_ARGUMENT;
				}
			} else if ( !sirGraphicsTextureBarrierRangeValid( source, nMips, nLayers ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			// Discard-first-use of owned default-heap resources maps their
			// native COMMON creation state in legacy D3D12; enhanced barriers
			// admit NO_ACCESS/UNDEFINED before their first native use.
			if ( source.eBefore == SIR_GRAPHICS_STATE_UNDEFINED &&
			     ( pSlot->eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL || pSlot->pExternalResource != nullptr ) ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
		} else if ( source.eType == SIR_GRAPHICS_BARRIER_UAV ) {
			if ( ( pSlot->nUsage & SIR_GRAPHICS_USAGE_STORAGE ) == 0 ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		} else {
			const sirD3D12ResourceSlot_t *pAliased = m_pResources->querySlot( source.hAliasedResource );
			if ( pAliased == nullptr || pAliased->getNativeResource() == nullptr ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
		}
		const bool bEnhancedTexture = m_pEnhancedCommands != nullptr && pSlot->eMemory == SIR_GRAPHICS_MEMORY_GPU_LOCAL &&
		                              ( source.eType == SIR_GRAPHICS_BARRIER_TRANSITION || source.eType == SIR_GRAPHICS_BARRIER_UAV );
		const bool bPartialLegacyTexture =
		    source.eType == SIR_GRAPHICS_BARRIER_TRANSITION &&
		    ( source.nBaseMip != 0 || source.nBaseLayer != 0 || ( source.nMipCount != 0 && source.nMipCount != pSlot->nMips ) ||
			  ( source.nLayerCount != 0 && source.nLayerCount != pSlot->nLayers ) );
		if ( pSlot->eType == SIR_GRAPHICS_HANDLE_TEXTURE && ( bEnhancedTexture || bPartialLegacyTexture ) ) {
			D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
			formatInfo.Format = pSlot->getNativeResource()->GetDesc().Format;
			if ( FAILED(
			         m_pNative->getNativeDevice()->CheckFeatureSupport( D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof( formatInfo ) ) ) ||
			     formatInfo.PlaneCount == 0 ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
		}
		const uint32_t nBarrierReferences = source.eType == SIR_GRAPHICS_BARRIER_ALIAS ? 2 : 1;
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
					return SIR_GRAPHICS_CAPACITY_EXCEEDED;
				}
				szPending[nPendingCount++] = hHandle;
			}
		}
	}
	const uint32_t nOriginalReferenceCount = m_nReferenceCount;
	for ( uint32_t iPending = 0; iPending < nPendingCount; ++iPending ) {
		const sirGraphicsResult_t ePinned = pin( szPending[iPending] );
		if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
			// Retention failed before encoding. Roll back only this batch's pins.
			while ( m_nReferenceCount > nOriginalReferenceCount ) {
				const sirGraphicsHandle_t& ref = m_szReferences[--m_nReferenceCount];
				if ( ref.eType == SIR_GRAPHICS_HANDLE_PIPELINE ) {
					m_pPipelines->releaseCommandReference( ref );
				} else if ( ref.eType == SIR_GRAPHICS_HANDLE_BINDING_SET ) {
					m_pBindings->releaseCommandReference( ref );
				} else {
					m_pResources->releaseCommandReference( ref );
				}
			}
			return ePinned;
		}
	}
	for ( uint32_t iBarrier = 0; iBarrier < nCount; ++iBarrier ) {
		const sirGraphicsBarrier_t& source = pBarriers[iBarrier];
		const sirD3D12ResourceSlot_t *pSlot = m_pResources->querySlot( source.hResource );
		if ( pSlot == nullptr || pSlot->getNativeResource() == nullptr ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( source.eType > SIR_GRAPHICS_BARRIER_ALIAS ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( m_eQueue == SIR_GRAPHICS_QUEUE_COPY && source.eType != SIR_GRAPHICS_BARRIER_TRANSITION ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		D3D12_RESOURCE_BARRIER native = {};
		native.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		if ( source.eType == SIR_GRAPHICS_BARRIER_TRANSITION ) {
			if ( source.eAfter == SIR_GRAPHICS_STATE_UNDEFINED ||
			     source.eBefore > SIR_GRAPHICS_STATE_PRESENT || source.eAfter > SIR_GRAPHICS_STATE_PRESENT ||
			     !sirD3D12ValidResourceState( pSlot->eType, source.eBefore ) ||
			     !sirD3D12ValidResourceState( pSlot->eType, source.eAfter ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( ( source.eBefore != SIR_GRAPHICS_STATE_UNDEFINED && !sirD3D12ValidQueueState( m_eQueue, source.eBefore ) ) ||
			     !sirD3D12ValidQueueState( m_eQueue, source.eAfter ) ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			const uint32_t nMips = pSlot->eType == SIR_GRAPHICS_HANDLE_TEXTURE ? pSlot->nMips : 1;
			const uint32_t nLayers =
			    pSlot->eType == SIR_GRAPHICS_HANDLE_TEXTURE && pSlot->eDimension != SIR_GRAPHICS_TEXTURE_3D ? pSlot->nLayers : 1;
			if ( pSlot->eType == SIR_GRAPHICS_HANDLE_BUFFER ) {
				if ( source.nBaseMip != 0 || source.nMipCount != 0 || source.nBaseLayer != 0 || source.nLayerCount != 0 ) {
					return SIR_GRAPHICS_INVALID_ARGUMENT;
				}
			} else if ( !sirGraphicsTextureBarrierRangeValid( source, nMips, nLayers ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			// Discard-first-use of owned default-heap resources maps their
			// native COMMON creation state in legacy D3D12; enhanced barriers
			// admit NO_ACCESS/UNDEFINED before their first native use.
			if ( source.eBefore == SIR_GRAPHICS_STATE_UNDEFINED &&
			     ( pSlot->eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL || pSlot->pExternalResource != nullptr ) ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			native.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			native.Transition.pResource = pSlot->getNativeResource();
			native.Transition.StateBefore = sirD3D12BarrierState( source.eBefore, m_eQueue );
			native.Transition.StateAfter = sirD3D12BarrierState( source.eAfter, m_eQueue );
			native.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		} else if ( source.eType == SIR_GRAPHICS_BARRIER_UAV ) {
			if ( ( pSlot->nUsage & SIR_GRAPHICS_USAGE_STORAGE ) == 0 ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			native.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			native.UAV.pResource = pSlot->getNativeResource();
		} else {
			if ( m_eQueue == SIR_GRAPHICS_QUEUE_COPY ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			const sirD3D12ResourceSlot_t *pAliased = m_pResources->querySlot( source.hAliasedResource );
			if ( pAliased == nullptr || pAliased->getNativeResource() == nullptr ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			const sirGraphicsResult_t eAliased = pin( source.hAliasedResource );
			if ( eAliased != SIR_GRAPHICS_SUCCESS ) {
				return eAliased;
			}
			native.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			native.Aliasing.pResourceBefore = pSlot->getNativeResource();
			native.Aliasing.pResourceAfter = pAliased->getNativeResource();
		}
		const sirGraphicsResult_t ePinned = pin( source.hResource );
		if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
			return ePinned;
		}
		// A resource created in a legacy non-COMMON heap state must continue using
		// the legacy barrier API. Only DEFAULT GPU memory uses this enhanced path.
		if ( m_pEnhancedCommands != nullptr &&
		     ( source.eType == SIR_GRAPHICS_BARRIER_TRANSITION || source.eType == SIR_GRAPHICS_BARRIER_UAV ) &&
		     pSlot->eMemory == SIR_GRAPHICS_MEMORY_GPU_LOCAL ) {
			// UAV ordering preserves the current UAV access/layout; transitions
			// use the explicit before/after states supplied by the caller.
			const sirGraphicsState_t eBefore = source.eType == SIR_GRAPHICS_BARRIER_UAV ? SIR_GRAPHICS_STATE_SHADER_WRITE : source.eBefore;
			const sirGraphicsState_t eAfter = source.eType == SIR_GRAPHICS_BARRIER_UAV ? SIR_GRAPHICS_STATE_SHADER_WRITE : source.eAfter;
			const D3D12_BARRIER_SYNC eSyncBefore = sirD3D12EnhancedSync( eBefore );
			const D3D12_BARRIER_SYNC eSyncAfter = sirD3D12EnhancedSync( eAfter );
			const D3D12_BARRIER_ACCESS eAccessBefore = sirD3D12EnhancedAccess( eBefore );
			const D3D12_BARRIER_ACCESS eAccessAfter = sirD3D12EnhancedAccess( eAfter );
			D3D12_BARRIER_GROUP group = {};
			group.NumBarriers = 1;
			if ( pSlot->eType == SIR_GRAPHICS_HANDLE_TEXTURE ) {
				D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
				formatInfo.Format = pSlot->getNativeResource()->GetDesc().Format;
				if ( FAILED( m_pNative->getNativeDevice()->CheckFeatureSupport( D3D12_FEATURE_FORMAT_INFO, &formatInfo,
				                                                                sizeof( formatInfo ) ) ) ||
				     formatInfo.PlaneCount == 0 ) {
					return SIR_GRAPHICS_UNSUPPORTED;
				}
				D3D12_TEXTURE_BARRIER barrier = {};
				barrier.SyncBefore = eSyncBefore;
				barrier.SyncAfter = eSyncAfter;
				barrier.AccessBefore = eAccessBefore;
				barrier.AccessAfter = eAccessAfter;
				barrier.LayoutBefore = sirD3D12EnhancedLayout( eBefore );
				barrier.LayoutAfter = sirD3D12EnhancedLayout( eAfter );
				barrier.pResource = pSlot->getNativeResource();
				const bool bUav = source.eType == SIR_GRAPHICS_BARRIER_UAV;
				barrier.Subresources.IndexOrFirstMipLevel = bUav ? 0 : source.nBaseMip;
				barrier.Subresources.NumMipLevels = bUav                    ? pSlot->nMips
				                                    : source.nMipCount == 0 ? pSlot->nMips - source.nBaseMip
				                                                            : source.nMipCount;
				barrier.Subresources.FirstArraySlice = bUav ? 0 : source.nBaseLayer;
				barrier.Subresources.NumArraySlices =
				    bUav                      ? ( pSlot->eDimension == SIR_GRAPHICS_TEXTURE_3D ? 1 : pSlot->nLayers )
					: source.nLayerCount == 0 ? ( pSlot->eDimension == SIR_GRAPHICS_TEXTURE_3D ? 1 : pSlot->nLayers ) - source.nBaseLayer
					                          : source.nLayerCount;
				barrier.Subresources.FirstPlane = 0;
				barrier.Subresources.NumPlanes = formatInfo.PlaneCount;
				group.Type = D3D12_BARRIER_TYPE_TEXTURE;
				group.pTextureBarriers = &barrier;
				m_pEnhancedCommands->Barrier( 1, &group );
			} else {
				D3D12_BUFFER_BARRIER barrier = {};
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
		if ( source.eType == SIR_GRAPHICS_BARRIER_TRANSITION && pSlot->eType == SIR_GRAPHICS_HANDLE_TEXTURE &&
		     ( source.nBaseMip != 0 || source.nBaseLayer != 0 || ( source.nMipCount != 0 && source.nMipCount != pSlot->nMips ) ||
		       ( source.nLayerCount != 0 && source.nLayerCount != pSlot->nLayers ) ) ) {
			D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {};
			formatInfo.Format = pSlot->getNativeResource()->GetDesc().Format;
			if ( FAILED(
			         m_pNative->getNativeDevice()->CheckFeatureSupport( D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof( formatInfo ) ) ) ||
			     formatInfo.PlaneCount == 0 ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			const uint32_t nMips = pSlot->nMips;
			const uint32_t nLayers = pSlot->eDimension == SIR_GRAPHICS_TEXTURE_3D ? 1 : pSlot->nLayers;
			const uint32_t nMipCount = source.nMipCount == 0 ? nMips - source.nBaseMip : source.nMipCount;
			const uint32_t nLayerCount = source.nLayerCount == 0 ? nLayers - source.nBaseLayer : source.nLayerCount;
			for ( uint32_t iPlane = 0; iPlane < formatInfo.PlaneCount; ++iPlane ) {
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
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource,
                                                     uint64_t nSrcOffset, uint64_t nBytes ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirD3D12ResourceSlot_t *pDst = m_pResources->querySlot( hDestination );
	const sirD3D12ResourceSlot_t *pSrc = m_pResources->querySlot( hSource );
	if ( pDst == nullptr || pSrc == nullptr || pDst->eType != SIR_GRAPHICS_HANDLE_BUFFER || pSrc->eType != SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( nBytes == 0 || nDstOffset > pDst->nSizeBytes || nSrcOffset > pSrc->nSizeBytes || nBytes > pDst->nSizeBytes - nDstOffset ||
	     nBytes > pSrc->nSizeBytes - nSrcOffset ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pDst->nUsage & SIR_GRAPHICS_USAGE_COPY_DESTINATION ) == 0 || ( pSrc->nUsage & SIR_GRAPHICS_USAGE_COPY_SOURCE ) == 0 ||
	     hDestination.nSlot == hSource.nSlot ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsResult_t ePinned = pin( hDestination );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	ePinned = pin( hSource );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	m_pCommands->CopyBufferRegion( pDst->getNativeResource(), nDstOffset, pSrc->getNativeResource(), nSrcOffset, nBytes );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::copyBufferTexture( const sirGraphicsBufferTextureCopy_t& copy ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirD3D12ResourceSlot_t *pBuffer = m_pResources->querySlot( copy.hBuffer );
	const sirD3D12ResourceSlot_t *pTexture = m_pResources->querySlot( copy.hTexture );
	if ( pBuffer == nullptr || pTexture == nullptr || pBuffer->eType != SIR_GRAPHICS_HANDLE_BUFFER ||
	     pTexture->eType != SIR_GRAPHICS_HANDLE_TEXTURE ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pTexture->eDimension != SIR_GRAPHICS_TEXTURE_2D || pTexture->nSamples != 1 || copy.nDepth != 1 ||
	     copy.nLayer >= pTexture->nLayers || copy.nMip >= pTexture->nMips ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nTexelBytes = sirD3D12TexelBytes( pTexture->eFormat );
	if ( nTexelBytes == 0 || copy.nWidth == 0 || copy.nHeight == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nMipWidth = pTexture->nWidth >> copy.nMip ? pTexture->nWidth >> copy.nMip : 1;
	const uint32_t nMipHeight = pTexture->nHeight >> copy.nMip ? pTexture->nHeight >> copy.nMip : 1;
	if ( copy.nX > nMipWidth || copy.nY > nMipHeight || copy.nWidth > nMipWidth - copy.nX || copy.nHeight > nMipHeight - copy.nY ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( copy.nBytesPerRow == 0 || copy.nBytesPerRow % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT != 0 || copy.nRowsPerImage != copy.nHeight ||
	     copy.nBufferOffset % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT != 0 || ( uint64_t )copy.nWidth * nTexelBytes > copy.nBytesPerRow ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint64_t nNeeded = ( uint64_t )copy.nBytesPerRow * copy.nHeight;
	if ( copy.nBufferOffset > pBuffer->nSizeBytes || nNeeded > pBuffer->nSizeBytes - copy.nBufferOffset ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( copy.bBufferToTexture ) {
		if ( ( pBuffer->nUsage & SIR_GRAPHICS_USAGE_COPY_SOURCE ) == 0 ||
		     ( pTexture->nUsage & SIR_GRAPHICS_USAGE_COPY_DESTINATION ) == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( ( pTexture->nUsage & SIR_GRAPHICS_USAGE_COPY_SOURCE ) == 0 ||
	            ( pBuffer->nUsage & SIR_GRAPHICS_USAGE_COPY_DESTINATION ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsResult_t ePinned = pin( copy.hBuffer );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	ePinned = pin( copy.hTexture );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	D3D12_TEXTURE_COPY_LOCATION placed = {};
	placed.pResource = pBuffer->getNativeResource();
	placed.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	placed.PlacedFootprint.Offset = copy.nBufferOffset;
	placed.PlacedFootprint.Footprint.Format = pTexture->getNativeResource()->GetDesc().Format;
	placed.PlacedFootprint.Footprint.Width = copy.nWidth;
	placed.PlacedFootprint.Footprint.Height = copy.nHeight;
	placed.PlacedFootprint.Footprint.Depth = 1;
	placed.PlacedFootprint.Footprint.RowPitch = copy.nBytesPerRow;
	D3D12_TEXTURE_COPY_LOCATION subresource = {};
	subresource.pResource = pTexture->getNativeResource();
	subresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	subresource.SubresourceIndex = copy.nMip + copy.nLayer * pTexture->nMips;
	if ( copy.bBufferToTexture ) {
		m_pCommands->CopyTextureRegion( &subresource, copy.nX, copy.nY, 0, &placed, nullptr );
	} else {
		D3D12_BOX box = { copy.nX, copy.nY, 0, copy.nX + copy.nWidth, copy.nY + copy.nHeight, 1 };
		m_pCommands->CopyTextureRegion( &placed, 0, 0, 0, &subresource, &box );
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::beginRenderPass( const sirGraphicsRenderPass_t& pass ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pass.nColorCount > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT || ( pass.nColorCount != 0 && pass.pColors == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE szColors[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE depth = {};
	for ( uint32_t iColor = 0; iColor < pass.nColorCount; ++iColor ) {
		const sirGraphicsColorAttachment_t& attachment = pass.pColors[iColor];
		if ( !m_pResources->queryDescriptor( attachment.hView, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, &szColors[iColor] ) ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		const sirGraphicsResult_t ePinned = pin( attachment.hView );
		if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
			return ePinned;
		}
	}
	const bool bDepth = pass.hDepthView.nGeneration != 0;
	if ( pass.nColorCount == 0 && !bDepth ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( bDepth ) {
		if ( !m_pResources->queryDescriptor( pass.hDepthView, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, &depth ) ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		const sirGraphicsResult_t ePinned = pin( pass.hDepthView );
		if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
			return ePinned;
		}
	}
	m_pCommands->OMSetRenderTargets( pass.nColorCount, pass.nColorCount == 0 ? nullptr : szColors, FALSE, bDepth ? &depth : nullptr );
	for ( uint32_t iColor = 0; iColor < pass.nColorCount; ++iColor ) {
		const sirGraphicsColorAttachment_t& attachment = pass.pColors[iColor];
		if ( attachment.bClear ) {
			m_pCommands->ClearRenderTargetView( szColors[iColor], attachment.szClearColor, 0, nullptr );
		}
	}
	if ( bDepth && pass.bClearDepth ) {
		m_pCommands->ClearDepthStencilView( depth, D3D12_CLEAR_FLAG_DEPTH, pass.fClearDepth, ( UINT8 )pass.nClearStencil, 0, nullptr );
	}
	m_bRenderPass = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::endRenderPass( void ) noexcept
{
	if ( !validRecording() || !m_bRenderPass ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	m_bRenderPass = false;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept
{
	if ( !validRecording() ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue == SIR_GRAPHICS_QUEUE_COPY ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirD3D12PipelineSlot_t *pPipeline = m_pPipelines->querySlot( hPipeline );
	if ( pPipeline == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pPipeline->eType == SIR_GRAPHICS_PIPELINE_GRAPHICS && m_eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( m_bRenderPass && pPipeline->eType != SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t ePinned = pin( hPipeline );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	m_pCommands->SetPipelineState( pPipeline->pPipelineState.Get() );
	if ( pPipeline->eType == SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
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
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept
{
	if ( !validRecording() ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( !m_bPipelineBound || m_eQueue == SIR_GRAPHICS_QUEUE_COPY ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirD3D12BindingSetSlot_t *pSet = m_pBindings->querySlot( hBindingSet );
	if ( pSet == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSet->nSetIndex != nSetIndex || pSet->hPipeline.nSlot != m_hBoundPipeline.nSlot ||
	     pSet->hPipeline.nGeneration != m_hBoundPipeline.nGeneration ||
	     pSet->hPipeline.nDeviceGeneration != m_hBoundPipeline.nDeviceGeneration ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !m_pBindings->isComplete( *pSet ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	ID3D12DescriptorHeap *szHeaps[2] = { m_pBindings->getHeap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ),
	                                     m_pBindings->getHeap( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) };
	if ( szHeaps[0] == nullptr || szHeaps[1] == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const sirGraphicsResult_t ePinned = pin( hBindingSet );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	m_pCommands->SetDescriptorHeaps( 2, szHeaps );
	for ( uint32_t iRange = 0; iRange < pSet->nRangeCount; ++iRange ) {
		const sirD3D12BindingRange_t& range = pSet->szRanges[iRange];
		if ( m_ePipelineType == SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
			m_pCommands->SetGraphicsRootDescriptorTable( range.nRootIndex, range.hGpu );
		} else {
			m_pCommands->SetComputeRootDescriptorTable( range.nRootIndex, range.hGpu );
		}
		m_szBoundRoots[range.nRootIndex] = 1;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes,
                                                           uint32_t nStrideBytes ) noexcept
{
	if ( !validRecording() ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirD3D12ResourceSlot_t *pBuffer = m_pResources->querySlot( hBuffer );
	if ( pBuffer == nullptr || pBuffer->eType != SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( nSlot >= 16 || nStrideBytes == 0 || nOffsetBytes >= pBuffer->nSizeBytes || pBuffer->nSizeBytes - nOffsetBytes > UINT32_MAX ||
	     ( pBuffer->nUsage & SIR_GRAPHICS_USAGE_VERTEX ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t ePinned = pin( hBuffer );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	D3D12_VERTEX_BUFFER_VIEW native = {};
	native.BufferLocation = pBuffer->getNativeResource()->GetGPUVirtualAddress() + nOffsetBytes;
	native.SizeInBytes = ( UINT )( pBuffer->nSizeBytes - nOffsetBytes );
	native.StrideInBytes = nStrideBytes;
	m_pCommands->IASetVertexBuffers( nSlot, 1, &native );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, bool bIndex32 ) noexcept
{
	if ( !validRecording() ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirD3D12ResourceSlot_t *pBuffer = m_pResources->querySlot( hBuffer );
	if ( pBuffer == nullptr || pBuffer->eType != SIR_GRAPHICS_HANDLE_BUFFER ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const uint32_t nIndexBytes = bIndex32 ? 4U : 2U;
	if ( nOffsetBytes >= pBuffer->nSizeBytes || nOffsetBytes % nIndexBytes != 0 || pBuffer->nSizeBytes - nOffsetBytes > UINT32_MAX ||
	     ( pBuffer->nUsage & SIR_GRAPHICS_USAGE_INDEX ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t ePinned = pin( hBuffer );
	if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
		return ePinned;
	}
	D3D12_INDEX_BUFFER_VIEW native = {};
	native.BufferLocation = pBuffer->getNativeResource()->GetGPUVirtualAddress() + nOffsetBytes;
	native.SizeInBytes = ( UINT )( pBuffer->nSizeBytes - nOffsetBytes );
	native.Format = bIndex32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	m_pCommands->IASetIndexBuffer( &native );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::setViewport( const sirGraphicsViewport_t& viewport, const sirGraphicsRect_t& scissor ) noexcept
{
	if ( !validRecording() ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( m_eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !( viewport.fWidth > 0.0f ) || !( viewport.fHeight > 0.0f ) || viewport.fMinimumDepth < 0.0f || viewport.fMaximumDepth > 1.0f ||
	     viewport.fMinimumDepth > viewport.fMaximumDepth || scissor.nLeft >= scissor.nRight || scissor.nTop >= scissor.nBottom ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	D3D12_VIEWPORT native = {};
	native.TopLeftX = viewport.fX;
	native.TopLeftY = viewport.fY;
	native.Width = viewport.fWidth;
	native.Height = viewport.fHeight;
	native.MinDepth = viewport.fMinimumDepth;
	native.MaxDepth = viewport.fMaximumDepth;
	D3D12_RECT rect = { scissor.nLeft, scissor.nTop, scissor.nRight, scissor.nBottom };
	m_pCommands->RSSetViewports( 1, &native );
	m_pCommands->RSSetScissorRects( 1, &rect );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex,
                                               uint32_t nFirstInstance ) noexcept
{
	if ( !validRecording() || !m_bRenderPass || !m_bPipelineBound || m_ePipelineType != SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nVertices == 0 || nInstances == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < m_nBindingCount; ++iBinding ) {
		if ( m_szBoundRoots[iBinding] == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	m_pCommands->DrawInstanced( nVertices, nInstances, nFirstVertex, nFirstInstance );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset,
                                                      uint32_t nFirstInstance ) noexcept
{
	if ( !validRecording() || !m_bRenderPass || !m_bPipelineBound || m_ePipelineType != SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nIndices == 0 || nInstances == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < m_nBindingCount; ++iBinding ) {
		if ( m_szBoundRoots[iBinding] == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	m_pCommands->DrawIndexedInstanced( nIndices, nInstances, nFirstIndex, nVertexOffset, nFirstInstance );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::dispatch( uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept
{
	if ( !validRecording() || m_bRenderPass || !m_bPipelineBound || m_ePipelineType != SIR_GRAPHICS_PIPELINE_COMPUTE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nX == 0 || nY == 0 || nZ == 0 || nX > 65535 || nY > 65535 || nZ > 65535 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < m_nBindingCount; ++iBinding ) {
		if ( m_szBoundRoots[iBinding] == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	m_pCommands->Dispatch( nX, nY, nZ );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12CommandList::close( void ) noexcept
{
	if ( !validRecording() || m_bRenderPass ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( FAILED( m_pCommands->Close() ) ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	m_bRecording = false;
	m_bClosed = true;
	return SIR_GRAPHICS_SUCCESS;
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
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			if ( !m_szLists[iQueue][iSlot].initialize( pNative, pResources, pPipelines, pBindings, ( sirGraphicsQueue_t )iQueue ) ) {
				for ( uint32_t iCleanup = 0; iCleanup < SIR_GRAPHICS_QUEUE_COUNT; ++iCleanup ) {
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
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
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
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			( void )m_szLists[iQueue][iSlot].completeSubmission();
		}
	}
}

sirGraphicsResult_t SIRD3D12Commands::acquire( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept
{
	if ( ppCommands == nullptr || eQueue >= SIR_GRAPHICS_QUEUE_COUNT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppCommands = nullptr;
	if ( m_pNative == nullptr || !m_pNative->isReady() ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	collectCompleted();
	for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
		SIRD3D12CommandList& commands = m_szLists[eQueue][iSlot];
		// A closed, unsubmitted list still belongs to its recording caller.
		// Only a completed submission (or explicit reset) releases that claim.
		if ( commands.m_bRecording || commands.m_bClosed || commands.m_bSubmitted ) {
			continue;
		}
		const sirGraphicsResult_t eReset = commands.reset();
		if ( eReset != SIR_GRAPHICS_SUCCESS ) {
			return eReset;
		}
		*ppCommands = &commands;
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

sirGraphicsResult_t SIRD3D12Commands::submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t& submission,
                                              sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	if ( m_pNative == nullptr || !m_pNative->isReady() ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	sirD3D12CompletionSet_t snapshot = {};
	if ( !m_pNative->snapshotCompletionSet( &snapshot ) || snapshot.nDeviceGeneration > UINT32_MAX ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const sirGraphicsResult_t eValid = sirGraphicsValidateSubmission( eQueue, submission, ( uint32_t )snapshot.nDeviceGeneration );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	// Image token ownership is validated by SIRD3D12Surfaces before submission.
	if ( submission.nCommandListCount > s_nListsPerQueue ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
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
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iList; ++iPrevious ) {
			if ( submission.ppCommandLists[iPrevious] == submission.ppCommandLists[iList] ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		// External swapchain resources may be referenced only in a submission
		// carrying the matching one-use image acquisition.
		for ( uint32_t iRef = 0; iRef < pMatched->m_nReferenceCount; ++iRef ) {
			const sirGraphicsHandle_t hRef = pMatched->m_szReferences[iRef];
			const sirD3D12ResourceSlot_t *pResource = pMatched->m_pResources->querySlot( hRef );
			sirGraphicsHandle_t hTexture = hRef;
			if ( pResource != nullptr && pResource->eType == SIR_GRAPHICS_HANDLE_VIEW ) {
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
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		szLists[iList] = pMatched->getNativeList();
	}
	const sirGraphicsResult_t eResult =
	    m_pNative->submitRecorded( eQueue, szLists, submission.nCommandListCount, submission.pWaits, submission.nWaitCount, pCompletion );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		for ( uint32_t iSlot = 0; iSlot < s_nListsPerQueue; ++iSlot ) {
			if ( submission.ppCommandLists[iList] == &m_szLists[eQueue][iSlot] ) {
				m_szLists[eQueue][iSlot].setSubmitted( pCompletion->nValue );
			}
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}
