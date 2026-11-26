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

#ifndef __SIR_JOBIOSERVICE_H__
#define __SIR_JOBIOSERVICE_H__
#include <Core/IO/SIRAsyncIOService.h>
#include <Core/Jobs/SIRTaskScheduler.h>
typedef struct sirJobIOConfig_s { SIRTaskScheduler *pScheduler; void *pRecordStorage; uint32_t nRecordCapacity; } sirJobIOConfig_t;
class SIRJobIOService final : public SIRAsyncIOService
{
public:
	SIRJobIOService( void ) noexcept;
	sirIOResult_t initialize( const sirJobIOConfig_t& ) noexcept;
	sirIOResult_t submit( const sirIORequest_t&, sirIOHandle_t* ) noexcept override;
	sirIOResult_t submitBatch( const sirIOBatch_t&, sirIOHandle_t* ) noexcept override;
	sirIOResult_t cancel( sirIOHandle_t ) noexcept override;
	uint32_t collect( sirIOCompletion_t*, uint32_t ) noexcept override;
	sirIOResult_t shutdown( void ) noexcept override;
private:
	struct Record;
	static SIRTaskResult run( void*, sirTaskHandle_t ) noexcept;
	SIRTaskScheduler *m_pScheduler; Record *m_pRecords; uint32_t m_nCapacity; bool m_bInitialized;
};
#endif
