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

static bool sirGraphicsVersionAtLeast( const sirGraphicsVersion_t& available, const sirGraphicsVersion_t& minimum ) noexcept
{
	if ( available.nMajor != minimum.nMajor ) {
		return available.nMajor > minimum.nMajor;
	}
	if ( available.nMinor != minimum.nMinor ) {
		return available.nMinor > minimum.nMinor;
	}
	return available.nPatch >= minimum.nPatch;
}

// AUTO ranking uses reported capabilities, not registration order or API version
// comparisons across unrelated graphics APIs. Hardware always wins over a
// software adapter when both satisfy the caller's contract.
static uint32_t sirGraphicsCountFeatureBits( uint64_t nMask ) noexcept
{
	uint32_t nCount = 0;
	while ( nMask != 0 ) {
		nMask &= nMask - 1;
		++nCount;
	}
	return nCount;
}

static bool sirGraphicsPreferAdapter( const sirGraphicsAdapterInfo_t& candidate, sirGraphicsBackend_t eCandidate,
                                      const sirGraphicsAdapterInfo_t& current, sirGraphicsBackend_t eCurrent ) noexcept
{
	if ( candidate.bSoftwareAdapter != current.bSoftwareAdapter ) {
		return !candidate.bSoftwareAdapter;
	}
	const uint32_t nCandidateFeatures = sirGraphicsCountFeatureBits( candidate.nFeatureMask );
	const uint32_t nCurrentFeatures = sirGraphicsCountFeatureBits( current.nFeatureMask );
	if ( nCandidateFeatures != nCurrentFeatures ) {
		return nCandidateFeatures > nCurrentFeatures;
	}
	if ( candidate.nDedicatedVideoBytes != current.nDedicatedVideoBytes ) {
		return candidate.nDedicatedVideoBytes > current.nDedicatedVideoBytes;
	}
	if ( candidate.nSharedSystemBytes != current.nSharedSystemBytes ) {
		return candidate.nSharedSystemBytes > current.nSharedSystemBytes;
	}
	if ( eCandidate != eCurrent ) {
		return eCandidate < eCurrent;
	}
	return candidate.nAdapterIndex < current.nAdapterIndex;
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
	// Backends are unique and all valid IDs are within 1..OPENGL.
	// Capacity is therefore a compile-time invariant, not a runtime branch.
	static_assert( s_nDriverCapacity >= SIR_GRAPHICS_BACKEND_OPENGL, "Graphics registry capacity must fit every backend" );

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

sirGraphicsResult_t SIRGraphicsRegistry::selectDriver( const sirGraphicsSelection_t& selection, SIRGraphicsDriver **ppDriver,
                                                       sirGraphicsAdapterInfo_t *pAdapter ) const noexcept
{
	if ( ppDriver == nullptr || pAdapter == nullptr || !sirGraphicsIsBackendValid( selection.eBackend ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppDriver = nullptr;
	*pAdapter = {};
	if ( selection.eBackend == SIR_GRAPHICS_BACKEND_AUTO &&
	     ( selection.nMinimumApiVersion.nMajor != 0 || selection.nMinimumApiVersion.nMinor != 0 ||
	       selection.nMinimumApiVersion.nPatch != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( selection.nMinimumApiVersion.nMajor == 0 &&
	     ( selection.nMinimumApiVersion.nMinor != 0 || selection.nMinimumApiVersion.nPatch != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	// An explicitly requested API never falls through to another backend.
	// AUTO ranks all eligible physical adapters before software fallbacks.
	SIRGraphicsDriver *pBestDriver = nullptr;
	sirGraphicsAdapterInfo_t best = {};
	for ( uint32_t iDriver = 0; iDriver < m_nDriverCount; ++iDriver ) {
		SIRGraphicsDriver *pCandidate = m_pszDrivers[iDriver];
		const sirGraphicsBackend_t eCandidate = pCandidate->getBackend();
		if ( selection.eBackend != SIR_GRAPHICS_BACKEND_AUTO && eCandidate != selection.eBackend ) {
			continue;
		}

		uint32_t nAdapters = 0;
		const sirGraphicsResult_t eCount = pCandidate->getAdapterCount( &nAdapters );
		if ( eCount != SIR_GRAPHICS_SUCCESS ) {
			if ( selection.eBackend != SIR_GRAPHICS_BACKEND_AUTO ) {
				return eCount;
			}
			continue;
		}
		if ( nAdapters > 256 ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		for ( uint32_t iAdapter = 0; iAdapter < nAdapters; ++iAdapter ) {
			sirGraphicsAdapterInfo_t info = {};
			const sirGraphicsResult_t eProbe = pCandidate->queryAdapter( iAdapter, &info );
			if ( eProbe != SIR_GRAPHICS_SUCCESS ) {
				if ( selection.eBackend != SIR_GRAPHICS_BACKEND_AUTO ) {
					return eProbe;
				}
				continue;
			}
			if ( info.nAdapterIndex != iAdapter || info.nApiVersion.nMajor == 0 ||
			     ( info.nFeatureMask & ~( ( SIR_GRAPHICS_FEATURE_PRESENT << 1 ) - 1ULL ) ) != 0 ) {
				return SIR_GRAPHICS_DRIVER_ERROR;
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
			if ( pBestDriver == nullptr || sirGraphicsPreferAdapter( info, eCandidate, best, pBestDriver->getBackend() ) ) {
				pBestDriver = pCandidate;
				best = info;
			}
		}
	}
	if ( pBestDriver == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*ppDriver = pBestDriver;
	*pAdapter = best;
	return SIR_GRAPHICS_SUCCESS;
}

uint32_t SIRGraphicsRegistry::getCount( void ) const noexcept
{
	return m_nDriverCount;
}

uint32_t SIRGraphicsRegistry::getCapacity( void ) const noexcept
{
	return s_nDriverCapacity;
}
