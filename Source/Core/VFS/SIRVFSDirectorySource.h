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

#ifndef __SIRENGINE_SIRVFSDIRECTORYSOURCE_H__
#define __SIRENGINE_SIRVFSDIRECTORYSOURCE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRPlatformRootDirectory.h>
#include <Core/Platform/SIRSync.h>
#include <Core/VFS/SIRVFSFileHandleTable.h>

// Binds one immutable VFS source generation to one OS-anchored directory.
// Directory roots must remain alive through all intended opens from that
// generation. Native files returned from openRead own independent OS handles.
class SIRVFSDirectorySource final
{
public:
	SIRVFSDirectorySource( void ) noexcept;
	~SIRVFSDirectorySource( void ) noexcept;

	SIRVFSDirectorySource( const SIRVFSDirectorySource& ) = delete;
	SIRVFSDirectorySource& operator=( const SIRVFSDirectorySource& ) = delete;

	bool isOpen( void ) const noexcept;

	sirVFSResult_t bind( const char *pszRootPath, const sirVFSMountDescriptor_t& descriptor ) noexcept;
	sirVFSResult_t openRead( SIRVFSFileHandleTable& files, sirVFSFileBorrow_t ticket, SIRPlatformFile *pFile ) noexcept;
	sirVFSResult_t close( void ) noexcept;

protected:
private:
	static sirVFSResult_t fileResult( sirFileResult_t eResult ) noexcept;

	mutable SIRMutex m_Mutex;
	SIRPlatformRootDirectory m_Root;
	sirVFSMountDescriptor_t m_Descriptor;
	bool m_bReady;
	bool m_bRetired;
};

#endif // __SIRENGINE_SIRVFSDIRECTORYSOURCE_H__
