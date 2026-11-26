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

#ifndef __SIR_VFSFILEHANDLETABLE_H__
#define __SIR_VFSFILEHANDLETABLE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/VFS/SIRVFSMountTable.h>

constexpr uint32_t SIR_VFS_MAX_OPEN_FILES = 64;
constexpr uint32_t SIR_VFS_MAX_FILE_BORROWS = 128;

typedef struct sirVFSFileHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirVFSFileHandle_t;

typedef struct sirVFSFileBorrow_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nToken;
} sirVFSFileBorrow_t;

typedef struct sirVFSFileView_s
{
	sirVFSMountDescriptor_t descriptor;
	sirVFSPath_t path;
	sirVFSFileBorrow_t borrow;
	sirVFSMountHandle_t sourceMount;
} sirVFSFileView_t;

typedef struct sirVFSFileSlot_s
{
	sirVFSMountDescriptor_t descriptor;
	sirVFSMountLease_t mountLease;
	sirVFSPath_t path;
	uint32_t nGeneration;
	uint32_t nBorrows;
	bool bOpen;
	bool bClosing;
} sirVFSFileSlot_t;

typedef struct sirVFSFileBorrowSlot_s
{
	uint64_t nToken;
	uint32_t nFileSlot;
	bool bActive;
} sirVFSFileBorrowSlot_t;

// Logical source identity only: native file descriptors and I/O requests belong
// to Core/Platform and Core/IO. Every borrowed view pins the immutable source
// generation even after close or remount, until releaseBorrow acknowledges it.
class SIRVFSFileHandleTable final
{
public:
	SIRVFSFileHandleTable( SIRVFSMountTable& table, uint32_t nFileCapacity, uint32_t nBorrowCapacity ) noexcept;
	~SIRVFSFileHandleTable( void ) noexcept;

	SIRVFSFileHandleTable( const SIRVFSFileHandleTable& ) = delete;
	SIRVFSFileHandleTable& operator=( const SIRVFSFileHandleTable& ) = delete;

	bool usesMountTable( const SIRVFSMountTable& table ) const noexcept;

	SIRVFSResult openResolved( const sirVFSPath_t& path, sirVFSMountLease_t sourceLease, sirVFSFileHandle_t *pHandle ) noexcept;
	SIRVFSResult borrow( sirVFSFileHandle_t handle, sirVFSFileView_t *pView ) noexcept;
	SIRVFSResult retainBorrow( sirVFSFileBorrow_t borrow, sirVFSFileView_t *pView ) noexcept;
	SIRVFSResult releaseBorrow( sirVFSFileBorrow_t borrow ) noexcept;
	SIRVFSResult close( sirVFSFileHandle_t handle ) noexcept;
	SIRVFSResult shutdown( void ) noexcept;

protected:
private:
	uint32_t findFreeFile( void ) const noexcept;
	uint32_t findFreeBorrow( void ) const noexcept;
	void retireFile( uint32_t nIndex ) noexcept;

	SIRVFSMountTable& m_Table;
	mutable SIRMutex m_Mutex;
	sirVFSFileSlot_t m_szFiles[SIR_VFS_MAX_OPEN_FILES];
	sirVFSFileBorrowSlot_t m_szBorrows[SIR_VFS_MAX_FILE_BORROWS];
	uint32_t m_nFileCapacity;
	uint32_t m_nBorrowCapacity;
	uint64_t m_nNextToken;
	bool m_bStopped;
};

#endif // __SIR_VFSFILEHANDLETABLE_H__
