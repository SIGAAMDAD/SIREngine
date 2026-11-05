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

#ifndef __SIRENGINE_SIRCONSOLESERVICE_H__
#define __SIRENGINE_SIRCONSOLESERVICE_H__

#include <Core/CVars/SIRCVarRegistry.h>
#include <SIRPCH.hpp>
#include <atomic>
#include <thread>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

constexpr uint32_t SIR_CONSOLE_MAX_ARGS = 16;

typedef struct sirConsoleArgument_s
{
	const char *pszText;
	uint32_t nBytes;
} sirConsoleArgument_t;

typedef struct sirConsoleArguments_s
{
	uint32_t nCount;
	sirConsoleArgument_t szArgs[SIR_CONSOLE_MAX_ARGS];
} sirConsoleArguments_t;

typedef struct sirConsoleSubmission_s
{
	sirCVarOrigin_t origin;
	uint64_t nRequestId;
} sirConsoleSubmission_t;

typedef struct sirConsoleOutput_s
{
	uint64_t nRequestId;
	sirRegistrationResult_t eResult;
	const char *pszText;
	uint32_t nBytes;
} sirConsoleOutput_t;

typedef void ( *sirConsoleOutputSink_t )( void *pContext, const sirConsoleOutput_t& output ) noexcept;
typedef bool ( *sirConsoleModuleAcquire_t )( void *pContext, uint32_t nOwnerId ) noexcept;
typedef void ( *sirConsoleModuleRelease_t )( void *pContext, uint32_t nOwnerId ) noexcept;

class SIRConsoleService;

typedef struct sirConsoleInvocation_s
{
	SIRConsoleService *pConsole;
	sirConsoleSubmission_t submission;
} sirConsoleInvocation_t;

// Per-slot publication uses the bounded lock-free MPSC sequence protocol.
// Slots must be constructed by the caller; no memset of atomic objects.
typedef struct sirConsoleRequestSlot_s
{
	std::atomic<uint64_t> nSequence{ 0 };
	sirConsoleSubmission_t submission = {};
	uint32_t nBytes = 0;
} sirConsoleRequestSlot_t;

typedef struct sirConsoleHistoryEntry_s
{
	uint64_t nRequestId;
	uint32_t nBytes;
} sirConsoleHistoryEntry_t;

typedef struct sirConsoleCompletionCandidate_s
{
	char szName[SIR_REGISTRATION_MAX_NAME_BYTES + 1];
} sirConsoleCompletionCandidate_t;

typedef struct sirConsoleCVarBinding_s
{
	sirRegistrationHandle_t registration;
	sirCVarHandle_t cvar;
} sirConsoleCVarBinding_t;

typedef struct sirConsoleStorage_s
{
	sirConsoleRequestSlot_t *pRequests;
	uint32_t nRequestCapacity;
	char *pszRequestText;
	size_t nRequestTextBytes;
	uint32_t nInputStride;
	sirConsoleHistoryEntry_t *pHistory;
	uint32_t nHistoryCapacity;
	char *pszHistoryText;
	size_t nHistoryTextBytes;
	sirConsoleCompletionCandidate_t *pCompletionScratch;
	uint32_t nCompletionCapacity;
	const sirConsoleCVarBinding_t *pCVarBindings;
	uint32_t nCVarBindings;
	uint32_t nMaxCommandsPerPump;
	uint32_t nMaxOutputsPerCommand;
	sirConsoleOutputSink_t pfnOutput;
	void *pOutputContext;
	sirConsoleModuleAcquire_t pfnAcquireModule;
	sirConsoleModuleRelease_t pfnReleaseModule;
	void *pModuleContext;
} sirConsoleStorage_t;

typedef struct sirConsolePumpReport_s
{
	uint32_t nProcessed;
	uint32_t nSucceeded;
	uint32_t nRejected;
	uint32_t nOutputs;
	uint32_t nOutputOverflow;
} sirConsolePumpReport_t;

typedef struct sirConsoleCompletionResult_s
{
	char *pszOutput;
	uint32_t nCapacity;
	uint32_t nCapabilities;
	uint32_t nWritten;
	uint32_t nCount;
} sirConsoleCompletionResult_t;

typedef struct sirConsoleHistoryView_s
{
	char *pszOutput;
	uint32_t nCapacity;
	uint32_t nWritten;
	uint64_t nRequestId;
} sirConsoleHistoryView_t;

class SIRConsoleService
{
public:
	SIRConsoleService( void ) noexcept;
	SIRConsoleService( const SIRConsoleService& ) = delete;
	~SIRConsoleService( void ) noexcept;

	SIRConsoleService& operator=( const SIRConsoleService& ) = delete;

	bool isInitialized( void ) const noexcept;
	uint32_t getHistoryCount( void ) const noexcept;

	sirRegistrationResult_t initialize( SIRRegistrationCatalog& registration, SIRCVarRegistry& cvars, SIRStringPool& pool,
	                                    const sirConsoleStorage_t& storage ) noexcept;
	sirRegistrationResult_t submit( const char *pszInput, uint32_t nBytes, const sirConsoleSubmission_t& submission ) noexcept;
	sirRegistrationResult_t pump( uint32_t nBudget, sirConsolePumpReport_t *pReport ) noexcept;
	sirRegistrationResult_t complete( const char *pszPrefix, uint32_t nBytes, sirConsoleCompletionResult_t *pResult ) noexcept;
	sirRegistrationResult_t readHistory( uint32_t iNewest, sirConsoleHistoryView_t *pResult ) const noexcept;
	sirRegistrationResult_t registerDynamicCommand( const sirRegistrationDynamicDesc_t& descriptor,
	                                                sirRegistrationHandle_t *pHandle ) noexcept;
	sirRegistrationResult_t unregisterDynamicCommand( sirRegistrationHandle_t handle ) noexcept;
	sirRegistrationResult_t unloadModule( uint32_t nOwnerId, uint32_t *pRemoved ) noexcept;
	sirRegistrationResult_t writeOutput( const sirConsoleOutput_t& output ) noexcept;
	void shutdown( void ) noexcept;

protected:
private:
	sirRegistrationResult_t tokenize( char *pszInput, uint32_t nBytes, sirConsoleArguments_t *pArgs ) const noexcept;
	sirRegistrationResult_t execute( const sirConsoleSubmission_t& submission, sirConsoleArguments_t& args ) noexcept;
	sirRegistrationResult_t executeBuiltin( const sirConsoleSubmission_t& submission, sirConsoleArguments_t& args ) noexcept;
	sirRegistrationResult_t executeCVar( const sirConsoleSubmission_t& submission, sirConsoleArguments_t& args,
	                                     const char *pszOperation ) noexcept;
	sirRegistrationResult_t parseValue( sirCVarType_t eType, const sirConsoleArgument_t& arg, sirCVarValue_t *pValue ) const noexcept;
	sirRegistrationResult_t printValue( const sirConsoleSubmission_t& submission, sirCVarType_t eType,
	                                    const sirCVarValue_t& value ) noexcept;
	sirRegistrationResult_t enumerate( const sirConsoleSubmission_t& submission, bool bCommandsOnly ) noexcept;
	sirRegistrationResult_t outputText( uint64_t nRequestId, sirRegistrationResult_t eResult, const char *pszText ) noexcept;
	const sirConsoleCVarBinding_t *findBoundCVar( sirRegistrationHandle_t registration ) const noexcept;
	bool ownerThread( void ) const noexcept;
	void addHistory( const sirConsoleRequestSlot_t& slot, const char *pszInput ) noexcept;

	SIRRegistrationCatalog *m_pRegistration;
	SIRCVarRegistry *m_pCVars;
	SIRStringPool *m_pPool;
	sirConsoleStorage_t m_Storage;
	std::atomic<uint64_t> m_nWriteTicket;
	std::atomic<bool> m_bAccepting;
	std::thread::id m_OwnerThread;
	uint64_t m_nReadTicket;
	uint32_t m_nHistoryCount;
	uint32_t m_nHistoryNext;
	uint32_t m_nOutputsCurrent;
	uint32_t m_nOutputOverflowCurrent;
	uint64_t m_nCurrentRequestId;
	bool m_bPumping;
};

#endif // __SIRENGINE_SIRCONSOLESERVICE_H__
