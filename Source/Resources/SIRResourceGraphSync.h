/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIR_RESOURCE_GRAPH_SYNC_H__
#define __SIR_RESOURCE_GRAPH_SYNC_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>
#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <stdint.h>
#include <thread>

// GRDG-specific synchronization. The graph owns the epoch array allocation;
// this object exclusively owns its reader/writer synchronization protocol.
// No reader ever acquires the writer guard or blocks on the writer thread.
class SIRResourceGraphSync
{
private:
	std::thread::id m_OwnerThread;
	std::atomic<uint64_t> *m_pReaderEpochs;
	std::atomic<uint64_t> m_nPublicationEpoch;
	uint32_t m_nMaxReaders;
	bool m_bWriterActive;

	void announceEpoch( uint32_t iReader ) const noexcept;

public:
	SIRResourceGraphSync( void ) noexcept;
	SIRResourceGraphSync( const SIRResourceGraphSync& ) = delete;
	SIRResourceGraphSync& operator=( const SIRResourceGraphSync& ) = delete;

	void configure( std::atomic<uint64_t> *pReaderEpochs, uint32_t nMaxReaders ) noexcept;
	bool isWriterThread( void ) const noexcept;
	bool beginWrite( void ) noexcept;
	void endWrite( void ) noexcept;
	bool isWriteActive( void ) const noexcept;

	uint64_t currentEpoch( void ) const noexcept;
	void advanceEpoch( uint64_t nPreviousEpoch ) noexcept;
	uint64_t oldestProtectedEpoch( void ) const noexcept;
	uint32_t countActiveReaders( void ) const noexcept;
	uint32_t maxReaders( void ) const noexcept;
	bool readersQuiescent( void ) const noexcept;

	uint64_t registerLane( void ) const noexcept;
	void releaseLane( uint64_t nReaderToken ) const noexcept;
	uint64_t enterRegisteredLane( uint64_t nReaderToken ) const noexcept;
	uint64_t enterRead( void ) const noexcept;
	void leaveRead( uint64_t nReaderToken ) const noexcept;
};

// GRDG provider-pin bookkeeping only. Never held while destroying a
// resource payload or entering a read scope. Uses the native blocking mutex
// on both Linux and Windows; spin tuning is deliberately not forced here.
class SIRResourceGraphPinMutex
{
private:
	SIRMutex m_Mutex;

public:
	SIRResourceGraphPinMutex( void ) noexcept;
	SIRResourceGraphPinMutex( const SIRResourceGraphPinMutex& ) = delete;
	SIRResourceGraphPinMutex& operator=( const SIRResourceGraphPinMutex& ) = delete;
	void lock( void );
	bool try_lock( void );
	void unlock( void ) noexcept;
};

// Exclusive authority to mutate GRDG's writer-owned metadata. It is not a
// shared lock: immutable readers continue in parallel with publication.
class SIRResourceGraphWriteGuard
{
private:
	SIRResourceGraphSync *m_pSync;

public:
	explicit SIRResourceGraphWriteGuard( SIRResourceGraphSync& rSync ) noexcept;
	~SIRResourceGraphWriteGuard( void ) noexcept;
	SIRResourceGraphWriteGuard( const SIRResourceGraphWriteGuard& ) = delete;
	SIRResourceGraphWriteGuard& operator=( const SIRResourceGraphWriteGuard& ) = delete;
	bool isActive( void ) const noexcept;
};

#endif
