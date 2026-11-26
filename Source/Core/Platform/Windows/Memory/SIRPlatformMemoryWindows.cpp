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

#include <Core/Platform/Shared/Memory/SIRPlatformMemory.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <jobapi2.h>
#include <psapi.h>

bool SIRPlatformMemory::querySystem( sirSystemMemoryStats_t *pStats ) noexcept
{
	if ( pStats == nullptr ) {
		return false;
	}
	MEMORYSTATUSEX memory {};
	memory.dwLength = sizeof( memory );
	if ( !GlobalMemoryStatusEx( &memory ) ) {
		return false;
	}
	sirSystemMemoryStats_t stats {};
	stats.nTotalPhysicalBytes = memory.ullTotalPhys;
	stats.nAvailablePhysicalBytes = memory.ullAvailPhys;
	PERFORMANCE_INFORMATION perf {};
	perf.cb = sizeof( perf );
	if ( GetPerformanceInfo( &perf, sizeof( perf ) ) ) {
		const uint64_t nPage = static_cast<uint64_t>(perf.PageSize);
		stats.nCommitLimitBytes = static_cast<uint64_t>(perf.CommitLimit) * nPage;
		stats.nCommittedBytes = static_cast<uint64_t>(perf.CommitTotal) * nPage;
		stats.nValidFields |= sirSystemMemoryField::SIR_SYSTEM_MEMORY_COMMIT;
	}
	BOOL bInJob = FALSE;
	if ( IsProcessInJob( GetCurrentProcess(), nullptr, &bInJob ) && bInJob ) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION job {};
		if ( QueryInformationJobObject( nullptr, JobObjectExtendedLimitInformation, &job, sizeof( job ), nullptr ) ) {
			const DWORD nFlags = job.BasicLimitInformation.LimitFlags;
			if ( nFlags & JOB_OBJECT_LIMIT_JOB_MEMORY ) {
				stats.nGroupMemoryLimitBytes = static_cast<uint64_t>(job.JobMemoryLimit);
				stats.nValidFields |= sirSystemMemoryField::SIR_SYSTEM_MEMORY_GROUP_LIMIT;
			}
		}
	}
	*pStats = stats;
	return true;
}

bool SIRPlatformMemory::queryProcess( sirProcessMemoryStats_t *pStats ) noexcept
{
	if ( pStats == nullptr ) {
		return false;
	}
	PROCESS_MEMORY_COUNTERS_EX counters {};
	if ( !GetProcessMemoryInfo( GetCurrentProcess(), static_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof( counters ) ) ) {
		return false;
	}
	sirProcessMemoryStats_t stats {};
	stats.nResidentBytes = static_cast<uint64_t>(counters.WorkingSetSize);
	stats.nPeakResidentBytes = static_cast<uint64_t>(counters.PeakWorkingSetSize);
	stats.nPrivateCommittedBytes = static_cast<uint64_t>(counters.PrivateUsage);
	stats.nPageFaultCount = static_cast<uint64_t>(counters.PageFaultCount);
	stats.nValidFields = sirProcessMemoryField::SIR_PROCESS_MEMORY_RESIDENT | sirProcessMemoryField::SIR_PROCESS_MEMORY_PEAK_RESIDENT | sirProcessMemoryField::SIR_PROCESS_MEMORY_PRIVATE_COMMITTED |
	                     sirProcessMemoryField::SIR_PROCESS_MEMORY_PAGE_FAULTS;
	*pStats = stats;
	return true;
}
