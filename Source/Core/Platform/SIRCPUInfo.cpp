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

#include <string.h>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	#include <intrin.h>
#elif (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
	#include <cpuid.h>
#endif

static void queryCPUID( uint32_t nLeaf, uint32_t nSubleaf, uint32_t *pRegs ) noexcept
{
	pRegs[0] = 0;
	pRegs[1] = 0;
	pRegs[2] = 0;
	pRegs[3] = 0;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	int szRegs[4] = {};
	__cpuidex( szRegs, (int)nLeaf, (int)nSubleaf );
	for ( int iReg = 0; iReg < 4; ++iReg ) {
		pRegs[iReg] = (uint32_t)szRegs[iReg];
	}
#elif (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
	uint32_t nA = 0, nB = 0, nC = 0, nD = 0;
	__cpuid_count( nLeaf, nSubleaf, nA, nB, nC, nD );
	pRegs[0] = nA;
	pRegs[1] = nB;
	pRegs[2] = nC;
	pRegs[3] = nD;
#else
	( void )nLeaf;
	( void )nSubleaf;
#endif
}

static uint64_t getEnabledXCR0( void ) noexcept
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	return (uint64_t)_xgetbv( 0 );
#elif defined(__x86_64__) || defined(__i386__)
	uint32_t nLo = 0;
	uint32_t nHi = 0;
	__asm__ volatile( "xgetbv" : "=a"( nLo ), "=d"( nHi ) : "c"( 0 ) );
	return ( (uint64_t)nHi << 32 ) | nLo;
#else
	return 0;
#endif
}

static void queryCacheSizes( sirCPUInfo_t *pInfo, uint32_t nHighestLeaf ) noexcept
{
	if ( nHighestLeaf < 4 ) {
		return;
	}

	for ( uint32_t iCache = 0; iCache < 32; ++iCache ) {
		uint32_t szRegs[4];
		queryCPUID( 4, iCache, szRegs );
		const uint32_t nType = szRegs[0] & 0x1fU;
		if ( nType == 0 ) {
			break;
		}
		const uint32_t nLevel = ( szRegs[0] >> 5 ) & 7U;
		const uint64_t nLine = ( ( szRegs[1] & 0xfffU ) + 1U );
		const uint64_t nPartitions = ( ( szRegs[1] >> 12 ) & 0x3ffU ) + 1U;
		const uint64_t nWays = ( ( szRegs[1] >> 22 ) & 0x3ffU ) + 1U;
		const uint64_t nSets = (uint64_t)szRegs[2] + 1U;
		const uint64_t nSize = nLine * nPartitions * nWays * nSets;
		if ( nLevel == 1 && nType == 1 && nSize > pInfo->nL1DataCacheBytes ) {
			pInfo->nL1DataCacheBytes = nSize;
		} else if ( nLevel == 2 && nSize > pInfo->nL2CacheBytes ) {
			pInfo->nL2CacheBytes = nSize;
		} else if ( nLevel == 3 && nSize > pInfo->nL3CacheBytes ) {
			pInfo->nL3CacheBytes = nSize;
		}
		if ( nLevel == 1 && pInfo->nCacheLineBytes == 0 ) {
			pInfo->nCacheLineBytes = (uint32_t)nLine;
		}
	}
}

void SIRCPUInfo::queryInstructionSet( sirCPUInfo_t *pInfo ) noexcept
{
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
	uint32_t szRegs[4];
	queryCPUID( 0, 0, szRegs );
	const uint32_t nHighest = szRegs[0];
	memcpy( pInfo->szVendor, &szRegs[1], 4 );
	memcpy( pInfo->szVendor + 4, &szRegs[3], 4 );
	memcpy( pInfo->szVendor + 8, &szRegs[2], 4 );

	queryCPUID( 0x80000000U, 0, szRegs );
	if ( szRegs[0] >= 0x80000004U ) {
		for ( uint32_t iLeaf = 0; iLeaf < 3; ++iLeaf ) {
			queryCPUID( 0x80000002U + iLeaf, 0, szRegs );
			memcpy( pInfo->szBrand + 16 * iLeaf, szRegs, 16 );
		}
	}
	if ( nHighest < 1 ) {
		return;
	}
	queryCPUID( 1, 0, szRegs );
	const uint32_t nECX = szRegs[2];
	const uint32_t nEDX = szRegs[3];
	if ( nEDX & (1U << 26) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_SSE2; }
	if ( nECX & (1U << 19) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_SSE41; }
	if ( nECX & (1U << 20) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_SSE42; }
	if ( nECX & (1U << 23) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_POPCNT; }
	if ( nECX & (1U << 25) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_AES; }
	const bool bOSXSAVE = ( nECX & (1U << 27) ) != 0;
	const uint64_t nXCR0 = bOSXSAVE ? getEnabledXCR0() : 0;
	const bool bAVXEnabled = ( nECX & (1U << 28) ) != 0 && ( nXCR0 & 0x6U ) == 0x6U;
	if ( bAVXEnabled ) {
		pInfo->nFeatures |= SIR_CPU_FEATURE_AVX;
		if ( nECX & (1U << 12) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_FMA; }
	}
	if ( nHighest >= 7 ) {
		queryCPUID( 7, 0, szRegs );
		if ( szRegs[1] & (1U << 3) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_BMI1; }
		if ( szRegs[1] & (1U << 8) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_BMI2; }
		if ( bAVXEnabled && ( szRegs[1] & (1U << 5) ) ) { pInfo->nFeatures |= SIR_CPU_FEATURE_AVX2; }
		const bool bZMM = ( nXCR0 & 0xe6U ) == 0xe6U;
		if ( bAVXEnabled && bZMM && ( szRegs[1] & (1U << 16) ) ) {
			pInfo->nFeatures |= SIR_CPU_FEATURE_AVX512F;
		}
	}
	queryCacheSizes( pInfo, nHighest );
#elif defined(__aarch64__) || defined(_M_ARM64)
	pInfo->nFeatures |= SIR_CPU_FEATURE_NEON;
#endif
}

bool SIRCPUInfo::query( sirCPUInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return false;
	}
	sirCPUInfo_t info = {};
	if ( !queryTopology( &info ) || info.nLogicalProcessorCount == 0 ) {
		return false;
	}
	queryInstructionSet( &info );
	*pInfo = info;
	return true;
}
