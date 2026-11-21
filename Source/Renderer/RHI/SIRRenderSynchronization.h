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

#ifndef __SIRENGINE_SIRRENDERSYNCHRONIZATION_H__
#define __SIRENGINE_SIRRENDERSYNCHRONIZATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// One generation-scoped completion token. Actual queue wait/poll and barrier
// execution are delegated to SIRRenderingDevice/SIRGraphicsCommandList.
class SIRRenderSynchronization final
{
public:
	SIRRenderSynchronization( void ) noexcept = default;
	explicit SIRRenderSynchronization( const sirGraphicsCompletion_t& completion ) noexcept : m_Completion( completion )
	{
	}

	sirGraphicsCompletion_t getCompletion( void ) const noexcept
	{
		return m_Completion;
	}
	bool isValid( void ) const noexcept
	{
		return m_Completion.nDeviceGeneration != 0 && m_Completion.nValue != 0 && m_Completion.eQueue < SIR_GRAPHICS_QUEUE_COUNT;
	}

private:
	sirGraphicsCompletion_t m_Completion = {};
};

#endif // __SIRENGINE_SIRRENDERSYNCHRONIZATION_H__
