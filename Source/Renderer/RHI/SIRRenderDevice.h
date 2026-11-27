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

#ifndef __SIR_RENDER_DEVICE_H__
#define __SIR_RENDER_DEVICE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Pointers/SIRSharedPtr.h>
#include <Renderer/RHI/SIRRenderCapabilities.h>
#include <Renderer/RHI/SIRRenderMemoryAllocator.h>

class SIRRenderQueue;

class SIRRenderDevice
{
public:
	SIRRenderDevice( const SIRRenderDevice& ) = delete;
	SIRRenderDevice( SIRRenderDevice&& ) = delete;
	virtual ~SIRRenderDevice() noexcept = default;

	SIRRenderDevice& operator=( const SIRRenderDevice& ) = delete;
	SIRRenderDevice& operator=( SIRRenderDevice&& ) = delete;

	SIRRenderBackend getBackend( void ) const noexcept
	{
		return m_eBackend;
	}

	const SIRRenderCapabilities& getCapabilities( void ) const noexcept
	{
		return m_Capabilities;
	}

	virtual SIRRenderResult initialize( void ) noexcept = 0;
	virtual SIRRenderQueue *getGraphicsQueue( void ) noexcept = 0;
	virtual SIRRenderQueue *getComputeQueue( void ) noexcept = 0;
	virtual SIRRenderQueue *getTransferQueue( void ) noexcept = 0;
	virtual SIRRenderMemoryAllocator& getMemoryAllocator( void ) noexcept = 0;
	virtual SIRRenderResult waitIdle( void ) noexcept = 0;
	virtual SIRRenderResult shutdown( void ) noexcept = 0;
	virtual bool isLost( void ) const noexcept = 0;

protected:
	SIRRenderDevice( SIRRenderBackend eBackend, const SIRRenderCapabilities& capabilities ) noexcept
	    : m_eBackend( eBackend ),
	      m_Capabilities( capabilities )
	{
	}

private:
	SIRRenderBackend m_eBackend;
	SIRRenderCapabilities m_Capabilities;
};

#endif // __SIR_RENDER_DEVICE_H__
