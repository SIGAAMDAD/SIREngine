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

#include <Core/Strings/SIRStringEpoch.h>
#include <Core/Strings/SIRStringOps.h>

SIRStringEpochLease::SIRStringEpochLease( SIRStringEpochLease&& other ) noexcept
    : m_pEpoch( other.m_pEpoch )
{
	other.m_pEpoch = nullptr;
}

SIRStringEpochLease::~SIRStringEpochLease() noexcept
{
	reset();
}

SIRStringEpochLease& SIRStringEpochLease::operator=( SIRStringEpochLease&& other ) noexcept
{
	if ( this != &other ) {
		reset();
		m_pEpoch = other.m_pEpoch;
		other.m_pEpoch = nullptr;
	}
	return *this;
}

bool SIRStringEpochLease::isValid( void ) const noexcept
{
	return m_pEpoch != nullptr;
}

void SIRStringEpochLease::reset( void ) noexcept
{
	if ( m_pEpoch != nullptr ) {
		m_pEpoch->leave();
		m_pEpoch = nullptr;
	}
}

SIRContainerResult SIRStringEpoch::tryEnter( SIRStringEpochLease& lease ) noexcept
{
	if ( lease.isValid() ) {
		return SIRContainerResult::SIR_CONTAINER_BUSY;
	}
	uint32_t nState = m_nState.load( std::memory_order_relaxed );
	for ( ;; ) {
		if ( ( nState & CLOSED ) != 0 ) {
			return SIRContainerResult::SIR_CONTAINER_CLOSED;
		}
		if ( nState == CLOSED - 1 ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		if ( m_nState.compare_exchange_weak( nState, nState + 1, std::memory_order_acquire, std::memory_order_relaxed ) ) {
			lease.m_pEpoch = this;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
	}
}

void SIRStringEpoch::leave( void ) noexcept
{
	m_nState.fetch_sub( 1, std::memory_order_release );
}

bool SIRStringEpoch::close( void ) noexcept
{
	return ( m_nState.fetch_or( CLOSED, std::memory_order_acq_rel ) & ~CLOSED ) == 0;
}

bool SIRStringEpoch::isQuiescent( void ) const noexcept
{
	return m_nState.load( std::memory_order_acquire ) == CLOSED;
}

bool SIRStringEpoch::reopen( void ) noexcept
{
	uint32_t nExpected = CLOSED;
	return m_nState.compare_exchange_strong( nExpected, 0, std::memory_order_release, std::memory_order_relaxed );
}

SIRBiasedString::SIRBiasedString() noexcept
    : m_bOpen( false )
{
	static_cast<void>( m_Epoch.close() );
}

SIRBiasedString::~SIRBiasedString() noexcept
{
	if ( !tryRetire() ) {
		abort();
	}
}

SIRBiasedStringLease::~SIRBiasedStringLease() noexcept
{
	reset();
}

sirStringView_t SIRBiasedStringLease::getView( void ) const noexcept
{
	return m_pOwner == nullptr ? sirStringView_t{ nullptr, 0 } : m_pOwner->m_String.getView();
}

void SIRBiasedStringLease::reset( void ) noexcept
{
	if ( m_pOwner != nullptr ) {
		if ( m_Remote.isValid() ) {
			m_Remote.reset();
		} else {
			--m_pOwner->m_nLocal;
		}
		m_pOwner = nullptr;
	}
}

SIRContainerResult SIRBiasedString::tryAssign( const sirContainerAllocator_t *pAllocator, sirStringView_t view ) noexcept
{
	if ( m_bOpen || m_nLocal != 0 || !m_Epoch.isQuiescent() ) {
		return SIRContainerResult::SIR_CONTAINER_BUSY;
	}
	const SIRContainerResult eResult = m_String.tryAssign( pAllocator, view );
	if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
		m_bOpen = m_Epoch.reopen();
	}
	return eResult;
}

SIRContainerResult SIRBiasedString::tryAcquireLocal( SIRBiasedStringLease& lease ) noexcept
{
	if ( lease.m_pOwner != nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_BUSY;
	}
	if ( !m_bOpen ) {
		return SIRContainerResult::SIR_CONTAINER_CLOSED;
	}
	if ( m_nLocal == UINT32_MAX ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	++m_nLocal;
	lease.m_pOwner = this;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRBiasedString::tryAcquireRemote( SIRBiasedStringLease& lease ) noexcept
{
	if ( lease.m_pOwner != nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_BUSY;
	}
	const SIRContainerResult eResult = m_Epoch.tryEnter( lease.m_Remote );
	if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
		lease.m_pOwner = this;
	}
	return eResult;
}

bool SIRBiasedString::tryRetire( void ) noexcept
{
	m_bOpen = false;
	const bool bDrained = m_Epoch.close();
	if ( !bDrained || m_nLocal != 0 ) {
		return false;
	}
	m_String.reset();
	return true;
}