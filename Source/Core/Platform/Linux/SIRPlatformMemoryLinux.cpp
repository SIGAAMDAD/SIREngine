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

#include <Core/Platform/SIRPlatformMemory.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <unistd.h>

static bool readFile( const char *szPath, char *szBuffer, size_t nCapacity ) noexcept
{
	if ( szPath == nullptr || szBuffer == nullptr || nCapacity < 2 ) {
		return false;
	}
	const int nFile = open( szPath, O_RDONLY | O_CLOEXEC );
	if ( nFile < 0 ) {
		return false;
	}
	const ssize_t nRead = read( nFile, szBuffer, nCapacity - 1 );
	const int nClose = close( nFile );
	if ( nRead <= 0 || nClose != 0 ) {
		return false;
	}
	szBuffer[nRead] = '\0';
	return true;
}

static bool parseKibField( const char *szData, const char *szKey, uint64_t *pBytes ) noexcept
{
	if ( szData == nullptr || szKey == nullptr || pBytes == nullptr ) {
		return false;
	}
	const size_t nKeyLength = strlen( szKey );
	for ( const char *pLine = szData; *pLine != '\0'; ) {
		if ( strncmp( pLine, szKey, nKeyLength ) == 0 ) {
			const char *pValue = pLine + nKeyLength;
			while ( *pValue == ' ' || *pValue == '\t' ) {
				++pValue;
			}
			errno = 0;
			char *pEnd = nullptr;
			const unsigned long long nKib = strtoull( pValue, &pEnd, 10 );
			if ( errno != 0 || pEnd == pValue || nKib > UINT64_MAX / 1024U ) {
				return false;
			}
			while ( *pEnd == ' ' || *pEnd == '\t' ) {
				++pEnd;
			}
			if ( pEnd[0] != 'k' || pEnd[1] != 'B' ) {
				return false;
			}
			*pBytes = (uint64_t)nKib * 1024U;
			return true;
		}
		const char *pEnd = strchr( pLine, '\n' );
		if ( pEnd == nullptr ) {
			break;
		}
		pLine = pEnd + 1;
	}
	return false;
}

static bool readUnsignedFile( const char *szPath, uint64_t *pNumber ) noexcept
{
	char szBuffer[64];
	if ( !readFile( szPath, szBuffer, sizeof( szBuffer ) ) || szBuffer[0] == 'm' ) {
		return false;
	}
	errno = 0;
	char *pEnd = nullptr;
	const unsigned long long nValue = strtoull( szBuffer, &pEnd, 10 );
	if ( pEnd == szBuffer || errno != 0 ) {
		return false;
	}
	while ( *pEnd == '\n' || *pEnd == ' ' || *pEnd == '\t' ) {
		++pEnd;
	}
	if ( *pEnd != '\0' ) {
		return false;
	}
	*pNumber = (uint64_t)nValue;
	return true;
}

bool SIRPlatformMemory::querySystem( sirSystemMemoryStats_t *pStats ) noexcept
{
	if ( pStats == nullptr ) {
		return false;
	}
	struct sysinfo system = {};
	char szMeminfo[16384];
	if ( sysinfo( &system ) != 0 || !readFile( "/proc/meminfo", szMeminfo, sizeof( szMeminfo ) ) ) {
		return false;
	}

	sirSystemMemoryStats_t stats = {};
	stats.nTotalPhysicalBytes = (uint64_t)system.totalram * system.mem_unit;
	stats.nAvailablePhysicalBytes = (uint64_t)system.freeram * system.mem_unit;
	stats.nTotalSwapBytes = (uint64_t)system.totalswap * system.mem_unit;
	stats.nAvailableSwapBytes = (uint64_t)system.freeswap * system.mem_unit;
	stats.nValidFields |= SIR_SYSTEM_MEMORY_SWAP;
	uint64_t nAvailable = 0;
	if ( parseKibField( szMeminfo, "MemAvailable:", &nAvailable ) ) {
		stats.nAvailablePhysicalBytes = nAvailable;
	}
	if ( parseKibField( szMeminfo, "CommitLimit:", &stats.nCommitLimitBytes ) &&
		parseKibField( szMeminfo, "Committed_AS:", &stats.nCommittedBytes ) ) {
		stats.nValidFields |= SIR_SYSTEM_MEMORY_COMMIT;
	}

	// The common /sys/fs/cgroup root is trustworthy for the process only when
	// the cgroup namespace presents the process at its root. More complex
	// mount mappings are deliberately reported as unavailable.
	char szCgroup[1024];
	if ( readFile( "/proc/self/cgroup", szCgroup, sizeof( szCgroup ) ) &&
		strncmp( szCgroup, "0::/", 4 ) == 0 && ( szCgroup[4] == '\n' || szCgroup[4] == '\0' ) ) {
		uint64_t nGroupLimit = 0;
		if ( readUnsignedFile( "/sys/fs/cgroup/memory.max", &nGroupLimit ) && nGroupLimit != 0 &&
			nGroupLimit != UINT64_MAX ) {
			stats.nGroupMemoryLimitBytes = nGroupLimit;
			stats.nValidFields |= SIR_SYSTEM_MEMORY_GROUP_LIMIT;
		}
		uint64_t nGroupUsage = 0;
		if ( readUnsignedFile( "/sys/fs/cgroup/memory.current", &nGroupUsage ) ) {
			stats.nGroupMemoryUsageBytes = nGroupUsage;
			stats.nValidFields |= SIR_SYSTEM_MEMORY_GROUP_USAGE;
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
	char szStatus[16384];
	if ( !readFile( "/proc/self/status", szStatus, sizeof( szStatus ) ) ) {
		return false;
	}
	sirProcessMemoryStats_t stats = {};
	if ( parseKibField( szStatus, "VmRSS:", &stats.nResidentBytes ) ) {
		stats.nValidFields |= SIR_PROCESS_MEMORY_RESIDENT;
	}
	if ( parseKibField( szStatus, "VmHWM:", &stats.nPeakResidentBytes ) ) {
		stats.nValidFields |= SIR_PROCESS_MEMORY_PEAK_RESIDENT;
	}
	if ( parseKibField( szStatus, "VmSize:", &stats.nVirtualAddressBytes ) ) {
		stats.nValidFields |= SIR_PROCESS_MEMORY_VIRTUAL_ADDRESS;
	}
	if ( parseKibField( szStatus, "RssAnon:", &stats.nAnonymousResidentBytes ) ) {
		stats.nValidFields |= SIR_PROCESS_MEMORY_ANONYMOUS_RESIDENT;
	}
	struct rusage usage = {};
	if ( getrusage( RUSAGE_SELF, &usage ) == 0 ) {
		stats.nPageFaultCount = (uint64_t)usage.ru_minflt + (uint64_t)usage.ru_majflt;
		stats.nValidFields |= SIR_PROCESS_MEMORY_PAGE_FAULTS;
	}
	if ( ( stats.nValidFields & SIR_PROCESS_MEMORY_RESIDENT ) == 0 ) {
		return false;
	}
	*pStats = stats;
	return true;
}
