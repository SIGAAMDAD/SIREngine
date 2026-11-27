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

#ifndef __SIR_HARDWARECATALOG_H__
#define __SIR_HARDWARECATALOG_H__

#include <Core/SIRPCH.h>
#include <Core/Hardware/SIRHardwareTypes.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

// One designated owner calls initialize, refresh, and shutdown. Concurrent
// readers receive copies. No operating-system query runs under the metadata lock.
class SIRHardwareCatalog final
{
public:
	explicit SIRHardwareCatalog( void ) noexcept = default;
	explicit SIRHardwareCatalog( const SIRHardwareCatalog& ) = delete;
	explicit SIRHardwareCatalog( SIRHardwareCatalog&& ) = delete;
	~SIRHardwareCatalog() noexcept = default;

	SIRHardwareCatalog& operator=( const SIRHardwareCatalog& ) = delete;
	SIRHardwareCatalog& operator=( SIRHardwareCatalog&& ) = delete;

	[[nodiscard]] SIRHardwareResult initialize( void ) noexcept;
	[[nodiscard]] SIRHardwareResult refresh( void ) noexcept;
	// If refresh fails, copies the previous generation and returns STALE.
	[[nodiscard]] SIRHardwareResult copySnapshot( sirHardwareSnapshot_t *pSnapshot ) const noexcept;
	// Cold, uncached advisory lookup. The requested generation must still be current on return.
	[[nodiscard]] SIRHardwareResult queryStorage( const sirStorageQuery_t& rQuery, sirStorageDescriptor_t *pDescriptor ) const noexcept;
	void shutdown( void ) noexcept;

private:
	[[nodiscard]] SIRHardwareResult discover( void ) noexcept;
	[[nodiscard]] SIRHardwareResult validateGeneration( uint32_t nGeneration ) const noexcept;
	static void readCPU( sirCPUDescriptor_t *pCPU ) noexcept;
	static SIRHardwareResult storageResult( sirPlatformStorageResult eResult ) noexcept;

	mutable SIRSpinLock m_MetadataLock;
	sirHardwareSnapshot_t m_Snapshot{};
	uint32_t m_nLastGeneration{};
	SIRHardwareResult m_eSnapshotResult{ SIRHardwareResult::SIR_HARDWARE_INVALID };
};

#endif // __SIR_HARDWARECATALOG_H__
