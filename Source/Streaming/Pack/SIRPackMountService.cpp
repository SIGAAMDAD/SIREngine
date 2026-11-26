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

#include <Streaming/Pack/SIRPackMountService.h>

#include <string.h>

struct SIRPackMountService::MountRecord
{
	SIRPack *pPackage;
	sirVFSMountHandle_t handle;
	uint32_t nLeases;
	bool bMounted;
};

SIRPackSourceLease::SIRPackSourceLease( void ) noexcept
    : m_pOwner( nullptr ),
      m_pPack( nullptr ),
      m_Lease{},
      m_NameSpace{},
      m_Source{},
      m_bHasChunk( false )
{
}

SIRPackSourceLease::~SIRPackSourceLease( void ) noexcept
{
	reset();
}

SIRPackSourceLease::SIRPackSourceLease( SIRPackSourceLease&& other ) noexcept
    : m_pOwner( other.m_pOwner ),
      m_pPack( other.m_pPack ),
      m_Lease( other.m_Lease ),
      m_Source( other.m_Source ),
      m_bHasChunk( other.m_bHasChunk )
{
	other.m_pOwner = nullptr;
	other.m_pPack = nullptr;
	other.m_Lease = {};
	other.m_NameSpace = {};
	other.m_bHasChunk = false;
}

SIRPackSourceLease& SIRPackSourceLease::operator=( SIRPackSourceLease&& other ) noexcept
{
	if ( this != &other ) {
		reset();
		m_pOwner = other.m_pOwner;
		m_pPack = other.m_pPack;
		m_Lease = other.m_Lease;
		m_NameSpace = other.m_NameSpace;
		m_Source = other.m_Source;
		m_bHasChunk = other.m_bHasChunk;
		other.m_pOwner = nullptr;
		other.m_pPack = nullptr;
		other.m_Lease = {};
		other.m_NameSpace = {};
		other.m_bHasChunk = false;
	}
	return *this;
}

bool SIRPackSourceLease::isValid( void ) const noexcept
{
	return m_pOwner != nullptr && m_pPack != nullptr && m_Lease.nToken != 0;
}

bool SIRPackSourceLease::hasChunk( void ) const noexcept
{
	return isValid() && m_bHasChunk;
}

const SIRPack *SIRPackSourceLease::getPack( void ) const noexcept
{
	return isValid() ? m_pPack : nullptr;
}

const sirPackChunkSource_t& SIRPackSourceLease::getSource( void ) const noexcept
{
	return m_Source;
}

void SIRPackSourceLease::reset( void ) noexcept
{
	if ( m_pOwner != nullptr )
		m_pOwner->releaseLease( *this );
	m_pOwner = nullptr;
	m_pPack = nullptr;
	m_Lease = {};
	m_Source = {};
	m_NameSpace = {};
	m_bHasChunk = false;
}

SIRPackMountService::SIRPackMountService( void ) noexcept
    : m_pVfs( nullptr ),
      m_pCodecRegistry( nullptr ),
      m_pRecords( nullptr ),
      m_nCapacity( 0 ),
      m_bInitialized( false )
{
}

SIRPackMountService::~SIRPackMountService( void ) noexcept
{
	shutdown();
}

uint64_t SIRPackMountService::requiredBytes( uint32_t nPackageCapacity ) noexcept
{
	return static_cast<uint64_t>( nPackageCapacity ) * sizeof( MountRecord );
}

sirPackRuntimeResult_t SIRPackMountService::initialize( const sirPackMountConfig_t& config ) noexcept
{
	if ( m_bInitialized )
		return SIR_PACK_RUNTIME_BUSY;
	if ( config.pVfs == nullptr || config.pPackageStorage == nullptr || config.pCodecRegistry == nullptr || config.pRecordStorage == nullptr || config.nPackageCapacity == 0 ||
	     config.nRecordStorageBytes < requiredBytes( config.nPackageCapacity ) )
		return SIR_PACK_RUNTIME_INVALID_ARGUMENT;
	m_pVfs = config.pVfs;
	m_pCodecRegistry = config.pCodecRegistry;
	m_pRecords = static_cast<MountRecord *>( config.pRecordStorage );
	for ( uint32_t i = 0; i < config.nPackageCapacity; ++i )
		m_pRecords[i] = {};
	m_nCapacity = config.nPackageCapacity;
	m_bInitialized = true;
	return SIR_PACK_RUNTIME_OK;
}

sirPackRuntimeResult_t SIRPackMountService::mount( const sirPackMountDesc_t& desc, sirVFSMountHandle_t *pHandle ) noexcept
{
	if ( !m_bInitialized || pHandle == nullptr || desc.pPackage == nullptr )
		return SIR_PACK_RUNTIME_INVALID_ARGUMENT;
	uint32_t iFree = m_nCapacity;
	for ( uint32_t i = 0; i < m_nCapacity; ++i )
		if ( m_pRecords[i].pPackage == nullptr ) {
			iFree = i;
			break;
		}
	if ( iFree == m_nCapacity )
		return SIR_PACK_RUNTIME_OUT_OF_CAPACITY;
	if ( desc.pPackage->open( desc.package ) != SIRPackResult::SIR_PACK_OK )
		return SIR_PACK_RUNTIME_IO_ERROR;
	for ( uint32_t i = 0; i < desc.pPackage->getCatalog().getChunkCount(); ++i ) {
		sirPackChunk_t chunk{};
		if ( desc.pPackage->getCatalog().chunkAt( i, &chunk ) != SIRPackResult::SIR_PACK_OK ||
			 ( chunk.eCodec != SIR_PACK_CODEC_NONE && m_pCodecRegistry->find( chunk.eCodec ) == nullptr ) ) {
			desc.pPackage->close();
			return SIR_PACK_RUNTIME_FORBIDDEN;
		}
	}
	sirVFSMountDescriptor_t vfs = desc.vfs;
	vfs.nSourceId = desc.pPackage->getPackageId();
	vfs.nSourceRevision = desc.pPackage->getGeneration();
	vfs.eKind = SIRVFSBackendKind::SIR_VFS_BACKEND_SIRPACK;
	if ( m_pVfs->mount( vfs, pHandle ) != SIRVFSResult::SIR_VFS_OK ) {
		desc.pPackage->close();
		return SIR_PACK_RUNTIME_FORBIDDEN;
	}
	m_pRecords[iFree] = { desc.pPackage, *pHandle, 0, true };
	return SIR_PACK_RUNTIME_OK;
}

sirPackRuntimeResult_t SIRPackMountService::unmount( sirVFSMountHandle_t handle ) noexcept
{
	if ( !m_bInitialized )
		return SIR_PACK_RUNTIME_NOT_READY;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) {
		MountRecord& record = m_pRecords[i];
		if ( record.pPackage != nullptr && record.handle.nSlot == handle.nSlot && record.handle.nGeneration == handle.nGeneration ) {
			if ( m_pVfs->unmount( handle ) != SIRVFSResult::SIR_VFS_OK )
				return SIR_PACK_RUNTIME_STALE_SOURCE;
			record.bMounted = false;
			if ( record.nLeases == 0 ) {
				record.pPackage->close();
				record = {};
			}
			return SIR_PACK_RUNTIME_OK;
		}
	}
	return SIR_PACK_RUNTIME_STALE_SOURCE;
}

static SIRPackMountService::MountRecord *sirPackFindRecord( SIRPackMountService::MountRecord *pRecords, uint32_t nCapacity, uint64_t nSourceId ) noexcept
{
	for ( uint32_t i = 0; i < nCapacity; ++i ) {
		if ( pRecords[i].pPackage != nullptr && pRecords[i].pPackage->getPackageId() == nSourceId ) {
			return &pRecords[i];
		}
	}
	return nullptr;
}

sirPackRuntimeResult_t SIRPackMountService::resolveResource( const sirPackLookup_t& lookup, sirPackResource_t *pResource, SIRPackSourceLease *pLease ) noexcept
{
	if ( !m_bInitialized || pResource == nullptr || pLease == nullptr ) {
		return SIR_PACK_RUNTIME_INVALID_ARGUMENT;
	}

	pLease->reset();
	sirVFSCandidateList_t candidates{};
	if ( m_pVfs->acquireCandidates( lookup.nameSpace, static_cast<uint32_t>( SIRVFSAccess::SIR_VFS_ACCESS_READ ), &candidates ) != SIRVFSResult::SIR_VFS_OK ) {
		return SIR_PACK_RUNTIME_NOT_FOUND;
	}

	for ( uint32_t i = 0; i < candidates.nCount; ++i ) {
		MountRecord *pRecord = sirPackFindRecord( m_pRecords, m_nCapacity, candidates.szCandidates[i].descriptor.nSourceId );
		sirPackResource_t resource{};
		if ( pRecord != nullptr && pRecord->pPackage->findResource( lookup.key, &resource ) == SIRPackResult::SIR_PACK_OK ) {
			for ( uint32_t j = i + 1; j < candidates.nCount; ++j ) {
				m_pVfs->release( candidates.szCandidates[j].lease );
			}
			pRecord->nLeases++;
			pLease->m_pOwner = this;
			pLease->m_pPack = pRecord->pPackage;
			pLease->m_Lease = candidates.szCandidates[i].lease;
			pLease->m_Source.source.mount = { pLease->m_Lease.nSlot, pLease->m_Lease.nGeneration };
			pLease->m_Source.source.nPackageId = pRecord->pPackage->getPackageId();
			pLease->m_Source.source.nPackageGeneration = pRecord->pPackage->getGeneration();
			pLease->m_NameSpace = lookup.nameSpace;
			pLease->m_Source.key = lookup.key;
			*pResource = resource;
			return SIR_PACK_RUNTIME_OK;
		}
		m_pVfs->release( candidates.szCandidates[i].lease );
	}
	return SIR_PACK_RUNTIME_NOT_FOUND;
}

sirPackRuntimeResult_t SIRPackMountService::resolveChunk( const sirPackLookup_t& lookup, uint32_t nChunkId, SIRPackSourceLease *pLease ) noexcept
{
	sirPackResource_t resource{};
	sirPackRuntimeResult_t result = resolveResource( lookup, &resource, pLease );
	if ( result != SIR_PACK_RUNTIME_OK )
		return result;
	sirPackChunkSource_t source{};
	if ( pLease->m_pPack->resolveChunk( lookup.key, nChunkId, &source ) != SIRPackResult::SIR_PACK_OK ) {
		pLease->reset();
		return SIR_PACK_RUNTIME_NOT_FOUND;
	}
	source.source.mount = { pLease->m_Lease.nSlot, pLease->m_Lease.nGeneration };
	pLease->m_Source = source;
	pLease->m_bHasChunk = true;
	return SIR_PACK_RUNTIME_OK;
}

bool SIRPackMountService::isCurrent( const SIRPackSourceLease& lease ) const noexcept
{
	if ( !lease.isValid() )
		return false;
	sirVFSCandidateList_t candidates{};
	if ( m_pVfs->acquireCandidates( lease.m_NameSpace, static_cast<uint32_t>( SIRVFSAccess::SIR_VFS_ACCESS_READ ), &candidates ) != SIRVFSResult::SIR_VFS_OK )
		return false;
	bool bCurrent = false;
	for ( uint32_t i = 0; i < candidates.nCount; ++i ) {
		if ( candidates.szCandidates[i].lease.nSlot == lease.m_Lease.nSlot && candidates.szCandidates[i].lease.nGeneration == lease.m_Lease.nGeneration )
			bCurrent = i == 0;
		m_pVfs->release( candidates.szCandidates[i].lease );
	}
	return bCurrent;
}

void SIRPackMountService::releaseLease( SIRPackSourceLease& lease ) noexcept
{
	if ( lease.m_pOwner != this )
		return;
	m_pVfs->release( lease.m_Lease );
	for ( uint32_t i = 0; i < m_nCapacity; ++i )
		if ( m_pRecords[i].pPackage == lease.m_pPack ) {
			MountRecord& record = m_pRecords[i];
			if ( record.nLeases != 0 )
				--record.nLeases;
			if ( !record.bMounted && record.nLeases == 0 ) {
				record.pPackage->close();
				record = {};
			}
			break;
		}
}

uint32_t SIRPackMountService::collectRetired( uint32_t nMaximum ) noexcept
{
	uint32_t nCollected = 0;
	for ( uint32_t i = 0; i < m_nCapacity && nCollected < nMaximum; ++i ) {
		if ( m_pRecords[i].pPackage != nullptr && !m_pRecords[i].bMounted && m_pRecords[i].nLeases == 0 ) {
			m_pRecords[i] = {};
			++nCollected;
		}
	}
	return nCollected;
}

sirPackRuntimeResult_t SIRPackMountService::shutdown( void ) noexcept
{
	if ( !m_bInitialized )
		return SIR_PACK_RUNTIME_OK;
	for ( uint32_t i = 0; i < m_nCapacity; ++i )
		if ( m_pRecords[i].pPackage != nullptr && ( m_pRecords[i].bMounted || m_pRecords[i].nLeases != 0 ) )
			return SIR_PACK_RUNTIME_BUSY;
	m_pRecords = nullptr;
	m_nCapacity = 0;
	m_pVfs = nullptr;
	m_pCodecRegistry = nullptr;
	m_bInitialized = false;
	return SIR_PACK_RUNTIME_OK;
}
