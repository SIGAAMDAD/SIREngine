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

#ifndef __SIR_RENDER_RESOURCE_H__
#define __SIR_RENDER_RESOURCE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Pointers/SIRSharedPtr.h>

class SIRRenderDevice;

class SIRRenderResource
{
public:
	SIRRenderResource( const SIRRenderResource& ) = delete;
	SIRRenderResource( SIRRenderResource&& ) = delete;
	SIRRenderResource& operator=( const SIRRenderResource& ) = delete;
	SIRRenderResource& operator=( SIRRenderResource&& ) = delete;
	virtual ~SIRRenderResource() noexcept = default;

	SIR_FORCEINLINE const SIRSharedPtr<SIRRenderDevice>& getDevice( void ) const noexcept
	{
		return m_pDevice;
	}

	template<typename TDevice = SIRRenderDevice>
	SIR_FORCEINLINE TDevice& getDevice() const noexcept
	{
		static_assert( std::is_base_of_v<SIRRenderDevice, TDevice> );
		return static_cast<TDevice&>( *m_pDevice.get() );
	}

	virtual bool isValid( void ) const noexcept = 0;

protected:
	explicit SIRRenderResource( const SIRSharedPtr<SIRRenderDevice>& pDevice ) noexcept
	    : m_pDevice( pDevice )
	{
	}

private:
	SIRSharedPtr<SIRRenderDevice> m_pDevice;
};

#endif // __SIR_RENDER_RESOURCE_H__
