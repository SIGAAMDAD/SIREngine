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

#include <Audio/FMOD/SIRFMODPackageLoader.h>
#include <Core/Containers/SIRRawStorage.h>
#include <stdlib.h>
#include <string.h>

SIRFMODPackageLoader::~SIRFMODPackageLoader( void ) noexcept
{
	if ( shutdown() != SIR_AUDIO_OK ) {
		// Native bank handles cannot be discarded without a completed unload.
		abort();
	}
}

bool SIRFMODPackageLoader::findIndex( uint64_t nPackageId, uint32_t &iPackage ) const noexcept
{
	uint32_t iFirst = 0;
	uint32_t iEnd = m_nPackageCount;
	while ( iFirst < iEnd ) {
		const uint32_t iMiddle = iFirst + ( iEnd - iFirst ) / 2;
		if ( m_pEntries[iMiddle].nPackageId < nPackageId ) {
			iFirst = iMiddle + 1;
		} else {
			iEnd = iMiddle;
		}
	}
	iPackage = iFirst;
	return iFirst < m_nPackageCount && m_pEntries[iFirst].nPackageId == nPackageId;
}

sirAudioResult_t SIRFMODPackageLoader::buildPlan( uint32_t iRoot, uint32_t &nPlanCount ) noexcept
{
	nPlanCount = 0;
	for ( uint32_t iPackage = 0; iPackage < m_nPackageCount; ++iPackage ) {
		m_pEntries[iPackage].bVisited = 0;
	}
	m_pEntries[iRoot].bVisited = 1;
	m_pFrames[0] = { iRoot, 0 };
	uint32_t nDepth = 1;

	while ( nDepth != 0 ) {
		sirFMODPackageFrame_t &frame = m_pFrames[nDepth - 1];
		sirAudioPackageRecord_t package = {};
		if ( !m_pManifest->getPackageAt( frame.iPackage, package ) ) {
			return SIR_AUDIO_INVALID_HANDLE;
		}
		if ( frame.iNextDependency < package.nDependencyCount ) {
			uint64_t nDependencyId = 0;
			if ( !m_pManifest->getPackageDependency( package.nPackageId, frame.iNextDependency, nDependencyId ) ) {
				return SIR_AUDIO_INVALID_HANDLE;
			}
			++frame.iNextDependency;
			uint32_t iDependency = 0;
			if ( !findIndex( nDependencyId, iDependency ) ) {
				return SIR_AUDIO_INVALID_HANDLE;
			}
			if ( m_pEntries[iDependency].bVisited == 1 ) {
				return SIR_AUDIO_INVALID_ARGUMENT;
			}
			if ( m_pEntries[iDependency].bVisited == 0 ) {
				m_pEntries[iDependency].bVisited = 1;
				m_pFrames[nDepth] = { iDependency, 0 };
				++nDepth;
			}
			continue;
		}
		m_pEntries[frame.iPackage].bVisited = 2;
		m_pPlan[nPlanCount++] = frame.iPackage;
		--nDepth;
	}
	return SIR_AUDIO_OK;
}

void SIRFMODPackageLoader::releaseStorage( void ) noexcept
{
	if ( m_pEntries != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pEntries, ( size_t )m_nPackageCount * sizeof( sirFMODPackageEntry_t ),
		                           alignof( sirFMODPackageEntry_t ) );
	}
	if ( m_pFrames != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pFrames, ( size_t )m_nPackageCount * sizeof( sirFMODPackageFrame_t ),
		                           alignof( sirFMODPackageFrame_t ) );
	}
	if ( m_pPlan != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pPlan, ( size_t )m_nPackageCount * sizeof( uint32_t ), alignof( uint32_t ) );
	}
	if ( m_pLoadOrder != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pLoadOrder, ( size_t )m_nPackageCount * sizeof( uint32_t ), alignof( uint32_t ) );
	}
	m_pEntries = nullptr;
	m_pFrames = nullptr;
	m_pPlan = nullptr;
	m_pLoadOrder = nullptr;
	m_pManifest = nullptr;
	m_pBanks = nullptr;
	m_pAllocator = nullptr;
	m_Resolver = {};
	m_nManifestEpoch = 0;
	m_nPackageCount = 0;
	m_nLoadedCount = 0;
}

sirAudioResult_t SIRFMODPackageLoader::initialize( const SIRAudioResourceStore *pManifest, SIRFMODBankStore *pBanks,
                                                   const sirContainerAllocator_t *pAllocator,
                                                   sirFMODPackagePathResolver_t resolver ) noexcept
{
	if ( m_pEntries != nullptr || pManifest == nullptr || !pManifest->isLoaded() || pManifest->getProvider() != SIR_AUDIO_PROVIDER_FMOD ||
	     pBanks == nullptr || !pBanks->isReady() || pBanks->getLoadedBankCount() != 0 || pBanks->getBoundEventCount() != 0 ||
	     !SIRRawStorage::isValidAllocator( pAllocator ) || resolver.resolve == nullptr ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	const uint32_t nPackageCount = pManifest->getPackageCount();
	if ( nPackageCount == 0 ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}

	m_pAllocator = pAllocator;
	m_nPackageCount = nPackageCount;
	void *pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, ( size_t )nPackageCount * sizeof( sirFMODPackageEntry_t ),
	                                 alignof( sirFMODPackageEntry_t ), &pMemory ) != SIR_CONTAINER_OK ) {
		releaseStorage();
		return SIR_AUDIO_CAPACITY;
	}
	m_pEntries = ( sirFMODPackageEntry_t * )pMemory;
	pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, ( size_t )nPackageCount * sizeof( sirFMODPackageFrame_t ),
	                                 alignof( sirFMODPackageFrame_t ), &pMemory ) != SIR_CONTAINER_OK ) {
		releaseStorage();
		return SIR_AUDIO_CAPACITY;
	}
	m_pFrames = ( sirFMODPackageFrame_t * )pMemory;
	pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, ( size_t )nPackageCount * sizeof( uint32_t ), alignof( uint32_t ), &pMemory ) !=
	     SIR_CONTAINER_OK ) {
		releaseStorage();
		return SIR_AUDIO_CAPACITY;
	}
	m_pPlan = ( uint32_t * )pMemory;
	pMemory = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, ( size_t )nPackageCount * sizeof( uint32_t ), alignof( uint32_t ), &pMemory ) !=
	     SIR_CONTAINER_OK ) {
		releaseStorage();
		return SIR_AUDIO_CAPACITY;
	}
	m_pLoadOrder = ( uint32_t * )pMemory;
	memset( m_pEntries, 0, ( size_t )nPackageCount * sizeof( sirFMODPackageEntry_t ) );
	for ( uint32_t iPackage = 0; iPackage < nPackageCount; ++iPackage ) {
		sirAudioPackageRecord_t package = {};
		if ( !pManifest->getPackageAt( iPackage, package ) ) {
			releaseStorage();
			return SIR_AUDIO_INVALID_ARGUMENT;
		}
		m_pEntries[iPackage].nPackageId = package.nPackageId;
	}
	m_pManifest = pManifest;
	m_pBanks = pBanks;
	m_Resolver = resolver;
	m_nManifestEpoch = pManifest->getEpoch();
	m_nLastFailedPackageId = 0;
	return SIR_AUDIO_OK;
}

bool SIRFMODPackageLoader::isReady( void ) const noexcept
{
	return m_pEntries != nullptr && m_pManifest != nullptr && m_pManifest->isLoaded() && m_pManifest->getEpoch() == m_nManifestEpoch &&
	       m_pBanks != nullptr && m_pBanks->isReady();
}

sirAudioResult_t SIRFMODPackageLoader::rollbackTo( uint32_t nOldCount ) noexcept
{
	while ( m_nLoadedCount > nOldCount ) {
		const uint32_t iPackage = m_pLoadOrder[m_nLoadedCount - 1];
		const sirAudioResult_t eResult = m_pBanks->unloadBank( m_pEntries[iPackage].nPackageId );
		if ( eResult != SIR_AUDIO_OK ) {
			m_nLastFailedPackageId = m_pEntries[iPackage].nPackageId;
			return eResult;
		}
		m_pEntries[iPackage].eState = 0;
		--m_nLoadedCount;
	}
	return SIR_AUDIO_OK;
}

sirAudioResult_t SIRFMODPackageLoader::loadPackage( uint64_t nPackageId, bool bPreloadSamples ) noexcept
{
	if ( !isReady() ) {
		return SIR_AUDIO_NOT_INITIALIZED;
	}
	uint32_t iRoot = 0;
	if ( !findIndex( nPackageId, iRoot ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( m_pBanks->getLoadedBankCount() != m_nLoadedCount ) {
		return SIR_AUDIO_BUSY;
	}

	uint32_t nPlanCount = 0;
	sirAudioResult_t eResult = buildPlan( iRoot, nPlanCount );
	if ( eResult != SIR_AUDIO_OK ) {
		return eResult;
	}
	for ( uint32_t iOrder = 0; iOrder < nPlanCount; ++iOrder ) {
		if ( m_pEntries[m_pPlan[iOrder]].eState == 2 ) {
			return SIR_AUDIO_BUSY;
		}
	}
	const uint32_t nPreviousLoaded = m_nLoadedCount;
	m_nLastFailedPackageId = 0;
	uint64_t nFailedPackageId = 0;
	for ( uint32_t iOrder = 0; iOrder < nPlanCount; ++iOrder ) {
		const uint32_t iPackage = m_pPlan[iOrder];
		sirFMODPackageEntry_t &entry = m_pEntries[iPackage];
		nFailedPackageId = entry.nPackageId;
		if ( entry.eState == 0 ) {
			sirAudioPackageRecord_t package = {};
			if ( !m_pManifest->getPackageAt( iPackage, package ) ) {
				eResult = SIR_AUDIO_INVALID_HANDLE;
				break;
			}
			const char *szPath = nullptr;
			eResult = m_Resolver.resolve( m_Resolver.pUserData, package.nPackageId, package.nProviderPackageKey, &szPath );
			if ( eResult != SIR_AUDIO_OK ) {
				break;
			}
			if ( szPath == nullptr || szPath[0] == '\0' ) {
				eResult = SIR_AUDIO_INVALID_ARGUMENT;
				break;
			}
			const uint32_t nBeforeBankLoad = m_pBanks->getLoadedBankCount();
			eResult = m_pBanks->loadBank( entry.nPackageId, szPath );
			if ( m_pBanks->getLoadedBankCount() != nBeforeBankLoad ) {
				entry.eState = eResult == SIR_AUDIO_OK ? 1 : 2;
				m_pLoadOrder[m_nLoadedCount++] = iPackage;
			}
			if ( eResult != SIR_AUDIO_OK ) {
				break;
			}
		}
		if ( bPreloadSamples ) {
			eResult = m_pBanks->preloadSamples( entry.nPackageId );
			if ( eResult != SIR_AUDIO_OK ) {
				break;
			}
		}
	}
	if ( eResult == SIR_AUDIO_OK ) {
		return SIR_AUDIO_OK;
	}
	m_nLastFailedPackageId = nFailedPackageId;
	const sirAudioResult_t eRollback = rollbackTo( nPreviousLoaded );
	return eRollback == SIR_AUDIO_OK ? eResult : eRollback;
}

sirAudioResult_t SIRFMODPackageLoader::unloadAll( void ) noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_AUDIO_OK;
	}
	if ( m_pBanks == nullptr || !m_pBanks->isReady() || m_pBanks->getLoadedBankCount() != m_nLoadedCount ) {
		return SIR_AUDIO_BUSY;
	}
	return rollbackTo( 0 );
}

sirAudioResult_t SIRFMODPackageLoader::shutdown( void ) noexcept
{
	if ( m_pEntries == nullptr ) {
		return SIR_AUDIO_OK;
	}
	const sirAudioResult_t eResult = unloadAll();
	if ( eResult != SIR_AUDIO_OK ) {
		return eResult;
	}
	releaseStorage();
	return SIR_AUDIO_OK;
}

uint32_t SIRFMODPackageLoader::getLoadedCount( void ) const noexcept
{
	return m_nLoadedCount;
}

uint64_t SIRFMODPackageLoader::getLastFailedPackageId( void ) const noexcept
{
	return m_nLastFailedPackageId;
}
