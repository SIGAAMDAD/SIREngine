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

#include <World/HLOD/SIRWorldHLODManifest.h>

#include <stdio.h>
#include <string.h>

#define SIR_HLOD_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "World HLOD manifest failure: %s at line %d\n", #expression, __LINE__ );                                      \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

static uint64_t sirHLODTestChecksum( const uint8_t *pData, size_t nBytes ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash = ( nHash ^ pData[iByte] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

static void sirHLODTestRepairChecksum( uint8_t *pData, uint64_t nBytes ) noexcept
{
	uint64_t nHash =
	    sirHLODTestChecksum( pData + SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES, ( size_t )( nBytes - SIR_WORLD_HLOD_MANIFEST_HEADER_BYTES ) );
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		pData[40 + iByte] = ( uint8_t )( nHash >> ( iByte * 8 ) );
	}
}

int main( void )
{
	sirWorldHLODManifestNode_t szSource[3] = {};
	szSource[0].node = { { 11, 3, 1 }, 21, 1, 2 };
	szSource[1].node = { { 12, 3, 1 }, 22, 0, 0 };
	szSource[2].node = { { 13, 3, 2 }, 23, 0, 0 };
	for ( uint32_t iNode = 0; iNode < 3; ++iNode ) {
		for ( uint32_t iAxis = 0; iAxis < 3; ++iAxis ) {
			szSource[iNode].fMinimum[iAxis] = 0.0f;
			szSource[iNode].fMaximum[iAxis] = iNode == 0 ? 10.0f : 5.0f;
		}
		szSource[iNode].fGeometricError = iNode == 0 ? 4.0f : 1.0f;
	}
	const sirWorldHLODManifestIdentity_t identity = { 100, 101, 102 };
	const sirWorldHLODManifestWrite_t write = { szSource, 3, identity };
	uint8_t szWire[256] = {};
	sirWorldHLODScratch_t szScratch[3] = {};
	uint64_t nWritten = 9;
	sirWorldHLODManifestEncode_t encoded = { szWire, sizeof( szWire ), szScratch, 3, &nWritten };
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, encoded ) == SIR_WORLD_HLOD_MANIFEST_OK );
	SIR_HLOD_CHECK( nWritten == 48 + 3 * 64 && szWire[0] == 'H' && szWire[1] == 'L' && szWire[2] == 'O' && szWire[3] == 'D' );
	const uint64_t nValidBytes = nWritten;

	sirWorldHLODNode_t szNodes[3] = {};
	sirWorldHLODManifestBounds_t szBounds[3] = {};
	float szErrors[3] = {};
	uint32_t nDecoded = 13;
	sirWorldHLODManifestDecode_t decoded = { szNodes, szBounds, szErrors, 3, szScratch, 3, &nDecoded };
	sirWorldHLODManifestRead_t read = { szWire, nWritten, identity, 3 };
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_OK );
	SIR_HLOD_CHECK( nDecoded == 3 && szNodes[0].nFirstChild == 1 && szNodes[0].nChildCount == 2 );
	SIR_HLOD_CHECK( szNodes[2].keyGeometry.m_nVariantId == 2 && szNodes[2].nSourceRevision == 23 );
	SIR_HLOD_CHECK( szBounds[0].fMaximum[1] == 10.0f && szErrors[0] == 4.0f );

	// Decoded nodes are the exact flat-array representation used by the CPU cut planner.
	sirWorldHLODResidency_t szResident[] = { { 21, 3, true }, { 22, 3, true }, { 23, 3, true } };
	sirWorldHLODScratch_t szCutScratch[3] = {};
	uint32_t szSelected[3] = {};
	sirWorldHLODDemand_t szDemands[3] = {};
	uint32_t nSelected = 0;
	uint32_t nDemands = 0;
	sirWorldHLODInput_t cutInput = { szNodes, szResident, szErrors, 3, 3, 2.0f };
	sirWorldHLODOutput_t cutOutput = { szCutScratch, 3, szSelected, 3, &nSelected, szDemands, 3, &nDemands };
	SIR_HLOD_CHECK( sirWorldHLODPlan( cutInput, cutOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 2 && szSelected[0] == 1 && szSelected[1] == 2 && nDemands == 0 );
	szResident[1].bReady = false;
	SIR_HLOD_CHECK( sirWorldHLODPlan( cutInput, cutOutput ) == SIR_WORLD_HLOD_OK );
	SIR_HLOD_CHECK( nSelected == 1 && szSelected[0] == 0 && nDemands == 1 && szDemands[0].nNodeIndex == 1 );

	// Wire format is deterministic and independent of in-memory struct padding.
	uint8_t szSecond[256] = {};
	uint64_t nSecond = 0;
	sirWorldHLODManifestEncode_t second = { szSecond, sizeof( szSecond ), szScratch, 3, &nSecond };
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, second ) == SIR_WORLD_HLOD_MANIFEST_OK );
	SIR_HLOD_CHECK( nSecond == nWritten && memcmp( szWire, szSecond, ( size_t )nWritten ) == 0 );

	// Wrong catalog identity and changed source fingerprints reject the package.
	read.expected.nSourceHashLo = 999;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_STALE_SOURCE );
	read.expected.nSourceHashLo = identity.nSourceHashLo;
	read.expected.nWorldId = 101;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_STALE_SOURCE );
	read.expected.nWorldId = identity.nWorldId;

	// Every failed read preserves all previously decoded nodes and count.
	szNodes[0].nSourceRevision = 123456;
	nDecoded = 77;
	read.nBytes--;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT );
	SIR_HLOD_CHECK( nDecoded == 77 && szNodes[0].nSourceRevision == 123456 );
	read.nBytes++;
	szWire[64] ^= 1;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT );
	SIR_HLOD_CHECK( nDecoded == 77 && szNodes[0].nSourceRevision == 123456 );
	szWire[64] ^= 1;
	szWire[4] = 2;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT );
	szWire[4] = 1;
	szWire[8] = 0;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT );
	szWire[8] = 64;
	read.nMaxNodes = 2;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_CAPACITY );
	read.nMaxNodes = 3;
	decoded.nScratchCapacity = 2;
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_CAPACITY );
	decoded.nScratchCapacity = 3;

	// A correct checksum does not override structural validation.
	szWire[48 + 24] = 3;
	sirHLODTestRepairChecksum( szWire, nValidBytes );
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY );
	szWire[48 + 24] = 1;
	sirHLODTestRepairChecksum( szWire, nValidBytes );
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_OK );
	szWire[48 + 60] = 1;
	sirHLODTestRepairChecksum( szWire, nValidBytes );
	SIR_HLOD_CHECK( sirWorldHLODReadManifest( read, decoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_FORMAT );
	szWire[48 + 60] = 0;
	sirHLODTestRepairChecksum( szWire, nValidBytes );

	// Reject overlapping ownership, child bounds escaping a parent and nonmonotonic error.
	szSource[1].node.nFirstChild = 2;
	szSource[1].node.nChildCount = 1;
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, encoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY );
	szSource[1].node.nFirstChild = 0;
	szSource[1].node.nChildCount = 0;
	szSource[1].fMaximum[0] = 11.0f;
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, encoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY );
	szSource[1].fMaximum[0] = 5.0f;
	szSource[1].fGeometricError = 5.0f;
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, encoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY );
	szSource[1].fGeometricError = 1.0f;
	szSource[1].node.nSourceRevision = 0;
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, encoded ) == SIR_WORLD_HLOD_MANIFEST_INVALID_HIERARCHY );
	szSource[1].node.nSourceRevision = 22;
	encoded.nCapacity = 8;
	nWritten = 78;
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, encoded ) == SIR_WORLD_HLOD_MANIFEST_CAPACITY );
	SIR_HLOD_CHECK( nWritten == 78 );
	encoded.nCapacity = sizeof( szWire );
	SIR_HLOD_CHECK( sirWorldHLODWriteManifest( write, encoded ) == SIR_WORLD_HLOD_MANIFEST_OK );
	return 0;
}
