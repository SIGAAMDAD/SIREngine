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

#ifndef __SIRENGINE_SIRRESOURCEFILELOAD_H__
#define __SIRENGINE_SIRRESOURCEFILELOAD_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Resources/SIRResourceGraph.h>

typedef enum sirResourceFileReadMode_e : uint8_t
{
	SIR_RESOURCE_FILE_READ_AT = 0,
	SIR_RESOURCE_FILE_READ_MAPPED = 1
} sirResourceFileReadMode_t;

// One cooked file interval becomes one canonical GRDG chunk. Dependencies
// are borrowed until poll() finishes the load. Source files must remain
// stable until the Core Jobs task reaches a terminal state.
typedef struct sirResourceFileLoadRequest_s
{
	SIRResourceKey keyResource;
	SIRResourceContentKey keyContent;
	SIRResourceChunkId idChunk;
	SIRResourceKind eKind;
	sirResourceFileReadMode_t eMode;
	uint16_t nSchemaVersion;
	uint32_t nFormatId;
	uint32_t nAlignment;
	uint32_t nStagingDomain;
	uint64_t nFileOffset;
	uint64_t nFileBytes;
	uint64_t nExpectedRevision;
	const SIRResourceDependency *pDependencies;
	uint32_t nDependencyCount;
} sirResourceFileLoadRequest_t;

// A single in-flight transfer, not a scheduler. Core Jobs owns execution,
// cancellation and completion. The GRDG writer owns staging and publication.
// One object per concurrent read; no extra CPU payload allocation or copy.
// The graph, scheduler and request dependencies must outlive an active load.
// Destroy only after poll() has released the terminal task.
class SIRResourceFileLoad final
{
public:
	SIRResourceFileLoad( SIRResourceGraph &rGraph, SIRTaskScheduler &rScheduler ) noexcept;
	~SIRResourceFileLoad( void ) noexcept;
	SIRResourceFileLoad( const SIRResourceFileLoad & ) = delete;
	SIRResourceFileLoad &operator=( const SIRResourceFileLoad & ) = delete;

	SIRResourceResult begin( const char *pszUtf8Path, const sirResourceFileLoadRequest_t &rRequest ) noexcept;
	// Core Jobs handle can be passed to waitTask() when worker threads exist.
	bool getTaskHandle( sirTaskHandle_t *pHandle ) const noexcept;
	// Returns BUSY until terminal. The writer publishes only on full success.
	// All terminal paths free staging and release the Core Jobs task handle.
	SIRResourceResult poll( SIRResourceHandle *pResource ) noexcept;
	// Requests Core Jobs cancellation; poll() retires resources once terminal.
	SIRResourceResult cancel( void ) noexcept;
	bool isActive( void ) const noexcept;
	// Meaningful after begin() rejects a file or poll() returns terminal.
	sirFileResult_t getLastFileResult( void ) const noexcept;

private:
	static sirTaskResult_t readTask( void *pContext, sirTaskHandle_t hTask ) noexcept;
	void clearStaging( void ) noexcept;
	void closeTransfer( void ) noexcept;

	SIRResourceGraph *m_pGraph;
	SIRTaskScheduler *m_pScheduler;
	SIRPlatformFile m_File;
	sirResourceFileLoadRequest_t m_Request;
	SIRResourceStagingHandle m_Staging;
	SIRResourceMutableBytes m_Bytes;
	sirTaskHandle_t m_Task;
	sirFileResult_t m_eTransferResult;
	sirFileResult_t m_eLastFileResult;
	bool m_bActive;
	bool m_bDiscard;
};

#endif // __SIRENGINE_SIRRESOURCEFILELOAD_H__
