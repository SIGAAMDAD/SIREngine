#include "SIRVulkanQueue.h"

#include "SIRVulkanCommandList.h"
#include "SIRVulkanDevice.h"
#include "SIRVulkanSynchronization.h"

SIRVulkanQueue::SIRVulkanQueue( SIRVulkanDevice& device, SIRRenderQueueType eType ) noexcept
    : SIRRenderQueue( device, eType )
{
}

SIRRenderResult SIRVulkanQueue::submit( const SIRRenderSubmission& submission ) noexcept
{
	if ( submission.commands.getData() == nullptr || submission.commands.getSize() == 0 ) {
		return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}
	if ( getType() != SIRRenderQueueType::SIR_RENDER_QUEUE_GRAPHICS ) {
		return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
	const uint32_t nWaitCount = submission.waits.getSize();
	const uint32_t nSignalCount = submission.signals.getSize();
	if ( nWaitCount > 8 || nSignalCount > 8 ) {
		return SIRRenderResult::SIR_RENDER_CAPACITY_EXCEEDED;
	}
	if ( submission.waitValues.getSize() != nWaitCount || submission.signalValues.getSize() != nSignalCount ||
	     ( nWaitCount != 0 && ( submission.waits.getData() == nullptr || submission.waitValues.getData() == nullptr ) ) ||
	     ( nSignalCount != 0 && ( submission.signals.getData() == nullptr || submission.signalValues.getData() == nullptr ) ) ) {
		return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < submission.commands.getSize(); ++i ) {
		const SIRVulkanCommandList *pCommandList = dynamic_cast<const SIRVulkanCommandList *>( submission.commands[i] );
		if ( pCommandList == nullptr || pCommandList->getDevice().get() != &getDevice() || pCommandList->getQueueType() != getType() ) {
			return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
		}
	}
	const SIRVulkanSynchronization *waits[8]{};
	SIRVulkanSynchronization *signals[8]{};
	VkSemaphoreSubmitInfo waitInfos[8]{};
	VkSemaphoreSubmitInfo signalInfos[8]{};
	for ( uint32_t i = 0; i < nWaitCount; ++i ) {
		waits[i] = dynamic_cast<const SIRVulkanSynchronization *>( submission.waits[i] );
		if ( waits[i] == nullptr || !waits[i]->isValid() || &waits[i]->getDevice() != &getDevice() || submission.waitValues[i] == 0 ) {
			return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
		}
		waitInfos[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitInfos[i].semaphore = waits[i]->getNativeSemaphore();
		waitInfos[i].value = submission.waitValues[i];
		waitInfos[i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	for ( uint32_t i = 0; i < nSignalCount; ++i ) {
		signals[i] = dynamic_cast<SIRVulkanSynchronization *>( submission.signals[i] );
		if ( signals[i] == nullptr || !signals[i]->isValid() || &signals[i]->getDevice() != &getDevice() ) {
			return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
		}
		for ( uint32_t j = 0; j < nWaitCount; ++j ) {
			if ( signals[i] == waits[j] && submission.waitValues[j] >= submission.signalValues[i] ) {
				return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
			}
		}
	}
	uint32_t nReserved = 0;
	for ( ; nReserved < nSignalCount; ++nReserved ) {
		const SIRRenderResult result = signals[nReserved]->reserveSignalValue( submission.signalValues[nReserved] );
		if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) {
			for ( uint32_t i = 0; i < nReserved; ++i ) {
				signals[i]->finishSignalReservation( submission.signalValues[i], false );
			}
			return result;
		}
		signalInfos[nReserved].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signalInfos[nReserved].semaphore = signals[nReserved]->getNativeSemaphore();
		signalInfos[nReserved].value = submission.signalValues[nReserved];
		signalInfos[nReserved].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	for ( uint32_t i = 0; i < submission.commands.getSize(); ++i ) {
		SIRVulkanCommandList *pCommandList = static_cast<SIRVulkanCommandList *>( submission.commands[i] );
		const sirVulkanSubmitSync_t sync{
		    i == 0 ? waitInfos : nullptr, i == 0 ? nWaitCount : 0,
		    i + 1 == submission.commands.getSize() ? signalInfos : nullptr,
		    i + 1 == submission.commands.getSize() ? nSignalCount : 0 };
		sirVulkanCompletion_t completion{};
		const SIRRenderResult result = pCommandList->submitNative( &completion, &sync );
		if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) {
			for ( uint32_t j = 0; j < nSignalCount; ++j ) {
				signals[j]->finishSignalReservation( submission.signalValues[j], false );
			}
			return result;
		}
		if ( i == 0 ) {
			for ( uint32_t j = 0; j < nWaitCount; ++j ) {
				static_cast<void>( waits[j]->noteGpuUse( completion ) );
			}
		}
		if ( i + 1 == submission.commands.getSize() ) {
			for ( uint32_t j = 0; j < nSignalCount; ++j ) {
				signals[j]->finishSignalReservation( submission.signalValues[j], true );
				static_cast<void>( signals[j]->noteGpuUse( completion ) );
			}
		}
	}
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRVulkanQueue::waitIdle( void ) noexcept
{
	SIRVulkanDevice& device = static_cast<SIRVulkanDevice&>( getDevice() );
	if ( !device.m_bReady || device.m_bDeviceLost || device.m_Procs.vkQueueWaitIdle == nullptr ) {
		return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	}
	VkQueue queue = device.m_GraphicsQueue;
	if ( getType() == SIRRenderQueueType::SIR_RENDER_QUEUE_COMPUTE ) {
		queue = device.m_ComputeQueue;
	} else if ( getType() == SIRRenderQueueType::SIR_RENDER_QUEUE_TRANSFER ) {
		queue = device.m_TransferQueue;
	}
	if ( queue == VK_NULL_HANDLE ) {
		return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
	const VkResult result = device.m_Procs.vkQueueWaitIdle( queue );
	if ( result == VK_ERROR_DEVICE_LOST ) {
		device.m_bDeviceLost = true;
		return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	}
	return result == VK_SUCCESS ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
}

SIRRenderResult SIRVulkanQueue::collectCompleted( void ) noexcept
{
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}
