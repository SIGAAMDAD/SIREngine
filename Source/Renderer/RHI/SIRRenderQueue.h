/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_RENDER_QUEUE_H__
#define __SIR_RENDER_QUEUE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderCommandList;
class SIRRenderSynchronization;
class SIRRenderDevice;

struct SIRRenderSubmission
{
	SIRSpan<SIRRenderCommandList *const> commands{};
	SIRSpan<const SIRRenderSynchronization *const> waits{};
	SIRSpan<SIRRenderSynchronization *const> signals{};
};

class SIRRenderQueue
{
public:
	explicit SIRRenderQueue( const SIRRenderQueue& ) = delete;
	explicit SIRRenderQueue( SIRRenderQueue&& ) = delete;
	virtual ~SIRRenderQueue() noexcept = default;
	SIRRenderQueue& operator=( const SIRRenderQueue& ) = delete;
	SIRRenderQueue& operator=( SIRRenderQueue&& ) = delete;

	SIRRenderQueueType getType( void ) const noexcept
	{
		return m_eType;
	}

	SIRRenderDevice& getDevice( void ) const noexcept
	{
		return *m_pDevice;
	}

	virtual SIRRenderResult submit( const SIRRenderSubmission& submission ) noexcept = 0;
	virtual SIRRenderResult waitIdle( void ) noexcept = 0;
	virtual SIRRenderResult collectCompleted( void ) noexcept = 0;

protected:
	SIRRenderQueue( SIRRenderDevice& device, SIRRenderQueueType eType ) noexcept
	    : m_pDevice( &device ),
	      m_eType( eType )
	{
	}

private:
	SIRRenderDevice *m_pDevice;
	SIRRenderQueueType m_eType;
};
#endif // __SIR_RENDER_QUEUE_H__
