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

#include <Audio/SIRAudioResources.h>

#include <Core/Containers/SIRRawStorage.h>
#include <Core/Platform/SIRAtomic.h>
#include <cmath>

static SIRAtomic<uint64_t> s_nAudioManifestEpoch = SIRAtomic<uint64_t>( 1 );

static bool advanceAudioTable( size_t &nOffset, uint32_t nCount, size_t nRecordBytes, size_t nLimit ) noexcept
{
	size_t nTableBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( nCount, nRecordBytes, &nTableBytes ) || nOffset > nLimit || nTableBytes > nLimit - nOffset ) {
		return false;
	}
	nOffset += nTableBytes;
	return true;
}

static void readAudioPackage( const uint8_t *pBytes, size_t nOffset, uint32_t iIndex, sirAudioPackageRecord_t &record ) noexcept
{
	memcpy( &record, pBytes + nOffset + ( size_t )iIndex * sizeof( record ), sizeof( record ) );
}

static void readAudioEvent( const uint8_t *pBytes, size_t nOffset, uint32_t iIndex, sirAudioEventRecord_t &record ) noexcept
{
	memcpy( &record, pBytes + nOffset + ( size_t )iIndex * sizeof( record ), sizeof( record ) );
}

static void readAudioControl( const uint8_t *pBytes, size_t nOffset, uint32_t iIndex, sirAudioControlRecord_t &record ) noexcept
{
	memcpy( &record, pBytes + nOffset + ( size_t )iIndex * sizeof( record ), sizeof( record ) );
}

static uint64_t readAudioDependency( const uint8_t *pBytes, size_t nOffset, uint32_t iIndex ) noexcept
{
	uint64_t nDependency = 0;
	memcpy( &nDependency, pBytes + nOffset + ( size_t )iIndex * sizeof( nDependency ), sizeof( nDependency ) );
	return nDependency;
}

static bool findAudioPackageIndex( const uint8_t *pBytes, size_t nPackageOffset, uint32_t nPackageCount, uint64_t nPackageId,
                                   uint32_t &iFound ) noexcept
{
	uint32_t iFirst = 0;
	uint32_t iEnd = nPackageCount;
	while ( iFirst < iEnd ) {
		const uint32_t iMiddle = iFirst + ( iEnd - iFirst ) / 2;
		sirAudioPackageRecord_t package = {};
		readAudioPackage( pBytes, nPackageOffset, iMiddle, package );
		if ( package.nPackageId < nPackageId ) {
			iFirst = iMiddle + 1;
		} else {
			iEnd = iMiddle;
		}
	}
	if ( iFirst == nPackageCount ) {
		return false;
	}
	sirAudioPackageRecord_t package = {};
	readAudioPackage( pBytes, nPackageOffset, iFirst, package );
	if ( package.nPackageId != nPackageId ) {
		return false;
	}
	iFound = iFirst;
	return true;
}

static sirAudioManifestResult_t validateAudioPackages( const uint8_t *pBytes, const sirAudioManifestHeader_t &header, size_t nPackageOffset,
                                                       size_t nDependencyOffset, uint64_t nAvailableCapabilities ) noexcept
{
	uint64_t nPreviousId = 0;
	uint32_t nConsumedDependencies = 0;
	for ( uint32_t iPackage = 0; iPackage < header.nPackageCount; ++iPackage ) {
		sirAudioPackageRecord_t package = {};
		readAudioPackage( pBytes, nPackageOffset, iPackage, package );
		if ( package.nPackageId == 0 || package.nProviderPackageKey == 0 || package.iDependencyStart != nConsumedDependencies ||
		     package.nDependencyCount > header.nDependencyCount - nConsumedDependencies ) {
			return SIR_AUDIO_MANIFEST_INVALID_RECORD;
		}
		if ( iPackage != 0 && package.nPackageId <= nPreviousId ) {
			return SIR_AUDIO_MANIFEST_DUPLICATE_ID;
		}
		if ( ( package.nRequiredCapabilities & ~nAvailableCapabilities ) != 0 ) {
			return SIR_AUDIO_MANIFEST_MISSING_CAPABILITY;
		}
		nPreviousId = package.nPackageId;
		nConsumedDependencies += package.nDependencyCount;
	}
	if ( nConsumedDependencies != header.nDependencyCount ) {
		return SIR_AUDIO_MANIFEST_INVALID_RECORD;
	}
	for ( uint32_t iPackage = 0; iPackage < header.nPackageCount; ++iPackage ) {
		sirAudioPackageRecord_t package = {};
		readAudioPackage( pBytes, nPackageOffset, iPackage, package );
		uint64_t nPreviousDependency = 0;
		for ( uint32_t iDependency = 0; iDependency < package.nDependencyCount; ++iDependency ) {
			const uint64_t nDependencyId = readAudioDependency( pBytes, nDependencyOffset, package.iDependencyStart + iDependency );
			if ( nDependencyId == package.nPackageId || ( iDependency != 0 && nDependencyId <= nPreviousDependency ) ) {
				return SIR_AUDIO_MANIFEST_INVALID_RECORD;
			}
			uint32_t iResolved = 0;
			if ( !findAudioPackageIndex( pBytes, nPackageOffset, header.nPackageCount, nDependencyId, iResolved ) ) {
				return SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY;
			}
			nPreviousDependency = nDependencyId;
		}
	}
	return SIR_AUDIO_MANIFEST_OK;
}

typedef struct sirAudioDependencyFrame_s
{
	uint32_t iPackage;
	uint32_t iNextDependency;
} sirAudioDependencyFrame_t;

static sirAudioManifestResult_t validateAudioDependencyGraph( const uint8_t *pBytes, const sirAudioManifestHeader_t &header,
                                                              size_t nPackageOffset, size_t nDependencyOffset,
                                                              const sirContainerAllocator_t *pAllocator ) noexcept
{
	if ( header.nPackageCount < 2 || header.nDependencyCount == 0 ) {
		return SIR_AUDIO_MANIFEST_OK;
	}
	size_t nFrameBytes = 0;
	if ( !SIRRawStorage::checkedMultiply( header.nPackageCount, sizeof( sirAudioDependencyFrame_t ), &nFrameBytes ) ) {
		return SIR_AUDIO_MANIFEST_BAD_SCHEMA;
	}
	const size_t nStateBytes = ( size_t )header.nPackageCount;
	if ( nStateBytes > SIZE_MAX - nFrameBytes - alignof( sirAudioDependencyFrame_t ) ) {
		return SIR_AUDIO_MANIFEST_BAD_SCHEMA;
	}
	const size_t nAlignedStateBytes =
	    ( nStateBytes + alignof( sirAudioDependencyFrame_t ) - 1 ) & ~( size_t )( alignof( sirAudioDependencyFrame_t ) - 1 );
	void *pScratch = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nAlignedStateBytes + nFrameBytes, alignof( sirAudioDependencyFrame_t ), &pScratch ) !=
	     SIR_CONTAINER_OK ) {
		return SIR_AUDIO_MANIFEST_OUT_OF_MEMORY;
	}
	uint8_t *pStates = ( uint8_t * )pScratch;
	sirAudioDependencyFrame_t *pFrames = ( sirAudioDependencyFrame_t * )( pStates + nAlignedStateBytes );
	memset( pStates, 0, nStateBytes );
	sirAudioManifestResult_t eResult = SIR_AUDIO_MANIFEST_OK;
	for ( uint32_t iRoot = 0; iRoot < header.nPackageCount && eResult == SIR_AUDIO_MANIFEST_OK; ++iRoot ) {
		if ( pStates[iRoot] != 0 ) {
			continue;
		}
		pStates[iRoot] = 1;
		pFrames[0] = { iRoot, 0 };
		uint32_t nDepth = 1;
		while ( nDepth != 0 ) {
			sirAudioDependencyFrame_t &frame = pFrames[nDepth - 1];
			sirAudioPackageRecord_t package = {};
			readAudioPackage( pBytes, nPackageOffset, frame.iPackage, package );
			if ( frame.iNextDependency == package.nDependencyCount ) {
				pStates[frame.iPackage] = 2;
				--nDepth;
				continue;
			}
			const uint64_t nDependencyId =
			    readAudioDependency( pBytes, nDependencyOffset, package.iDependencyStart + frame.iNextDependency );
			++frame.iNextDependency;
			uint32_t iDependency = 0;
			if ( !findAudioPackageIndex( pBytes, nPackageOffset, header.nPackageCount, nDependencyId, iDependency ) ) {
				eResult = SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY;
				break;
			}
			if ( pStates[iDependency] == 1 ) {
				eResult = SIR_AUDIO_MANIFEST_CYCLIC_DEPENDENCY;
				break;
			}
			if ( pStates[iDependency] == 0 ) {
				pStates[iDependency] = 1;
				pFrames[nDepth] = { iDependency, 0 };
				++nDepth;
			}
		}
	}
	SIRRawStorage::deallocate( pAllocator, pScratch, nAlignedStateBytes + nFrameBytes, alignof( sirAudioDependencyFrame_t ) );
	return eResult;
}

static sirAudioManifestResult_t validateAudioEvents( const uint8_t *pBytes, const sirAudioManifestHeader_t &header, size_t nPackageOffset,
                                                     size_t nEventOffset, uint64_t nAvailableCapabilities ) noexcept
{
	uint64_t nPreviousId = 0;
	for ( uint32_t iEvent = 0; iEvent < header.nEventCount; ++iEvent ) {
		sirAudioEventRecord_t event = {};
		readAudioEvent( pBytes, nEventOffset, iEvent, event );
		if ( event.nEventId == 0 || event.nProviderEventKey == 0 ) {
			return SIR_AUDIO_MANIFEST_INVALID_RECORD;
		}
		if ( iEvent != 0 && event.nEventId <= nPreviousId ) {
			return SIR_AUDIO_MANIFEST_DUPLICATE_ID;
		}
		if ( ( event.nRequiredCapabilities & ~nAvailableCapabilities ) != 0 ) {
			return SIR_AUDIO_MANIFEST_MISSING_CAPABILITY;
		}
		uint32_t iPackage = 0;
		if ( !findAudioPackageIndex( pBytes, nPackageOffset, header.nPackageCount, event.nPackageId, iPackage ) ) {
			return SIR_AUDIO_MANIFEST_MISSING_DEPENDENCY;
		}
		nPreviousId = event.nEventId;
	}
	return SIR_AUDIO_MANIFEST_OK;
}

static sirAudioManifestResult_t validateAudioControls( const uint8_t *pBytes, const sirAudioManifestHeader_t &header, size_t nControlOffset,
                                                       uint64_t nAvailableCapabilities ) noexcept
{
	uint64_t nPreviousId = 0;
	for ( uint32_t iControl = 0; iControl < header.nControlCount; ++iControl ) {
		sirAudioControlRecord_t control = {};
		readAudioControl( pBytes, nControlOffset, iControl, control );
		if ( control.nControlId == 0 || control.nReserved != 0 || control.eScope > SIR_AUDIO_CONTROL_PLAYING ||
		     control.eType > SIR_AUDIO_CONTROL_DISCRETE || control.eBindingMode > SIR_AUDIO_BINDING_EMULATED ||
		     ( control.eBindingMode == SIR_AUDIO_BINDING_NATIVE && control.nProviderControlId == 0 ) ||
		     !std::isfinite( control.fDefault ) || !std::isfinite( control.fMinimum ) || !std::isfinite( control.fMaximum ) ||
		     control.fMinimum > control.fDefault || control.fDefault > control.fMaximum ) {
			return SIR_AUDIO_MANIFEST_INVALID_RECORD;
		}
		if ( iControl != 0 && control.nControlId <= nPreviousId ) {
			return SIR_AUDIO_MANIFEST_DUPLICATE_ID;
		}
		if ( ( control.nRequiredCapabilities & ~nAvailableCapabilities ) != 0 ) {
			return SIR_AUDIO_MANIFEST_MISSING_CAPABILITY;
		}
		if ( control.eType == SIR_AUDIO_CONTROL_BOOLEAN ) {
			if ( control.fMinimum < 0.0f || control.fMaximum > 1.0f || ( control.fDefault != 0.0f && control.fDefault != 1.0f ) ) {
				return SIR_AUDIO_MANIFEST_INVALID_RECORD;
			}
		} else if ( control.eType == SIR_AUDIO_CONTROL_DISCRETE ) {
			if ( std::trunc( control.fDefault ) != control.fDefault || std::trunc( control.fMinimum ) != control.fMinimum ||
			     std::trunc( control.fMaximum ) != control.fMaximum ) {
				return SIR_AUDIO_MANIFEST_INVALID_RECORD;
			}
		}
		nPreviousId = control.nControlId;
	}
	return SIR_AUDIO_MANIFEST_OK;
}

SIRAudioResourceStore::~SIRAudioResourceStore( void ) noexcept
{
	shutdown();
}

uint64_t SIRAudioResourceStore::hashPayload( const void *pBytes, size_t nBytes ) noexcept
{
	if ( pBytes == nullptr && nBytes != 0 ) {
		return 0;
	}
	const uint8_t *pData = ( const uint8_t * )pBytes;
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash ^= pData[iByte];
		nHash *= UINT64_C( 1099511628211 );
	}
	return nHash;
}

sirAudioManifestResult_t SIRAudioResourceStore::load( const void *pBytes, size_t nBytes, sirAudioProvider_t eExpectedProvider,
                                                      uint64_t nAvailableCapabilities, const sirContainerAllocator_t *pAllocator ) noexcept
{
	if ( pBytes == nullptr || ( pAllocator == nullptr || pAllocator->pfnAllocate == nullptr || pAllocator->pfnDeallocate == nullptr ) ||
	     nBytes < sizeof( sirAudioManifestHeader_t ) || nBytes > UINT32_MAX || eExpectedProvider <= SIR_AUDIO_PROVIDER_NONE ||
	     eExpectedProvider > SIR_AUDIO_PROVIDER_OPENAL ) {
		return SIR_AUDIO_MANIFEST_INVALID_ARGUMENT;
	}
	const uint8_t *pSource = ( const uint8_t * )pBytes;
	sirAudioManifestHeader_t header = {};
	memcpy( &header, pSource, sizeof( header ) );
	const uint16_t nHostEndianProbe = 1;
	if ( *( const uint8_t * )&nHostEndianProbe != 1 || header.nMagic != SIR_AUDIO_MANIFEST_MAGIC ||
	     header.nVersion != SIR_AUDIO_MANIFEST_VERSION || header.nHeaderBytes != sizeof( header ) ||
	     header.nEndianness != SIR_AUDIO_MANIFEST_LITTLE_ENDIAN || header.nFlags != 0 || header.nReserved != 0 ||
	     header.nTotalBytes != nBytes ) {
		return SIR_AUDIO_MANIFEST_BAD_SCHEMA;
	}
	if ( header.eProvider != eExpectedProvider ) {
		return SIR_AUDIO_MANIFEST_PROVIDER_MISMATCH;
	}
	if ( ( header.nRequiredCapabilities & ~nAvailableCapabilities ) != 0 ) {
		return SIR_AUDIO_MANIFEST_MISSING_CAPABILITY;
	}
	size_t nOffset = sizeof( header );
	const size_t nPackageOffset = nOffset;
	if ( !advanceAudioTable( nOffset, header.nPackageCount, sizeof( sirAudioPackageRecord_t ), nBytes ) ) {
		return SIR_AUDIO_MANIFEST_BAD_SCHEMA;
	}
	const size_t nEventOffset = nOffset;
	if ( !advanceAudioTable( nOffset, header.nEventCount, sizeof( sirAudioEventRecord_t ), nBytes ) ) {
		return SIR_AUDIO_MANIFEST_BAD_SCHEMA;
	}
	const size_t nControlOffset = nOffset;
	if ( !advanceAudioTable( nOffset, header.nControlCount, sizeof( sirAudioControlRecord_t ), nBytes ) ) {
		return SIR_AUDIO_MANIFEST_BAD_SCHEMA;
	}
	const size_t nDependencyOffset = nOffset;
	if ( !advanceAudioTable( nOffset, header.nDependencyCount, sizeof( uint64_t ), nBytes ) || nOffset != nBytes ) {
		return SIR_AUDIO_MANIFEST_BAD_SCHEMA;
	}
	if ( hashPayload( pSource + sizeof( header.nContentHash ), nBytes - sizeof( header.nContentHash ) ) != header.nContentHash ) {
		return SIR_AUDIO_MANIFEST_HASH_MISMATCH;
	}
	sirAudioManifestResult_t eResult = validateAudioPackages( pSource, header, nPackageOffset, nDependencyOffset, nAvailableCapabilities );
	if ( eResult != SIR_AUDIO_MANIFEST_OK ) {
		return eResult;
	}
	eResult = validateAudioDependencyGraph( pSource, header, nPackageOffset, nDependencyOffset, pAllocator );
	if ( eResult != SIR_AUDIO_MANIFEST_OK ) {
		return eResult;
	}
	eResult = validateAudioEvents( pSource, header, nPackageOffset, nEventOffset, nAvailableCapabilities );
	if ( eResult != SIR_AUDIO_MANIFEST_OK ) {
		return eResult;
	}
	eResult = validateAudioControls( pSource, header, nControlOffset, nAvailableCapabilities );
	if ( eResult != SIR_AUDIO_MANIFEST_OK ) {
		return eResult;
	}
	uint64_t nNewEpoch = s_nAudioManifestEpoch.load( SIR_MEMORY_ORDER_RELAXED );
	while ( nNewEpoch != 0 && nNewEpoch != UINT64_MAX ) {
		if ( s_nAudioManifestEpoch.compareExchangeWeak( nNewEpoch, nNewEpoch + 1, SIR_MEMORY_ORDER_RELAXED ) ) {
			break;
		}
	}
	if ( nNewEpoch == 0 || nNewEpoch == UINT64_MAX ) {
		return SIR_AUDIO_MANIFEST_EPOCH_EXHAUSTED;
	}
	void *pOwned = nullptr;
	if ( SIRRawStorage::tryAllocate( pAllocator, nBytes, alignof( sirAudioManifestHeader_t ), &pOwned ) != SIR_CONTAINER_OK ) {
		return SIR_AUDIO_MANIFEST_OUT_OF_MEMORY;
	}
	memcpy( pOwned, pBytes, nBytes );
	shutdown();
	m_pBytes = ( uint8_t * )pOwned;
	m_pAllocator = pAllocator;
	m_Header = header;
	m_nPackageOffset = nPackageOffset;
	m_nEventOffset = nEventOffset;
	m_nControlOffset = nControlOffset;
	m_nDependencyOffset = nDependencyOffset;
	m_nEpoch = nNewEpoch;
	return SIR_AUDIO_MANIFEST_OK;
}

void SIRAudioResourceStore::shutdown( void ) noexcept
{
	if ( m_pBytes != nullptr ) {
		SIRRawStorage::deallocate( m_pAllocator, m_pBytes, m_Header.nTotalBytes, alignof( sirAudioManifestHeader_t ) );
	}
	m_pBytes = nullptr;
	m_pAllocator = nullptr;
	m_Header = {};
	m_nEpoch = 0;
	m_nPackageOffset = 0;
	m_nEventOffset = 0;
	m_nControlOffset = 0;
	m_nDependencyOffset = 0;
}

bool SIRAudioResourceStore::isLoaded( void ) const noexcept
{
	return m_pBytes != nullptr;
}

uint64_t SIRAudioResourceStore::getEpoch( void ) const noexcept
{
	return m_nEpoch;
}

sirAudioProvider_t SIRAudioResourceStore::getProvider( void ) const noexcept
{
	return m_Header.eProvider;
}

uint64_t SIRAudioResourceStore::getBuildFingerprint( void ) const noexcept
{
	return m_Header.nBuildFingerprint;
}

uint32_t SIRAudioResourceStore::getPackageCount( void ) const noexcept
{
	return m_pBytes == nullptr ? 0 : m_Header.nPackageCount;
}

bool SIRAudioResourceStore::getPackageAt( uint32_t iPackage, sirAudioPackageRecord_t &package ) const noexcept
{
	if ( m_pBytes == nullptr || iPackage >= m_Header.nPackageCount ) {
		return false;
	}
	readAudioPackage( m_pBytes, m_nPackageOffset, iPackage, package );
	return true;
}

bool SIRAudioResourceStore::getPackageDependency( uint64_t nPackageId, uint32_t iDependency, uint64_t &nDependencyId ) const noexcept
{
	uint32_t iPackage = 0;
	if ( !findPackageIndex( nPackageId, iPackage ) ) {
		return false;
	}
	sirAudioPackageRecord_t package = {};
	readAudioPackage( m_pBytes, m_nPackageOffset, iPackage, package );
	if ( iDependency >= package.nDependencyCount ) {
		return false;
	}
	nDependencyId = readAudioDependency( m_pBytes, m_nDependencyOffset, package.iDependencyStart + iDependency );
	return true;
}

bool SIRAudioResourceStore::findPackageIndex( uint64_t nPackageId, uint32_t &iPackage ) const noexcept
{
	return m_pBytes != nullptr && findAudioPackageIndex( m_pBytes, m_nPackageOffset, m_Header.nPackageCount, nPackageId, iPackage );
}

bool SIRAudioResourceStore::findEventIndex( uint64_t nEventId, uint32_t &iEvent ) const noexcept
{
	if ( m_pBytes == nullptr ) {
		return false;
	}
	uint32_t iFirst = 0;
	uint32_t iEnd = m_Header.nEventCount;
	while ( iFirst < iEnd ) {
		const uint32_t iMiddle = iFirst + ( iEnd - iFirst ) / 2;
		sirAudioEventRecord_t event = {};
		readAudioEvent( m_pBytes, m_nEventOffset, iMiddle, event );
		if ( event.nEventId < nEventId ) {
			iFirst = iMiddle + 1;
		} else {
			iEnd = iMiddle;
		}
	}
	if ( iFirst == m_Header.nEventCount ) {
		return false;
	}
	sirAudioEventRecord_t event = {};
	readAudioEvent( m_pBytes, m_nEventOffset, iFirst, event );
	if ( event.nEventId != nEventId ) {
		return false;
	}
	iEvent = iFirst;
	return true;
}

bool SIRAudioResourceStore::findEvent( uint64_t nEventId, sirAudioEventRecord_t &event, sirAudioEventHandle_t *pHandle ) const noexcept
{
	uint32_t iEvent = 0;
	if ( !findEventIndex( nEventId, iEvent ) ) {
		return false;
	}
	readAudioEvent( m_pBytes, m_nEventOffset, iEvent, event );
	if ( pHandle != nullptr ) {
		*pHandle = { iEvent, 1, m_nEpoch };
	}
	return true;
}

bool SIRAudioResourceStore::resolveEvent( sirAudioEventHandle_t hEvent, sirAudioEventRecord_t &event ) const noexcept
{
	if ( m_pBytes == nullptr || hEvent.nEpoch != m_nEpoch || hEvent.nGeneration != 1 || hEvent.nSlot >= m_Header.nEventCount ) {
		return false;
	}
	readAudioEvent( m_pBytes, m_nEventOffset, hEvent.nSlot, event );
	return true;
}

bool SIRAudioResourceStore::findPackage( uint64_t nPackageId, sirAudioPackageRecord_t &package,
                                         sirAudioPackageHandle_t *pHandle ) const noexcept
{
	uint32_t iPackage = 0;
	if ( !findPackageIndex( nPackageId, iPackage ) ) {
		return false;
	}
	readAudioPackage( m_pBytes, m_nPackageOffset, iPackage, package );
	if ( pHandle != nullptr ) {
		*pHandle = { iPackage, 1, m_nEpoch };
	}
	return true;
}

bool SIRAudioResourceStore::resolvePackage( sirAudioPackageHandle_t hPackage, sirAudioPackageRecord_t &package ) const noexcept
{
	if ( m_pBytes == nullptr || hPackage.nEpoch != m_nEpoch || hPackage.nGeneration != 1 || hPackage.nSlot >= m_Header.nPackageCount ) {
		return false;
	}
	readAudioPackage( m_pBytes, m_nPackageOffset, hPackage.nSlot, package );
	return true;
}

bool SIRAudioResourceStore::findControl( uint64_t nControlId, sirAudioControlRecord_t &control ) const noexcept
{
	if ( m_pBytes == nullptr ) {
		return false;
	}
	uint32_t iFirst = 0;
	uint32_t iEnd = m_Header.nControlCount;
	while ( iFirst < iEnd ) {
		const uint32_t iMiddle = iFirst + ( iEnd - iFirst ) / 2;
		sirAudioControlRecord_t candidate = {};
		readAudioControl( m_pBytes, m_nControlOffset, iMiddle, candidate );
		if ( candidate.nControlId < nControlId ) {
			iFirst = iMiddle + 1;
		} else {
			iEnd = iMiddle;
		}
	}
	if ( iFirst == m_Header.nControlCount ) {
		return false;
	}
	readAudioControl( m_pBytes, m_nControlOffset, iFirst, control );
	return control.nControlId == nControlId;
}

sirAudioResult_t SIRAudioResourceStore::validateControl( uint64_t nControlId, sirAudioControlScope_t eScope, float fValue ) const noexcept
{
	sirAudioControlRecord_t control = {};
	if ( !findControl( nControlId, control ) ) {
		return SIR_AUDIO_INVALID_HANDLE;
	}
	if ( control.eScope != eScope || !std::isfinite( fValue ) || fValue < control.fMinimum || fValue > control.fMaximum ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( control.eBindingMode == SIR_AUDIO_BINDING_REJECT ) {
		return SIR_AUDIO_UNSUPPORTED;
	}
	if ( control.eType == SIR_AUDIO_CONTROL_BOOLEAN && fValue != 0.0f && fValue != 1.0f ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	if ( control.eType == SIR_AUDIO_CONTROL_DISCRETE && std::trunc( fValue ) != fValue ) {
		return SIR_AUDIO_INVALID_ARGUMENT;
	}
	return SIR_AUDIO_OK;
}
