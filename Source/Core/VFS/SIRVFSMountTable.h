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

#ifndef __SIRENGINE_SIRVFSMOUNTTABLE_H__
#define __SIRENGINE_SIRVFSMOUNTTABLE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRSync.h>
#include <Core/VFS/SIRVFSPath.h>

constexpr uint32_t SIR_VFS_MAX_MOUNTS = 32;
constexpr uint32_t SIR_VFS_MAX_LEASES = 128;

typedef enum sirVFSResult_e : uint8_t
{
	SIR_VFS_OK = 0,
	SIR_VFS_INVALID_ARGUMENT,
	SIR_VFS_FORBIDDEN,
	SIR_VFS_NOT_FOUND,
	SIR_VFS_NO_CAPACITY,
	SIR_VFS_STALE_HANDLE,
	SIR_VFS_IO_ERROR,
	SIR_VFS_BUSY,
	SIR_VFS_STOPPED
} sirVFSResult_t;

typedef enum sirVFSBackendKind_e : uint8_t
{
	SIR_VFS_BACKEND_DIRECTORY = 0,
	SIR_VFS_BACKEND_SIRPACK,
	SIR_VFS_BACKEND_MEMORY,
	SIR_VFS_BACKEND_GENERATED
} sirVFSBackendKind_t;

typedef enum sirVFSAccess_e : uint32_t
{
	SIR_VFS_ACCESS_READ = 1u << 0,
	SIR_VFS_ACCESS_WRITE = 1u << 1,
	SIR_VFS_ACCESS_ENUMERATE = 1u << 2
} sirVFSAccess_t;

typedef struct sirVFSMountDescriptor_s
{
	char szNamespace[SIR_VFS_MAX_NAMESPACE_BYTES + 1];
	uint64_t nSourceId;
	uint64_t nSourceRevision;
	int32_t iPriority;
	uint32_t nAllowedOps;
	uint32_t nOwnerScope;
	sirVFSBackendKind_t eKind;
	bool bTrusted;
} sirVFSMountDescriptor_t;

typedef struct sirVFSMountHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirVFSMountHandle_t;

typedef struct sirVFSMountLease_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nToken;
} sirVFSMountLease_t;

typedef struct sirVFSMountCandidate_s
{
	sirVFSMountDescriptor_t descriptor;
	sirVFSMountLease_t lease;
} sirVFSMountCandidate_t;

typedef struct sirVFSCandidateList_s
{
	uint64_t nRevision;
	uint32_t nCount;
	sirVFSMountCandidate_t szCandidates[SIR_VFS_MAX_MOUNTS];
} sirVFSCandidateList_t;

typedef struct sirVFSMountSlot_s
{
	sirVFSMountDescriptor_t descriptor;
	uint64_t nSequence;
	uint32_t nGeneration;
	uint32_t nPins;
	bool bActive;
	bool bRetired;
} sirVFSMountSlot_t;

typedef struct sirVFSLeaseSlot_s
{
	uint64_t nToken;
	uint32_t nMountSlot;
	bool bActive;
} sirVFSLeaseSlot_t;

// Metadata-only registry: backend existence probes and physical I/O execute
// outside this lock. Acquired leases pin retired mount generations until release.
class SIRVFSMountTable final
{
public:
	SIRVFSMountTable( void ) noexcept;
	SIRVFSMountTable( uint32_t nMountCapacity, uint32_t nLeaseCapacity ) noexcept;
	~SIRVFSMountTable( void ) noexcept;

	SIRVFSMountTable( const SIRVFSMountTable& ) = delete;
	SIRVFSMountTable& operator=( const SIRVFSMountTable& ) = delete;

	uint64_t getRevision( void ) const noexcept;

	sirVFSResult_t mount( const sirVFSMountDescriptor_t& descriptor, sirVFSMountHandle_t *pHandle ) noexcept;
	sirVFSResult_t unmount( sirVFSMountHandle_t handle ) noexcept;
	sirVFSResult_t acquireCandidates( const sirVFSPath_t& path, uint32_t nOperation, sirVFSCandidateList_t *pCandidates ) noexcept;
	sirVFSResult_t acquireWriteTarget( const sirVFSPath_t& path, sirVFSMountHandle_t handle, sirVFSMountCandidate_t *pCandidate ) noexcept;
	sirVFSResult_t retain( sirVFSMountLease_t lease, sirVFSMountCandidate_t *pCandidate ) noexcept;
	sirVFSResult_t release( sirVFSMountLease_t lease ) noexcept;

protected:
private:
	uint32_t findFreeLease( void ) const noexcept;
	void retireSlot( uint32_t nIndex ) noexcept;

	mutable SIRMutex m_Mutex;
	sirVFSMountSlot_t m_szMounts[SIR_VFS_MAX_MOUNTS];
	sirVFSLeaseSlot_t m_szLeases[SIR_VFS_MAX_LEASES];
	uint32_t m_nMountCapacity;
	uint32_t m_nLeaseCapacity;
	uint64_t m_nRevision;
	uint64_t m_nSequence;
	uint64_t m_nNextToken;
};

#endif // __SIRENGINE_SIRVFSMOUNTTABLE_H__
