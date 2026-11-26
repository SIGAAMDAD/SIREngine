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

#ifndef __SIR_SCRIPTOPTIMIZERPROCESSTRANSPORT_H__
#define __SIR_SCRIPTOPTIMIZERPROCESSTRANSPORT_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Scripting/Core/SIRScriptOptimizationCoordinator.h>
#include <atomic>
#include <chrono>
#include <Core/Platform/Shared/Threading/SIRSync.h>

#include <Core/Platform/Shared/Threading/SIRThread.h>

// Paths and arguments are copied at configuration time. The host owns
// executable and cache-input staging; no shell is involved in launching.
static constexpr uint32_t SIR_SCRIPT_OPT_MAX_ARGUMENTS = 8;
static constexpr uint32_t SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES = 16;

typedef struct sirScriptOptimizerProcessConfig_s
{
	char szExecutable[512];
	char szArguments[SIR_SCRIPT_OPT_MAX_ARGUMENTS][256];
	uint32_t nArgumentCount;
	uint32_t nWatchdogMilliseconds;
} sirScriptOptimizerProcessConfig_t;

// Separately hosted optimizer transport. Two I/O threads isolate blocking
// anonymous pipes from owner-thread coordinator callbacks. One queue entry
// always represents a complete canonical request or reply wire record.
class SIRScriptOptimizerProcessTransport
{
public:
	SIRScriptOptimizerProcessTransport( void ) noexcept;
	~SIRScriptOptimizerProcessTransport( void ) noexcept;

	SIRScriptOptimizerProcessTransport( const SIRScriptOptimizerProcessTransport& ) = delete;
	SIRScriptOptimizerProcessTransport& operator=( const SIRScriptOptimizerProcessTransport& ) = delete;

	sirScriptOptimizationTransportAPI_t getAPI( void ) noexcept;
	bool isRunning( void ) const noexcept;
	bool hasFailed( void ) const noexcept;

	SIRScriptResult configure( const sirScriptOptimizerProcessConfig_t& config ) noexcept;
	SIRScriptResult start( void ) noexcept;
	void stop( void ) noexcept;
	SIRScriptResult send( const uint8_t *pWire, uint32_t nBytes ) noexcept;
	SIRScriptResult receive( uint8_t *pWire, uint32_t nCapacity, uint32_t *pReceived ) noexcept;

protected:
	// Process handles are exclusively managed by this transport.

private:
	static SIRScriptResult apiStart( void *pUser ) noexcept;
	static void apiStop( void *pUser ) noexcept;
	static SIRScriptResult apiSend( void *pUser, const uint8_t *pWire, uint32_t nBytes ) noexcept;
	static SIRScriptResult apiReceive( void *pUser, uint8_t *pWire, uint32_t nCapacity, uint32_t *pReceived ) noexcept;

	bool spawn( void ) noexcept;
	void terminateChild( void ) noexcept;
	void closePipes( void ) noexcept;
	bool writeFrame( const uint8_t *pData, uint32_t nBytes ) noexcept;
	bool readFrame( uint8_t *pData, uint32_t nBytes ) noexcept;
	static void writerEntry( void *pUserData ) noexcept;
	static void readerEntry( void *pUserData ) noexcept;
	void writerMain( void ) noexcept;
	void readerMain( void ) noexcept;
	void failTransport( void ) noexcept;

	sirScriptOptimizerProcessConfig_t m_Config;
	const sirThreadId_t m_OwnerThread;
	SIRThread m_Writer;
	SIRThread m_Reader;
	SIRMutex m_Mutex;
	SIRConditionVariable m_CV;
	std::atomic<bool> m_bStopping;
	std::atomic<bool> m_bFailed;
	std::chrono::steady_clock::time_point m_LastActivity;
	uint8_t m_szRequests[SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES][SIRScriptOptimizationProtocol::REQUEST_BYTES];
	uint8_t m_szReplies[SIR_SCRIPT_OPT_MAX_QUEUED_FRAMES][SIRScriptOptimizationProtocol::REPLY_BYTES];
	uint32_t m_nRequestHead;
	uint32_t m_nRequestTail;
	uint32_t m_nRequestCount;
	uint32_t m_nReplyHead;
	uint32_t m_nReplyTail;
	uint32_t m_nReplyCount;
	uint32_t m_nOutstanding;
	bool m_bConfigured;
	bool m_bRunning;
#if defined( _WIN32 )
	void *m_hChildProcess;
	void *m_hReadPipe;
	void *m_hWritePipe;
#else
	int m_nChildPid;
	int m_nReadFd;
	int m_nWriteFd;
#endif
};

#endif // __SIR_SCRIPTOPTIMIZERPROCESSTRANSPORT_H__
