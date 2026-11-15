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

#include <limits.h>
#include <new>

struct sirWorkBudgetSlot_s
{
	sirWorkBudgetRequest_t request;
	sirWorkBudgetVector_t charged;
	uint32_t nGeneration = 1;
	bool bActive = false;
};

static const uint64_t& sirBudgetValue( const sirWorkBudgetVector_t& value, uint32_t i ) noexcept
{
	switch ( i ) {
	case 0:
		return value.nCpuMicroseconds;
	case 1:
		return value.nGpuMicroseconds;
	case 2:
		return value.nIOBytes;
	case 3:
		return value.nIngressBytes;
	case 4:
		return value.nRamBytes;
	case 5:
		return value.nVramBytes;
	case 6:
		return value.nUploadBytes;
	case 7:
		return value.nWorkerSlots;
	default:
		return value.nCpuMicroseconds;
	}
}

static uint64_t& sirBudgetMutableValue( sirWorkBudgetVector_t& value, uint32_t i ) noexcept
{
	switch ( i ) {
	case 0:
		return value.nCpuMicroseconds;
	case 1:
		return value.nGpuMicroseconds;
	case 2:
		return value.nIOBytes;
	case 3:
		return value.nIngressBytes;
	case 4:
		return value.nRamBytes;
	case 5:
		return value.nVramBytes;
	case 6:
		return value.nUploadBytes;
	case 7:
		return value.nWorkerSlots;
	default:
		return value.nCpuMicroseconds;
	}
}

static bool sirBudgetContains( const sirWorkBudgetVector_t& lhs, const sirWorkBudgetVector_t& rhs ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		if ( sirBudgetValue( lhs, i ) < sirBudgetValue( rhs, i ) ) {
			return false;
		}
	}
	return true;
}

static bool sirBudgetFits( const sirWorkBudgetVector_t& used, const sirWorkBudgetVector_t& wanted,
                           const sirWorkBudgetVector_t& limit ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		const uint64_t nUsed = sirBudgetValue( used, i );
		const uint64_t nWanted = sirBudgetValue( wanted, i );
		const uint64_t nLimit = sirBudgetValue( limit, i );
		if ( nUsed > nLimit || nWanted > nLimit - nUsed ) {
			return false;
		}
	}
	return true;
}

static void sirBudgetAdd( sirWorkBudgetVector_t& used, const sirWorkBudgetVector_t& cost ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		sirBudgetMutableValue( used, i ) += sirBudgetValue( cost, i );
	}
}

static void sirBudgetSubtract( sirWorkBudgetVector_t& used, const sirWorkBudgetVector_t& cost ) noexcept
{
	for ( uint32_t i = 0; i < 8; ++i ) {
		sirBudgetMutableValue( used, i ) -= sirBudgetValue( cost, i );
	}
}

static sirWorkBudgetVector_t sirBudgetOptionalLimit( const sirWorkBudgetConfig_t& config ) noexcept
{
	sirWorkBudgetVector_t result = {};
	for ( uint32_t i = 0; i < 8; ++i ) {
		sirBudgetMutableValue( result, i ) = sirBudgetValue( config.limits, i ) - sirBudgetValue( config.mandatoryReserve, i );
	}
	return result;
}

SIRAdaptiveWorkBudgeter::SIRAdaptiveWorkBudgeter( void ) noexcept
    : m_pSlots( nullptr ), m_Config{}, m_Used{}, m_nActive( 0 ), m_bInitialized( false )
{
}

uint64_t SIRAdaptiveWorkBudgeter::requiredBytes( uint32_t nCapacity ) noexcept
{
	if ( nCapacity == 0 || nCapacity > 65536 ) {
		return 0;
	}
	return ( uint64_t )nCapacity * sizeof( sirWorkBudgetSlot_s );
}

sirWorkBudgetResult_t SIRAdaptiveWorkBudgeter::initialize( void *pStorage, uint64_t nStorageBytes,
                                                           const sirWorkBudgetConfig_t& config ) noexcept
{
	const uint64_t nNeeded = requiredBytes( config.nCapacity );
	if ( m_bInitialized || pStorage == nullptr || nNeeded == 0 || nStorageBytes < nNeeded ||
	     ( ( uintptr_t )pStorage % alignof( sirWorkBudgetSlot_s ) ) != 0 || !sirBudgetContains( config.limits, config.mandatoryReserve ) ) {
		return SIR_WORK_BUDGET_INVALID_ARGUMENT;
	}
	m_pSlots = ( sirWorkBudgetSlot_s * )pStorage;
	for ( uint32_t i = 0; i < config.nCapacity; ++i ) {
		new ( m_pSlots + i ) sirWorkBudgetSlot_s();
	}
	m_Config = config;
	m_Used = {};
	m_nActive = 0;
	m_bInitialized = true;
	return SIR_WORK_BUDGET_OK;
}

sirWorkBudgetResult_t SIRAdaptiveWorkBudgeter::admit( const sirWorkBudgetRequest_t& request, sirWorkBudgetHandle_t *pHandle ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_WORK_BUDGET_NOT_READY;
	}
	if ( pHandle == nullptr || request.key.m_nAssetId == 0 || request.nRevision == 0 || request.eClass > SIR_WORK_BUDGET_OPTIONAL ||
	     ( request.nPhysicalBackingId != 0 && request.nBackingGeneration == 0 ) ) {
		return SIR_WORK_BUDGET_INVALID_ARGUMENT;
	}
	uint32_t iVacant = UINT32_MAX;
	sirWorkBudgetVector_t wanted = request.cost;
	for ( uint32_t iSlot = 0; iSlot < m_Config.nCapacity; ++iSlot ) {
		const sirWorkBudgetSlot_s& slot = m_pSlots[iSlot];
		if ( !slot.bActive ) {
			if ( iVacant == UINT32_MAX && slot.nGeneration != UINT32_MAX ) {
				iVacant = iSlot;
			}
			continue;
		}
		if ( request.nPhysicalBackingId != 0 && slot.request.nPhysicalBackingId == request.nPhysicalBackingId &&
		     slot.request.nBackingGeneration == request.nBackingGeneration ) {
			if ( slot.request.cost.nRamBytes != request.cost.nRamBytes || slot.request.cost.nVramBytes != request.cost.nVramBytes ) {
				return SIR_WORK_BUDGET_INVALID_ARGUMENT;
			}
			wanted.nRamBytes = 0;
			wanted.nVramBytes = 0;
		}
	}
	if ( iVacant == UINT32_MAX ) {
		return SIR_WORK_BUDGET_OUT_OF_CAPACITY;
	}
	const sirWorkBudgetVector_t limit = request.eClass == SIR_WORK_BUDGET_OPTIONAL ? sirBudgetOptionalLimit( m_Config ) : m_Config.limits;
	if ( !sirBudgetFits( m_Used, wanted, limit ) ) {
		return SIR_WORK_BUDGET_OUT_OF_BUDGET;
	}
	sirWorkBudgetSlot_s& slot = m_pSlots[iVacant];
	slot.request = request;
	slot.charged = wanted;
	slot.bActive = true;
	sirBudgetAdd( m_Used, wanted );
	++m_nActive;
	*pHandle = { iVacant, slot.nGeneration };
	return SIR_WORK_BUDGET_OK;
}

sirWorkBudgetResult_t SIRAdaptiveWorkBudgeter::release( sirWorkBudgetHandle_t hHandle ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_WORK_BUDGET_NOT_READY;
	}
	if ( hHandle.nSlot >= m_Config.nCapacity || hHandle.nGeneration == 0 ) {
		return SIR_WORK_BUDGET_STALE_HANDLE;
	}
	sirWorkBudgetSlot_s& slot = m_pSlots[hHandle.nSlot];
	if ( !slot.bActive || slot.nGeneration != hHandle.nGeneration ) {
		return SIR_WORK_BUDGET_STALE_HANDLE;
	}
	if ( slot.request.nPhysicalBackingId != 0 && ( slot.charged.nRamBytes != 0 || slot.charged.nVramBytes != 0 ) ) {
		for ( uint32_t iOther = 0; iOther < m_Config.nCapacity; ++iOther ) {
			if ( iOther == hHandle.nSlot || !m_pSlots[iOther].bActive ) {
				continue;
			}
			sirWorkBudgetSlot_s& other = m_pSlots[iOther];
			if ( other.request.nPhysicalBackingId == slot.request.nPhysicalBackingId &&
			     other.request.nBackingGeneration == slot.request.nBackingGeneration ) {
				other.charged.nRamBytes = slot.charged.nRamBytes;
				other.charged.nVramBytes = slot.charged.nVramBytes;
				slot.charged.nRamBytes = 0;
				slot.charged.nVramBytes = 0;
				break;
			}
		}
	}
	sirBudgetSubtract( m_Used, slot.charged );
	slot.request = {};
	slot.charged = {};
	slot.bActive = false;
	--m_nActive;
	if ( slot.nGeneration != UINT32_MAX ) {
		++slot.nGeneration;
	}
	return SIR_WORK_BUDGET_OK;
}

sirWorkBudgetVector_t SIRAdaptiveWorkBudgeter::getUsed( void ) const noexcept
{
	return m_bInitialized ? m_Used : sirWorkBudgetVector_t{};
}

uint32_t SIRAdaptiveWorkBudgeter::getActiveCount( void ) const noexcept
{
	return m_bInitialized ? m_nActive : 0;
}

sirWorkBudgetResult_t SIRAdaptiveWorkBudgeter::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_WORK_BUDGET_NOT_READY;
	}
	if ( m_nActive != 0 ) {
		return SIR_WORK_BUDGET_BUSY;
	}
	m_pSlots = nullptr;
	m_Config = {};
	m_Used = {};
	m_bInitialized = false;
	return SIR_WORK_BUDGET_OK;
}
