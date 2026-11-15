/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRADAPTIVEWORKBUDGETER_H__
#define __SIRENGINE_SIRADAPTIVEWORKBUDGETER_H__

#include <Resources/SIRResourceTypes.h>

typedef struct sirWorkBudgetVector_s
{
	uint64_t nCpuMicroseconds;
	uint64_t nGpuMicroseconds;
	uint64_t nIOBytes;
	uint64_t nIngressBytes;
	uint64_t nRamBytes;
	uint64_t nVramBytes;
	uint64_t nUploadBytes;
	uint64_t nWorkerSlots;
} sirWorkBudgetVector_t;

typedef enum sirWorkBudgetClass_e : uint8_t
{
	SIR_WORK_BUDGET_MANDATORY = 0,
	SIR_WORK_BUDGET_OPTIONAL = 1
} sirWorkBudgetClass_t;

typedef struct sirWorkBudgetRequest_s
{
	SIRResourceKey key;
	uint64_t nRevision;
	uint64_t nPhysicalBackingId;
	uint32_t nBackingGeneration;
	sirWorkBudgetClass_t eClass;
	sirWorkBudgetVector_t cost;
} sirWorkBudgetRequest_t;

typedef struct sirWorkBudgetHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirWorkBudgetHandle_t;

typedef struct sirWorkBudgetConfig_s
{
	sirWorkBudgetVector_t limits;
	sirWorkBudgetVector_t mandatoryReserve;
	uint32_t nCapacity;
} sirWorkBudgetConfig_t;

typedef enum sirWorkBudgetResult_e : uint8_t
{
	SIR_WORK_BUDGET_OK = 0,
	SIR_WORK_BUDGET_INVALID_ARGUMENT,
	SIR_WORK_BUDGET_NOT_READY,
	SIR_WORK_BUDGET_OUT_OF_CAPACITY,
	SIR_WORK_BUDGET_OUT_OF_BUDGET,
	SIR_WORK_BUDGET_STALE_HANDLE,
	SIR_WORK_BUDGET_BUSY
} sirWorkBudgetResult_t;

struct sirWorkBudgetSlot_s;

// Single-owner, fixed-capacity physical/admission ledger. One backing ID
// incurs RAM/VRAM cost only once, even when several systems borrow it.
// All scratch and metadata storage is caller supplied from SIRMemory.
class SIRAdaptiveWorkBudgeter final
{
private:
	sirWorkBudgetSlot_s *m_pSlots;
	sirWorkBudgetConfig_t m_Config;
	sirWorkBudgetVector_t m_Used;
	uint32_t m_nActive;
	bool m_bInitialized;

public:
	SIRAdaptiveWorkBudgeter( void ) noexcept;
	static uint64_t requiredBytes( uint32_t nCapacity ) noexcept;
	sirWorkBudgetResult_t initialize( void *pStorage, uint64_t nStorageBytes, const sirWorkBudgetConfig_t& config ) noexcept;
	sirWorkBudgetResult_t admit( const sirWorkBudgetRequest_t& request, sirWorkBudgetHandle_t *pHandle ) noexcept;
	sirWorkBudgetResult_t release( sirWorkBudgetHandle_t hHandle ) noexcept;
	sirWorkBudgetVector_t getUsed( void ) const noexcept;
	uint32_t getActiveCount( void ) const noexcept;
	sirWorkBudgetResult_t shutdown( void ) noexcept;
};

#endif // __SIRENGINE_SIRADAPTIVEWORKBUDGETER_H__
