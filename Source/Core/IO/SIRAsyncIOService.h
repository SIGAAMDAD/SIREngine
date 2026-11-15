/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties of
merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRASYNCIOSERVICE_H__
#define __SIRENGINE_SIRASYNCIOSERVICE_H__

#include <SIRPCH.hpp>
#include <Core/Platform/SIRSync.h>
#include <Core/Platform/SIRThread.h>

// Backend-neutral bounded asynchronous file reader. Each accepted request
// produces one terminal completion. Destinations and open sources stay pinned
// until that completion is acknowledged, including across logical cancellation.
typedef enum sirIOResult_e : uint8_t
{
	SIR_IO_OK = 0,
	SIR_IO_INVALID_ARGUMENT,
	SIR_IO_INVALID_HANDLE,
	SIR_IO_OUT_OF_CAPACITY,
	SIR_IO_BUSY,
	SIR_IO_NOT_READY,
	SIR_IO_CANCELLED,
	SIR_IO_SHORT_READ,
	SIR_IO_READ_FAILED,
	SIR_IO_SHUTTING_DOWN
} sirIOResult_t;

typedef struct sirIORequestHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirIORequestHandle_t;

typedef struct sirIOSourceHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirIOSourceHandle_t;

typedef struct sirIOBufferHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirIOBufferHandle_t;

typedef struct sirIOReadRequest_s
{
	sirIOSourceHandle_t hSource;
	sirIOBufferHandle_t hBuffer;
	uint64_t nOffset;
	uint64_t nDestinationOffset;
	uint64_t nBytes;
	uint64_t nDeadlineTick;
	uint64_t nClientToken;
	uint32_t nPriority;
} sirIOReadRequest_t;

typedef struct sirIOCompletion_s
{
	sirIORequestHandle_t hRequest;
	sirIOSourceHandle_t hSource;
	sirIOBufferHandle_t hBuffer;
	sirIOResult_t eResult;
	uint64_t nTransferred;
	uint64_t nClientToken;
	uint64_t nSequence;
} sirIOCompletion_t;

struct sirIOSourceSlot_s;
struct sirIOBufferSlot_s;
struct sirIORequestSlot_s;

class SIRAsyncIOService final
{
private:
	SIRMutex m_Mutex;
	SIRConditionVariable m_Ready;
	SIRThread m_Worker;
	sirIOSourceSlot_s *m_pSources;
	sirIOBufferSlot_s *m_pBuffers;
	sirIORequestSlot_s *m_pRequests;
	uint32_t m_nSourceCapacity;
	uint32_t m_nBufferCapacity;
	uint32_t m_nRequestCapacity;
	uint64_t m_nCompletionSequence;
	bool m_bInitialized;
	bool m_bClosing;

	static void workerEntry( void *pContext ) noexcept;
	void workerLoop( void ) noexcept;
	uint32_t selectQueuedLocked( void ) const noexcept;
	bool hasRunningOrQueuedLocked( void ) const noexcept;
	bool validSourceLocked( sirIOSourceHandle_t hSource ) const noexcept;
	bool validBufferLocked( sirIOBufferHandle_t hBuffer ) const noexcept;
	bool validRequestLocked( sirIORequestHandle_t hRequest ) const noexcept;

public:
	SIRAsyncIOService( void ) noexcept;
	~SIRAsyncIOService( void ) noexcept;
	SIRAsyncIOService( const SIRAsyncIOService& ) = delete;
	SIRAsyncIOService& operator=( const SIRAsyncIOService& ) = delete;

	static uint64_t requiredBytes( uint32_t nRequests, uint32_t nSources, uint32_t nBuffers ) noexcept;
	bool initialize( void *pStorage, uint64_t nStorageBytes, uint32_t nRequests, uint32_t nSources, uint32_t nBuffers ) noexcept;
	sirIOResult_t openSource( const char *szPath, sirIOSourceHandle_t *pHandle ) noexcept;
	sirIOResult_t closeSource( sirIOSourceHandle_t hSource ) noexcept;
	// Bounded index/header inspection on the exact opened physical handle;
	// intended for startup SIRPack mount validation, not frame-time reads.
	sirIOResult_t inspectSource( sirIOSourceHandle_t hSource, void *pHeader, uint32_t nHeaderBytes, uint64_t *pSize ) noexcept;
	sirIOResult_t registerBuffer( void *pData, uint64_t nBytes, sirIOBufferHandle_t *pHandle ) noexcept;
	sirIOResult_t unregisterBuffer( sirIOBufferHandle_t hBuffer ) noexcept;
	sirIOResult_t submit( const sirIOReadRequest_t& request, sirIORequestHandle_t *pHandle ) noexcept;
	sirIOResult_t cancel( sirIORequestHandle_t hRequest ) noexcept;
	uint32_t poll( sirIOCompletion_t *pCompletions, uint32_t nCapacity ) noexcept;
	sirIOResult_t acknowledge( sirIORequestHandle_t hRequest ) noexcept;
	// Calls worker join and invalidates handles. Waits for native reads to finish.
	void shutdown( void ) noexcept;
};

static_assert( sizeof( sirIORequestHandle_t ) == 8, "IO request handle must be generation safe" );
static_assert( sizeof( sirIOBufferHandle_t ) == 8, "IO buffer handle must be generation safe" );
static_assert( sizeof( sirIOSourceHandle_t ) == 8, "IO source handle must be generation safe" );

#endif // __SIRENGINE_SIRASYNCIOSERVICE_H__
