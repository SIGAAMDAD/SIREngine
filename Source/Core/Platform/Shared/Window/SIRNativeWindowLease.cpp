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

#include <Core/Platform/Shared/Window/SIRNativeWindowLease.h>
#include <Core/Platform/Shared/Window/SIRWindowSystem.h>

SIRNativeWindowLease::SIRNativeWindowLease( void ) noexcept
    : m_pOwner( nullptr ),
      m_Lease{},
      m_View{}
{
}

SIRNativeWindowLease::SIRNativeWindowLease( SIRNativeWindowLease&& other ) noexcept
    : m_pOwner( other.m_pOwner ),
      m_Lease( other.m_Lease ),
      m_View( other.m_View )
{
	other.m_pOwner = nullptr;
	other.m_Lease = {};
	other.m_View = {};
}

SIRNativeWindowLease::~SIRNativeWindowLease() noexcept
{
	static_cast<void>( reset() );
}

bool SIRNativeWindowLease::isValid( void ) const noexcept
{
	return m_pOwner != nullptr;
}

const sirWindowNativeView_t& SIRNativeWindowLease::getView( void ) const noexcept
{
	return m_View;
}

sirWindowResult SIRNativeWindowLease::reset( void ) noexcept
{
	if ( m_pOwner == nullptr ) {
		return sirWindowResult::SIR_WINDOW_OK;
	}
	const sirWindowResult eResult = m_pOwner->releaseNativeLease( m_Lease );
	if ( eResult == sirWindowResult::SIR_WINDOW_OK ) {
		m_pOwner = nullptr;
		m_Lease = {};
		m_View = {};
	}
	return eResult;
}
