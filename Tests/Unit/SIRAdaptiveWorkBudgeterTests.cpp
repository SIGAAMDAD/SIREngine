/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Prediction/Budget/SIRAdaptiveWorkBudgeter.h>

#include <stdio.h>

#define REQUIRE( x )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( x ) ) {                                                                                                                    \
			fprintf( stderr, "budget assertion %u: %s\n", __LINE__, #x );                                                                  \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static sirWorkBudgetRequest_t makeRequest( uint64_t nAssetId, sirWorkBudgetClass_t eClass ) noexcept
{
	sirWorkBudgetRequest_t request = {};
	request.key.m_nAssetId = nAssetId;
	request.key.m_nTypeId = 1;
	request.nRevision = 1;
	request.eClass = eClass;
	request.cost.nCpuMicroseconds = 10;
	request.cost.nIOBytes = 16;
	request.cost.nRamBytes = 20;
	request.cost.nVramBytes = 30;
	request.cost.nWorkerSlots = 1;
	return request;
}

static bool testReservationsAndPhysicalDedup( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[16384] = {};
	sirWorkBudgetConfig_t config = {};
	config.nCapacity = 8;
	config.limits = { 100, 100, 100, 100, 100, 100, 100, 8 };
	config.mandatoryReserve = { 30, 0, 30, 0, 30, 30, 0, 2 };
	SIRAdaptiveWorkBudgeter budget;
	REQUIRE( SIRAdaptiveWorkBudgeter::requiredBytes( config.nCapacity ) <= sizeof( szStorage ) );
	REQUIRE( budget.initialize( szStorage, sizeof( szStorage ), config ) == SIR_WORK_BUDGET_OK );

	sirWorkBudgetRequest_t a = makeRequest( 1, SIR_WORK_BUDGET_OPTIONAL );
	a.nPhysicalBackingId = 123;
	a.nBackingGeneration = 9;
	sirWorkBudgetHandle_t ha = {};
	REQUIRE( budget.admit( a, &ha ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.getUsed().nRamBytes == 20 && budget.getUsed().nVramBytes == 30 );

	sirWorkBudgetRequest_t b = makeRequest( 2, SIR_WORK_BUDGET_OPTIONAL );
	b.nPhysicalBackingId = 123;
	b.nBackingGeneration = 9;
	sirWorkBudgetHandle_t hb = {};
	REQUIRE( budget.admit( b, &hb ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.getUsed().nRamBytes == 20 && budget.getUsed().nVramBytes == 30 );
	REQUIRE( budget.getUsed().nIOBytes == 32 );
	REQUIRE( budget.release( ha ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.getUsed().nRamBytes == 20 && budget.getUsed().nVramBytes == 30 );
	REQUIRE( budget.release( ha ) == SIR_WORK_BUDGET_STALE_HANDLE );

	sirWorkBudgetRequest_t mismatch = makeRequest( 3, SIR_WORK_BUDGET_MANDATORY );
	mismatch.nPhysicalBackingId = 123;
	mismatch.nBackingGeneration = 9;
	mismatch.cost.nRamBytes = 21;
	sirWorkBudgetHandle_t untouched = { 77, 99 };
	REQUIRE( budget.admit( mismatch, &untouched ) == SIR_WORK_BUDGET_INVALID_ARGUMENT );
	REQUIRE( untouched.nSlot == 77 && untouched.nGeneration == 99 );

	sirWorkBudgetRequest_t tooLarge = makeRequest( 3, SIR_WORK_BUDGET_OPTIONAL );
	tooLarge.cost.nCpuMicroseconds = 70;
	REQUIRE( budget.admit( tooLarge, &untouched ) == SIR_WORK_BUDGET_OUT_OF_BUDGET );
	REQUIRE( untouched.nSlot == 77 && untouched.nGeneration == 99 );

	const sirWorkBudgetRequest_t required = makeRequest( 4, SIR_WORK_BUDGET_MANDATORY );
	sirWorkBudgetHandle_t hc = {};
	REQUIRE( budget.admit( required, &hc ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.shutdown() == SIR_WORK_BUDGET_BUSY );
	REQUIRE( budget.release( hb ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.getUsed().nRamBytes == 20 && budget.getUsed().nVramBytes == 30 );
	REQUIRE( budget.release( hc ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.getUsed().nRamBytes == 0 && budget.getUsed().nVramBytes == 0 );
	REQUIRE( budget.getActiveCount() == 0 );
	REQUIRE( budget.shutdown() == SIR_WORK_BUDGET_OK );
	return true;
}

static bool testCapacityAndOverflow( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	SIRAdaptiveWorkBudgeter budget;
	sirWorkBudgetConfig_t invalid = {};
	invalid.nCapacity = 2;
	invalid.limits.nRamBytes = 1;
	invalid.mandatoryReserve.nRamBytes = 2;
	REQUIRE( budget.initialize( szStorage, sizeof( szStorage ), invalid ) == SIR_WORK_BUDGET_INVALID_ARGUMENT );

	sirWorkBudgetConfig_t config = {};
	config.nCapacity = 1;
	config.limits = { UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX };
	REQUIRE( budget.initialize( szStorage, sizeof( szStorage ), config ) == SIR_WORK_BUDGET_OK );
	const sirWorkBudgetRequest_t request = makeRequest( 1, SIR_WORK_BUDGET_MANDATORY );
	sirWorkBudgetHandle_t ha = {};
	REQUIRE( budget.admit( request, &ha ) == SIR_WORK_BUDGET_OK );
	sirWorkBudgetHandle_t extra = { 88, 44 };
	REQUIRE( budget.admit( request, &extra ) == SIR_WORK_BUDGET_OUT_OF_CAPACITY );
	REQUIRE( extra.nSlot == 88 && extra.nGeneration == 44 );
	REQUIRE( budget.release( ha ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.admit( request, &extra ) == SIR_WORK_BUDGET_OK );
	REQUIRE( extra.nGeneration != ha.nGeneration );
	REQUIRE( budget.release( extra ) == SIR_WORK_BUDGET_OK );
	REQUIRE( budget.shutdown() == SIR_WORK_BUDGET_OK );
	return true;
}

int main( void )
{
	return testReservationsAndPhysicalDedup() && testCapacityAndOverflow() ? 0 : 1;
}
