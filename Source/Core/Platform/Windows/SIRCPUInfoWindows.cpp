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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

bool SIRCPUInfo::queryTopology( sirCPUInfo_t *pInfo ) noexcept
{
	const DWORD nLogical = GetActiveProcessorCount( ALL_PROCESSOR_GROUPS );
	pInfo->nLogicalProcessorCount = ( uint32_t )nLogical;
	DWORD nBytes = 0;
	GetLogicalProcessorInformationEx( RelationProcessorCore, nullptr, &nBytes );
	if ( nBytes == 0 || nBytes > 16 * 1024 * 1024 ) {
		return nLogical > 0;
	}
	void *pBuffer = VirtualAlloc( nullptr, nBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
	if ( pBuffer == nullptr ) {
		return nLogical > 0;
	}
	DWORD nWritten = nBytes;
	if ( GetLogicalProcessorInformationEx( RelationProcessorCore, ( PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX )pBuffer, &nWritten ) ) {
		DWORD nOffset = 0;
		uint32_t nPhysical = 0;
		while ( nOffset < nWritten ) {
			const PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pRecord =
			    ( PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX )( ( uint8_t * )pBuffer + nOffset );
			if ( pRecord->Size < sizeof( DWORD ) * 2 || pRecord->Size > nWritten - nOffset ) {
				nPhysical = 0;
				break;
			}
			if ( pRecord->Relationship == RelationProcessorCore ) {
				++nPhysical;
			}
			nOffset += pRecord->Size;
		}
		pInfo->nPhysicalCoreCount = nPhysical;
	}
	VirtualFree( pBuffer, 0, MEM_RELEASE );
	return nLogical > 0;
}
