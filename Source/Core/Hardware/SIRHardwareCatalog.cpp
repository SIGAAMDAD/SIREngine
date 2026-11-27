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

#include <Core/Hardware/SIRHardwareCatalog.h>
#include <Core/Platform/Shared/Threading/SIRScopedLock.h>

SIRHardwareResult SIRHardwareCatalog::initialize( void ) noexcept
{
	if ( m_Snapshot.nGeneration != 0 ) {
		return SIRHardwareResult::SIR_HARDWARE_INVALID;
	}
	return discover();
}

SIRHardwareResult SIRHardwareCatalog::refresh( void ) noexcept
{
	if ( m_Snapshot.nGeneration == 0 ) {
		return SIRHardwareResult::SIR_HARDWARE_INVALID;
	}
	return discover();
}

SIRHardwareResult SIRHardwareCatalog::copySnapshot( sirHardwareSnapshot_t *pSnapshot ) const noexcept
{
	if ( pSnapshot == nullptr ) {
		return SIRHardwareResult::SIR_HARDWARE_INVALID;
	}
	const SIRScopedLock<SIRSpinLock> lock( m_MetadataLock );
	*pSnapshot = m_Snapshot;
	return m_eSnapshotResult;
}

SIRHardwareResult SIRHardwareCatalog::queryStorage( const sirStorageQuery_t& rQuery, sirStorageDescriptor_t *pDescriptor ) const noexcept
{
	if ( pDescriptor == nullptr ) {
		return SIRHardwareResult::SIR_HARDWARE_INVALID;
	}
	*pDescriptor = {};
	const SIRHardwareResult eBefore = validateGeneration( rQuery.nGeneration );
	if ( eBefore != SIRHardwareResult::SIR_HARDWARE_OK ) {
		return eBefore;
	}

	sirPlatformStorageObservation_t volume{};
	const SIRHardwareResult eStorage = storageResult( SIRPlatformStorageQuery::queryPath( rQuery.pszUtf8Path, &volume ) );
	const SIRHardwareResult eAfter = validateGeneration( rQuery.nGeneration );
	if ( eAfter != SIRHardwareResult::SIR_HARDWARE_OK ) {
		return eAfter;
	}

	pDescriptor->volume = volume;
	if ( eStorage == SIRHardwareResult::SIR_HARDWARE_PARTIAL ) {
		pDescriptor->nGeneration = rQuery.nGeneration;
	}
	return eStorage;
}

void SIRHardwareCatalog::shutdown( void ) noexcept
{
	const SIRScopedLock<SIRSpinLock> lock( m_MetadataLock );
	m_Snapshot = {};
	m_eSnapshotResult = SIRHardwareResult::SIR_HARDWARE_INVALID;
}

SIRHardwareResult SIRHardwareCatalog::discover( void ) noexcept
{
	if ( m_nLastGeneration == UINT32_MAX ) {
		const SIRScopedLock<SIRSpinLock> lock( m_MetadataLock );
		if ( m_Snapshot.nGeneration != 0 ) {
			m_eSnapshotResult = SIRHardwareResult::SIR_HARDWARE_STALE;
		}
		return SIRHardwareResult::SIR_HARDWARE_OUT_OF_CAPACITY;
	}

	sirHardwareSnapshot_t next{};
	readCPU( &next.cpu );
	const bool bSystem = SIRPlatformMemory::querySystem( &next.memory.system );
	const bool bProcess = SIRPlatformMemory::queryProcess( &next.memory.process );
	if ( bSystem ) {
		next.memory.nValidFields = static_cast<uint32_t>( SIRMemoryDescriptorField::SIR_MEMORY_DESCRIPTOR_TOTAL_PHYSICAL ) |
		                           static_cast<uint32_t>( SIRMemoryDescriptorField::SIR_MEMORY_DESCRIPTOR_AVAILABLE_PHYSICAL );
	}

	const SIRScopedLock<SIRSpinLock> lock( m_MetadataLock );
	const bool bCPU = next.cpu.nValidFields != 0;
	if ( !bCPU && !bSystem && !bProcess ) {
		if ( m_Snapshot.nGeneration != 0 ) {
			m_eSnapshotResult = SIRHardwareResult::SIR_HARDWARE_STALE;
		}
		return SIRHardwareResult::SIR_HARDWARE_OS_ERROR;
	}

	next.nGeneration = ++m_nLastGeneration;
	m_Snapshot = next;
	m_eSnapshotResult = bCPU && bSystem && bProcess ? SIRHardwareResult::SIR_HARDWARE_OK : SIRHardwareResult::SIR_HARDWARE_PARTIAL;
	return m_eSnapshotResult;
}

SIRHardwareResult SIRHardwareCatalog::validateGeneration( uint32_t nGeneration ) const noexcept
{
	const SIRScopedLock<SIRSpinLock> lock( m_MetadataLock );
	if ( nGeneration == 0 ) {
		return SIRHardwareResult::SIR_HARDWARE_INVALID;
	}
	if ( nGeneration != m_Snapshot.nGeneration || m_eSnapshotResult == SIRHardwareResult::SIR_HARDWARE_STALE ) {
		return SIRHardwareResult::SIR_HARDWARE_STALE;
	}
	return SIRHardwareResult::SIR_HARDWARE_OK;
}

void SIRHardwareCatalog::readCPU( sirCPUDescriptor_t *pCPU ) noexcept
{
	if ( !SIRCPUInfo::query( &pCPU->info ) ) {
		return;
	}
	const sirCPUInfo_t& rInfo = pCPU->info;
	uint32_t nFields = static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_LOGICAL_COUNT );
	if ( rInfo.nPhysicalCoreCount != 0 ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_PHYSICAL_COUNT );
	}
	if ( rInfo.szVendor[0] != '\0' ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_VENDOR );
	}
	if ( rInfo.szBrand[0] != '\0' ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_BRAND );
	}
	// A zero raw feature mask does not distinguish an unprobed architecture from no usable features.
	if ( rInfo.nFeatures != 0 ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_FEATURES );
	}
	if ( rInfo.nCacheLineBytes != 0 ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_CACHE_LINE );
	}
	if ( rInfo.nL1DataCacheBytes != 0 ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_L1_DATA_CACHE );
	}
	if ( rInfo.nL2CacheBytes != 0 ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_L2_CACHE );
	}
	if ( rInfo.nL3CacheBytes != 0 ) {
		nFields |= static_cast<uint32_t>( SIRCPUDescriptorField::SIR_CPU_DESCRIPTOR_L3_CACHE );
	}
	pCPU->nValidFields = nFields;
}

SIRHardwareResult SIRHardwareCatalog::storageResult( sirPlatformStorageResult eResult ) noexcept
{
	switch ( eResult ) {
	case sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OK:
	case sirPlatformStorageResult::SIR_PLATFORM_STORAGE_PARTIAL:
		return SIRHardwareResult::SIR_HARDWARE_PARTIAL;
	case sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNKNOWN:
		return SIRHardwareResult::SIR_HARDWARE_UNKNOWN;
	case sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNSUPPORTED:
		return SIRHardwareResult::SIR_HARDWARE_UNSUPPORTED;
	case sirPlatformStorageResult::SIR_PLATFORM_STORAGE_ACCESS_DENIED:
		return SIRHardwareResult::SIR_HARDWARE_ACCESS_DENIED;
	case sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID:
		return SIRHardwareResult::SIR_HARDWARE_INVALID;
	case sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OS_ERROR:
		return SIRHardwareResult::SIR_HARDWARE_OS_ERROR;
	}
	return SIRHardwareResult::SIR_HARDWARE_OS_ERROR;
}
