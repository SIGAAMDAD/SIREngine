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

#include <Renderer/Drivers/D3D12/SIRD3D12Bindings.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <limits.h>

SIRD3D12Bindings::~SIRD3D12Bindings( void ) noexcept
{
	shutdown( false );
}

bool SIRD3D12Bindings::initialize( SIRD3D12Device *pNative, SIRD3D12Resources *pResources, SIRD3D12Pipelines *pPipelines,
                                   uint32_t nGeneration ) noexcept
{
	if ( m_pNative != nullptr || pNative == nullptr || pResources == nullptr || pPipelines == nullptr || nGeneration == 0 ) {
		return false;
	}
	m_pDevice = pNative->getNativeDevice();
	if ( m_pDevice == nullptr ) {
		return false;
	}
	m_pNative = pNative;
	m_pResources = pResources;
	m_pPipelines = pPipelines;
	m_nDeviceGeneration = nGeneration;
	return true;
}

void SIRD3D12Bindings::shutdown( bool bGpuComplete ) noexcept
{
	if ( bGpuComplete ) {
		for ( uint32_t iSlot = 0; iSlot < s_nSetCapacity; ++iSlot ) {
			releaseSlot( iSlot );
		}
	} else {
		// An uncertain fence cannot authorize recycling shader-visible descriptors
		// or releasing referenced resources.
		for ( uint32_t iSlot = 0; iSlot < s_nSetCapacity; ++iSlot ) {
			const uint32_t nGeneration = m_szSets[iSlot].nGeneration;
			m_szSets[iSlot] = {};
			m_szSets[iSlot].nGeneration = nGeneration;
		}
	}
	m_pNative = nullptr;
	m_pResources = nullptr;
	m_pPipelines = nullptr;
	m_pDevice = nullptr;
	m_nDeviceGeneration = 0;
}

sirD3D12BindingSetSlot_t *SIRD3D12Bindings::findSlot( sirGraphicsHandle_t hSet ) noexcept
{
	if ( m_pNative == nullptr || hSet.nSlot >= s_nSetCapacity ||
	     !sirGraphicsIsHandleValid( hSet, SIR_GRAPHICS_HANDLE_BINDING_SET, m_nDeviceGeneration ) ) {
		return nullptr;
	}
	sirD3D12BindingSetSlot_t& slot = m_szSets[hSet.nSlot];
	return slot.bLive && slot.nGeneration == hSet.nGeneration ? &slot : nullptr;
}

const sirD3D12BindingSetSlot_t *SIRD3D12Bindings::querySlot( sirGraphicsHandle_t hSet ) noexcept
{
	return findSlot( hSet );
}

void SIRD3D12Bindings::releaseSlot( uint32_t iSlot ) noexcept
{
	sirD3D12BindingSetSlot_t& slot = m_szSets[iSlot];
	if ( !slot.bLive && !slot.bRetiring ) {
		return;
	}
	for ( uint32_t iFlat = 0; iFlat < slot.nFlatCount; ++iFlat ) {
		if ( slot.szResources[iFlat].nGeneration != 0 ) {
			m_pResources->releaseCommandReference( slot.szResources[iFlat] );
		}
	}
	for ( uint32_t iRange = 0; iRange < slot.nRangeCount; ++iRange ) {
		const sirD3D12BindingRange_t& range = slot.szRanges[iRange];
		if ( range.bAllocated ) {
			m_pResources->releaseDescriptorSpan( range.eHeap, range.iDescriptor, range.nDescriptorCount );
		}
	}
	if ( slot.hPipeline.nGeneration != 0 ) {
		m_pPipelines->releaseCommandReference( slot.hPipeline );
	}
	const uint32_t nGeneration = slot.nGeneration;
	slot = {};
	slot.nGeneration = nGeneration;
}

bool SIRD3D12Bindings::pollRetirement( const sirD3D12CompletionSet_t& completion, bool *pCompleted ) noexcept
{
	return m_pNative != nullptr && m_pNative->pollCompletionSet( completion, pCompleted );
}

void SIRD3D12Bindings::collectRetired( void ) noexcept
{
	if ( m_pNative == nullptr ) {
		return;
	}
	for ( uint32_t iSlot = 0; iSlot < s_nSetCapacity; ++iSlot ) {
		sirD3D12BindingSetSlot_t& slot = m_szSets[iSlot];
		if ( !slot.bRetiring || slot.nCommandReferences != 0 ) {
			continue;
		}
		bool bCompleted = false;
		if ( !pollRetirement( slot.retireCompletion, &bCompleted ) ) {
			return;
		}
		if ( bCompleted ) {
			releaseSlot( iSlot );
		}
	}
}

sirGraphicsResult_t SIRD3D12Bindings::createSet( sirGraphicsHandle_t hPipeline, uint32_t nSetIndex, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	if ( m_pNative == nullptr || m_pDevice == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const sirD3D12PipelineSlot_t *pPipeline = m_pPipelines->querySlot( hPipeline );
	if ( pPipeline == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	uint32_t nRanges = 0;
	uint32_t nTotal = 0;
	for ( uint32_t i = 0; i < pPipeline->nBindingCount; ++i ) {
		const sirGraphicsBindingDesc_t& binding = pPipeline->szBindings[i];
		if ( binding.nSpace != nSetIndex ) {
			continue;
		}
		if ( binding.nArrayCount == 0 || binding.nArrayCount > s_nDescriptorCapacity ||
		     nTotal > s_nDescriptorCapacity - binding.nArrayCount ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			const sirGraphicsBindingDesc_t& prior = pPipeline->szBindings[j];
			if ( prior.nSpace == nSetIndex && prior.nBinding == binding.nBinding ) {
				// Descriptor writes identify registers, not range type.
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		nTotal += binding.nArrayCount;
		++nRanges;
	}
	if ( nRanges == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	collectRetired();
	uint32_t iFree = s_nSetCapacity;
	for ( uint32_t i = 0; i < s_nSetCapacity; ++i ) {
		if ( !m_szSets[i].bLive && !m_szSets[i].bRetiring && m_szSets[i].nGeneration < UINT32_MAX ) {
			iFree = i;
			break;
		}
	}
	if ( iFree == s_nSetCapacity ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const sirGraphicsResult_t ePin = m_pPipelines->retainCommandReference( hPipeline );
	if ( ePin != SIR_GRAPHICS_SUCCESS ) {
		return ePin;
	}
	sirD3D12BindingSetSlot_t& slot = m_szSets[iFree];
	const uint32_t nGeneration = slot.nGeneration + 1;
	slot = {};
	slot.nGeneration = nGeneration;
	slot.hPipeline = hPipeline;
	slot.nSetIndex = nSetIndex;
	slot.bLive = true;
	for ( uint32_t i = 0; i < pPipeline->nBindingCount; ++i ) {
		const sirGraphicsBindingDesc_t& binding = pPipeline->szBindings[i];
		if ( binding.nSpace != nSetIndex ) {
			continue;
		}
		sirD3D12BindingRange_t& range = slot.szRanges[slot.nRangeCount];
		range.desc = binding;
		range.nRootIndex = i;
		range.nFlatStart = slot.nFlatCount;
		range.nDescriptorCount = binding.nArrayCount;
		range.eHeap =
		    binding.eType == SIR_GRAPHICS_BINDING_SAMPLER ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		const sirGraphicsResult_t eReserved =
		    m_pResources->reserveDescriptorSpan( range.eHeap, binding.nArrayCount, &range.iDescriptor, &range.hCpu, &range.hGpu );
		if ( eReserved != SIR_GRAPHICS_SUCCESS ) {
			releaseSlot( iFree );
			return eReserved;
		}
		range.bAllocated = true;
		slot.nFlatCount += binding.nArrayCount;
		++slot.nRangeCount;
	}
	*pHandle = { iFree, nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_BINDING_SET };
	return SIR_GRAPHICS_SUCCESS;
}

bool SIRD3D12Bindings::isComplete( const sirD3D12BindingSetSlot_t& slot ) const noexcept
{
	if ( slot.nRangeCount == 0 ) {
		return false;
	}
	for ( uint32_t i = 0; i < slot.nFlatCount; ++i ) {
		if ( slot.szResources[i].nGeneration == 0 ) {
			return false;
		}
	}
	return true;
}

ID3D12DescriptorHeap *SIRD3D12Bindings::getHeap( D3D12_DESCRIPTOR_HEAP_TYPE eHeap ) noexcept
{
	return m_pResources != nullptr ? m_pResources->getShaderVisibleHeap( eHeap ) : nullptr;
}

sirGraphicsResult_t SIRD3D12Bindings::updateSet( sirGraphicsHandle_t hSet, const sirGraphicsDescriptorWrite_t *pWrites,
                                                 uint32_t nWriteCount ) noexcept
{
	sirD3D12BindingSetSlot_t *pSlot = findSlot( hSet );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( nWriteCount == 0 ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( pWrites == nullptr || nWriteCount > s_nDescriptorCapacity ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSlot->nCommandReferences != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	uint32_t szFlat[s_nDescriptorCapacity] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE szSource[s_nDescriptorCapacity] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE szTarget[s_nDescriptorCapacity] = {};
	D3D12_DESCRIPTOR_HEAP_TYPE szHeaps[s_nDescriptorCapacity] = {};
	for ( uint32_t iWrite = 0; iWrite < nWriteCount; ++iWrite ) {
		const sirGraphicsDescriptorWrite_t& write = pWrites[iWrite];
		if ( write.nOffsetBytes != 0 || write.nRangeBytes != 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		const sirD3D12BindingRange_t *pRange = nullptr;
		for ( uint32_t iRange = 0; iRange < pSlot->nRangeCount; ++iRange ) {
			if ( pSlot->szRanges[iRange].desc.nBinding == write.nBinding ) {
				pRange = &pSlot->szRanges[iRange];
				break;
			}
		}
		if ( pRange == nullptr || write.nArrayIndex >= pRange->nDescriptorCount ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirD3D12ResourceSlot_t *pResource = m_pResources->querySlot( write.hResource );
		if ( pResource == nullptr ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		const uint32_t nExpectedUsage = pRange->desc.eType == SIR_GRAPHICS_BINDING_CONSTANT_BUFFER   ? SIR_GRAPHICS_USAGE_CONSTANT
		                                : pRange->desc.eType == SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE ? SIR_GRAPHICS_USAGE_SAMPLED
		                                                                                             : SIR_GRAPHICS_USAGE_STORAGE;
		if ( pRange->desc.eType == SIR_GRAPHICS_BINDING_SAMPLER ) {
			if ( pResource->eType != SIR_GRAPHICS_HANDLE_SAMPLER ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		} else if ( pResource->eType != SIR_GRAPHICS_HANDLE_VIEW || pResource->nUsage != nExpectedUsage ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !m_pResources->queryDescriptor( write.hResource, pRange->eHeap, &szSource[iWrite] ) ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		const uint32_t iFlat = pRange->nFlatStart + write.nArrayIndex;
		for ( uint32_t iPrior = 0; iPrior < iWrite; ++iPrior ) {
			if ( szFlat[iPrior] == iFlat ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
		szFlat[iWrite] = iFlat;
		szHeaps[iWrite] = pRange->eHeap;
		szTarget[iWrite] = pRange->hCpu;
		szTarget[iWrite].ptr += ( SIZE_T )write.nArrayIndex * m_pDevice->GetDescriptorHandleIncrementSize( pRange->eHeap );
	}
	uint32_t nRetained = 0;
	for ( uint32_t iWrite = 0; iWrite < nWriteCount; ++iWrite ) {
		const sirGraphicsResult_t ePinned = m_pResources->retainCommandReference( pWrites[iWrite].hResource );
		if ( ePinned != SIR_GRAPHICS_SUCCESS ) {
			for ( uint32_t iPinned = 0; iPinned < nRetained; ++iPinned ) {
				m_pResources->releaseCommandReference( pWrites[iPinned].hResource );
			}
			return ePinned;
		}
		++nRetained;
	}
	for ( uint32_t iWrite = 0; iWrite < nWriteCount; ++iWrite ) {
		m_pDevice->CopyDescriptorsSimple( 1, szTarget[iWrite], szSource[iWrite], szHeaps[iWrite] );
		sirGraphicsHandle_t& hOld = pSlot->szResources[szFlat[iWrite]];
		if ( hOld.nGeneration != 0 ) {
			m_pResources->releaseCommandReference( hOld );
		}
		hOld = pWrites[iWrite].hResource;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Bindings::retainCommandReference( sirGraphicsHandle_t hSet ) noexcept
{
	sirD3D12BindingSetSlot_t *pSlot = findSlot( hSet );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->nCommandReferences == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	++pSlot->nCommandReferences;
	return SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12Bindings::releaseCommandReference( sirGraphicsHandle_t hSet ) noexcept
{
	sirD3D12BindingSetSlot_t *pSlot = findSlot( hSet );
	if ( pSlot != nullptr && pSlot->nCommandReferences != 0 ) {
		--pSlot->nCommandReferences;
	}
}

sirGraphicsResult_t SIRD3D12Bindings::retireSet( sirGraphicsHandle_t hSet, const sirGraphicsCompletionSet_t& completion ) noexcept
{
	sirD3D12BindingSetSlot_t *pSlot = findSlot( hSet );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSlot->nCommandReferences != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirD3D12CompletionSet_t native = {};
	native.nDeviceGeneration = completion.nDeviceGeneration;
	native.nGraphicsFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
	native.nComputeFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE];
	native.nCopyFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY];
	bool bCompleted = false;
	if ( !pollRetirement( native, &bCompleted ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pSlot->bLive = false;
	pSlot->bRetiring = true;
	pSlot->retireCompletion = native;
	if ( bCompleted ) {
		releaseSlot( hSet.nSlot );
	}
	return SIR_GRAPHICS_SUCCESS;
}
