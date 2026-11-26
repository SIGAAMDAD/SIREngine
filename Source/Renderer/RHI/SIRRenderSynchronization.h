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

#ifndef __SIR_RENDER_SYNCHRONIZATION_H__
#define __SIR_RENDER_SYNCHRONIZATION_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/RHI/SIRRenderTypes.h>

class SIRRenderSynchronization
{
public:
	explicit SIRRenderSynchronization( const SIRRenderSynchronization& ) = delete;
	explicit SIRRenderSynchronization( SIRRenderSynchronization&& ) = delete;
	virtual ~SIRRenderSynchronization() noexcept = default;
	SIRRenderSynchronization& operator=( const SIRRenderSynchronization& ) = delete;
	SIRRenderSynchronization& operator=( SIRRenderSynchronization&& ) = delete;

	SIRRenderSynchronizationType getType( void ) const noexcept
	{
		return m_eType;
	}

	virtual SIRRenderResult signal( uint64_t nValue ) noexcept = 0;
	virtual SIRRenderResult wait( uint64_t nValue, uint64_t nTimeoutNanoseconds ) noexcept = 0;
	virtual uint64_t getCompletedValue( void ) const noexcept = 0;

protected:
	explicit SIRRenderSynchronization( SIRRenderSynchronizationType eType ) noexcept
	    : m_eType( eType )
	{
	}

private:
	SIRRenderSynchronizationType m_eType;
};
#endif // __SIR_RENDER_SYNCHRONIZATION_H__
