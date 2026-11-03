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

#include <Core/Platform/SIRCPUInfo.h>

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static bool readTopologyNumber( uint32_t nCPU, const char *szField, uint32_t *pValue ) noexcept
{
	char szPath[160];
	const int nPathLength = snprintf( szPath, sizeof( szPath ),
		"/sys/devices/system/cpu/cpu%u/topology/%s", nCPU, szField );
	if ( nPathLength <= 0 || (size_t)nPathLength >= sizeof( szPath ) ) {
		return false;
	}
	FILE *pFile = fopen( szPath, "r" );
	if ( pFile == nullptr ) {
		return false;
	}
	unsigned int nParsed = 0;
	const int nResult = fscanf( pFile, "%u", &nParsed );
	fclose( pFile );
	if ( nResult != 1 ) {
		return false;
	}
	*pValue = (uint32_t)nParsed;
	return true;
}

bool SIRCPUInfo::queryTopology( sirCPUInfo_t *pInfo ) noexcept
{
	cpu_set_t allowed;
	CPU_ZERO( &allowed );
	if ( sched_getaffinity( 0, sizeof( allowed ), &allowed ) != 0 ) {
		const long nOnline = sysconf( _SC_NPROCESSORS_ONLN );
		pInfo->nLogicalProcessorCount = nOnline > 0 ? (uint32_t)nOnline : 1;
		return true;
	}
	uint64_t szCoreKeys[CPU_SETSIZE] = {};
	uint32_t nCores = 0;
	uint32_t nLogical = 0;
	bool bTopologyComplete = true;
	for ( uint32_t iCPU = 0; iCPU < CPU_SETSIZE; ++iCPU ) {
		if ( !CPU_ISSET( (int)iCPU, &allowed ) ) {
			continue;
		}
		++nLogical;
		uint32_t nPackage = 0;
		uint32_t nCore = 0;
		if ( !readTopologyNumber( iCPU, "physical_package_id", &nPackage ) ||
			!readTopologyNumber( iCPU, "core_id", &nCore ) ) {
			bTopologyComplete = false;
			continue;
		}
		const uint64_t nKey = ( (uint64_t)nPackage << 32 ) | nCore;
		bool bSeen = false;
		for ( uint32_t iCore = 0; iCore < nCores; ++iCore ) {
			if ( szCoreKeys[iCore] == nKey ) {
				bSeen = true;
				break;
			}
		}
		if ( !bSeen ) {
			szCoreKeys[nCores] = nKey;
			++nCores;
		}
	}
	pInfo->nLogicalProcessorCount = nLogical;
	pInfo->nPhysicalCoreCount = bTopologyComplete ? nCores : 0;
	return nLogical > 0;
}
