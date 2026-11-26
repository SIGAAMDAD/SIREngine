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

#include "SIRResourcePinPtr.h"
#include <stdlib.h>

SIRResourcePinPtr::SIRResourcePinPtr( void ) noexcept : m_pGraph( nullptr ), m_hPin{}, m_nRevision( 0 )
{
}

SIRResourcePinPtr::~SIRResourcePinPtr( void ) noexcept
{
	if ( reset() != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		abort(); // An owned pin must not become invalid while its graph exists.
	}
}

SIRResourcePinPtr::SIRResourcePinPtr( SIRResourcePinPtr&& rOther ) noexcept
    : m_pGraph( rOther.m_pGraph ), m_hPin( rOther.m_hPin ), m_nRevision( rOther.m_nRevision )
{
	rOther.m_pGraph = nullptr;
	rOther.m_hPin = {};
	rOther.m_nRevision = 0;
}

SIRResourcePinPtr& SIRResourcePinPtr::operator=( SIRResourcePinPtr&& rOther ) noexcept
{
	if ( this == &rOther ) {
		return *this;
	}
	if ( reset() != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		abort();
	}
	m_pGraph = rOther.m_pGraph;
	m_hPin = rOther.m_hPin;
	m_nRevision = rOther.m_nRevision;
	rOther.m_pGraph = nullptr;
	rOther.m_hPin = {};
	rOther.m_nRevision = 0;
	return *this;
}

bool SIRResourcePinPtr::isValid( void ) const noexcept
{
	return m_pGraph != nullptr;
}

SIRResourcePinHandle SIRResourcePinPtr::getHandle( void ) const noexcept
{
	return m_hPin;
}

uint64_t SIRResourcePinPtr::getRevision( void ) const noexcept
{
	return m_nRevision;
}

SIRResourceResult SIRResourcePinPtr::reset( void ) noexcept
{
	if ( m_pGraph == nullptr ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}
	const SIRResourceResult eResult = m_pGraph->releaseProviderPin( m_hPin );
	if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	m_pGraph = nullptr;
	m_hPin = {};
	m_nRevision = 0;
	return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourcePinPtr::pin( const SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, const SIRResourceView& rView,
                                          uint32_t nProviderId ) noexcept
{
	if ( isValid() ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_BUSY;
	}
	SIRResourcePinHandle hPin {};
	const SIRResourceResult eResult = rGraph.pinForProvider( rScope, rView, nProviderId, &hPin );
	if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	m_pGraph = &rGraph;
	m_hPin = hPin;
	m_nRevision = rView.getRevision();
	return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
}
