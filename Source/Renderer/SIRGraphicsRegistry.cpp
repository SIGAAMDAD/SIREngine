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

#include <Renderer/SIRGraphicsRegistry.h>
#include <Renderer/SIRGraphicsValidation.h>

static bool sirGraphicsVersionAtLeast( const sirGraphicsVersion_t &available, const sirGraphicsVersion_t &minimum ) noexcept
{
	if ( available.nMajor != minimum.nMajor ) {
		return available.nMajor > minimum.nMajor;
	}
	if ( available.nMinor != minimum.nMinor ) {
		return available.nMinor > minimum.nMinor;
	}
	return available.nPatch >= minimum.nPatch;
}

sirGraphicsResult_t SIRGraphicsRegistry::registerDriver( SIRGraphicsDriver *pDriver ) noexcept
{
	if ( pDriver == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	const sirGraphicsBackend_t eBackend = pDriver->getBackend();
	if ( !sirGraphicsIsBackendValid( eBackend ) || eBackend == SIR_GRAPHICS_BACKEND_AUTO ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( findDriver( eBackend ) != nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_nDriverCount >= s_nDriverCapacity ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	m_pszDrivers[m_nDriverCount] = pDriver;
	++m_nDriverCount;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRGraphicsRegistry::unregisterDriver( sirGraphicsBackend_t eBackend ) noexcept
{
	for ( uint32_t iDriver = 0; iDriver < m_nDriverCount; ++iDriver ) {
		if ( m_pszDrivers[iDriver]->getBackend() != eBackend ) {
			continue;
		}

		for ( uint32_t iShift = iDriver + 1; iShift < m_nDriverCount; ++iShift ) {
			m_pszDrivers[iShift - 1] = m_pszDrivers[iShift];
		}
		--m_nDriverCount;
		m_pszDrivers[m_nDriverCount] = nullptr;
		return SIR_GRAPHICS_SUCCESS;
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}

SIRGraphicsDriver *SIRGraphicsRegistry::findDriver( sirGraphicsBackend_t eBackend ) const noexcept
{
	for ( uint32_t iDriver = 0; iDriver < m_nDriverCount; ++iDriver ) {
		if ( m_pszDrivers[iDriver]->getBackend() == eBackend ) {
			return m_pszDrivers[iDriver];
		}
	}
	return nullptr;
}

sirGraphicsResult_t SIRGraphicsRegistry::selectDriver( const sirGraphicsSelection_t &selection, SIRGraphicsDriver **ppDriver,
                                                       sirGraphicsAdapterInfo_t *pAdapter ) const noexcept
{
	if ( ppDriver == nullptr || pAdapter == nullptr || !sirGraphicsIsBackendValid( selection.eBackend ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDriver = nullptr;
	*pAdapter = {};

	// AUTO is registration-order selection. An explicitly selected API never
	// falls through to another backend when the adapter is unavailable.
	for ( uint32_t iDriver = 0; iDriver < m_nDriverCount; ++iDriver ) {
		SIRGraphicsDriver *pCandidate = m_pszDrivers[iDriver];
		if ( selection.eBackend != SIR_GRAPHICS_BACKEND_AUTO && pCandidate->getBackend() != selection.eBackend ) {
			continue;
		}

		uint32_t nAdapters = 0;
		if ( pCandidate->getAdapterCount( &nAdapters ) != SIR_GRAPHICS_SUCCESS ) {
			continue;
		}
		// Bound all enumeration work; a bogus driver count must never turn
		// renderer selection into an unbounded startup operation.
		if ( nAdapters > 256 ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		for ( uint32_t iAdapter = 0; iAdapter < nAdapters; ++iAdapter ) {
			sirGraphicsAdapterInfo_t info = {};
			if ( pCandidate->queryAdapter( iAdapter, &info ) != SIR_GRAPHICS_SUCCESS ) {
				continue;
			}
			if ( info.bSoftwareAdapter && !selection.bAllowSoftware ) {
				continue;
			}
			if ( !sirGraphicsHasFeatures( info.nFeatureMask, selection.nRequiredFeatures ) ) {
				continue;
			}
			if ( !sirGraphicsVersionAtLeast( info.nApiVersion, selection.nMinimumApiVersion ) ) {
				continue;
			}

			*ppDriver = pCandidate;
			*pAdapter = info;
			return SIR_GRAPHICS_SUCCESS;
		}
	}
	return SIR_GRAPHICS_UNSUPPORTED;
}

uint32_t SIRGraphicsRegistry::getCount( void ) const noexcept
{
	return m_nDriverCount;
}

uint32_t SIRGraphicsRegistry::getCapacity( void ) const noexcept
{
	return s_nDriverCapacity;
}
