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

#ifndef __SIRENGINE_SIRRENDERQUEUE_H__
#define __SIRENGINE_SIRRENDERQUEUE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/SIRRenderTypes.h>

class SIRRenderingDevice;

// Borrowed queue interface for one logical execution class; no second queue
// or submission engine is created. Calls delegate to SIRRenderingDevice.
class SIRRenderQueue final
{
public:
	SIRRenderQueue( void ) noexcept = default;
	SIRRenderQueue( SIRRenderingDevice& device, sirGraphicsQueue_t eQueue ) noexcept : m_pDevice( &device ), m_eQueue( eQueue )
	{
	}

	sirGraphicsQueue_t getType( void ) const noexcept
	{
		return m_eQueue;
	}
	bool isValid( void ) const noexcept
	{
		return m_pDevice != nullptr && m_eQueue < SIR_GRAPHICS_QUEUE_COUNT;
	}

	sirGraphicsResult_t submit( const sirGraphicsSubmission_t& submission, sirGraphicsCompletion_t *pCompletion ) noexcept;
	sirGraphicsResult_t poll( const sirGraphicsCompletion_t& completion, bool *pComplete ) noexcept;
	sirGraphicsResult_t wait( const sirGraphicsCompletion_t& completion, uint64_t nTimeoutNanoseconds ) noexcept;

private:
	SIRRenderingDevice *m_pDevice = nullptr;
	sirGraphicsQueue_t m_eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
};

#endif // __SIRENGINE_SIRRENDERQUEUE_H__
