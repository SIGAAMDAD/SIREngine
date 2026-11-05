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

#ifndef __SIRENGINE_SIRPLATFORMROOTDIRECTORY_H__
#define __SIRENGINE_SIRPLATFORMROOTDIRECTORY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRPlatformFile.h>

constexpr uint32_t SIR_PLATFORM_ROOTED_MAX_RELATIVE_BYTES = 1024;

// The root is a trusted, pre-authorized directory opened once. Relative
// components are opened by OS handles, never by recombining absolute paths.
// Roots and returned files are separately owned; each must be synchronized
// against concurrent close by its owner.
class SIRPlatformRootDirectory final
{
public:
	SIRPlatformRootDirectory( void ) noexcept;
	~SIRPlatformRootDirectory( void ) noexcept;

	SIRPlatformRootDirectory( const SIRPlatformRootDirectory& ) = delete;
	SIRPlatformRootDirectory& operator=( const SIRPlatformRootDirectory& ) = delete;

	bool isOpen( void ) const noexcept;

	sirFileResult_t openRoot( const char *pszRootPath ) noexcept;
	sirFileResult_t openRead( const char *pszRelativePath, SIRPlatformFile *pFile ) const noexcept;
	sirFileResult_t close( void ) noexcept;

protected:
private:
	static bool validRelative( const char *pszRelativePath ) noexcept;

	uintptr_t m_nNativeDirectory;
};

#endif // __SIRENGINE_SIRPLATFORMROOTDIRECTORY_H__
