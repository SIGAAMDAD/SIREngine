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

#include <Core/Platform/SIRPlatformFileSystem.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

static void sirCheckFileSystem( bool bCondition, const char *pszMessage )
{
	if ( !bCondition ) {
		std::fprintf( stderr, "Filesystem test failed: %s\n", pszMessage );
		std::abort();
	}
}

typedef struct sirFileTestVisitor_s
{
	uint32_t nFound;
	bool bStop;
} sirFileTestVisitor_t;

static bool sirVisitFile( const char *pszName, const sirFileSystemInfo_t *pInfo, void *pContext ) noexcept
{
	sirFileTestVisitor_t *pVisitor = ( sirFileTestVisitor_t * )pContext;
	if ( std::strcmp( pszName, "dest.bin" ) == 0 && pInfo->eType == SIR_FILESYSTEM_REGULAR_FILE ) {
		++pVisitor->nFound;
	}
	return !pVisitor->bStop;
}

int main( void )
{
	const std::filesystem::path pathRoot = std::filesystem::current_path() / "sir-filesystem-test";
	std::filesystem::remove_all( pathRoot );
	std::filesystem::path pathChild = pathRoot / "one" / "two";
	const std::string szRoot = pathRoot.string();
	const std::string szChild = pathChild.string();
	sirCheckFileSystem( SIRPlatformFileSystem::createDirectory( nullptr, false ) == SIR_FILESYSTEM_INVALID_ARGUMENT, "null path" );
	sirCheckFileSystem( SIRPlatformFileSystem::createDirectory( szChild.c_str(), true ) == SIR_FILESYSTEM_OK, "recursive mkdir" );
	sirFileSystemInfo_t info{};
	sirCheckFileSystem( SIRPlatformFileSystem::query( szChild.c_str(), &info ) == SIR_FILESYSTEM_OK &&
		info.eType == SIR_FILESYSTEM_DIRECTORY, "directory stat" );
	sirCheckFileSystem( SIRPlatformFileSystem::createDirectory( szChild.c_str(), false ) == SIR_FILESYSTEM_ALREADY_EXISTS, "existing directory" );
	std::filesystem::path pathSource = pathChild / "source.bin";
	std::filesystem::path pathDestination = pathChild / "dest.bin";
	const std::string szSource = pathSource.string();
	const std::string szDestination = pathDestination.string();
	{
		FILE *pFile = std::fopen( szSource.c_str(), "wb" );
		sirCheckFileSystem( pFile != nullptr, "create test file" );
		sirCheckFileSystem( std::fwrite( "TEST", 1, 4, pFile ) == 4, "write test file" );
		std::fclose( pFile );
	}
	sirCheckFileSystem( SIRPlatformFileSystem::query( szSource.c_str(), &info ) == SIR_FILESYSTEM_OK &&
		info.eType == SIR_FILESYSTEM_REGULAR_FILE && info.nSizeBytes == 4, "file stat" );
	{
		FILE *pFile = std::fopen( szDestination.c_str(), "wb" );
		sirCheckFileSystem( pFile != nullptr, "create replacement target" );
		std::fclose( pFile );
	}
	sirCheckFileSystem( SIRPlatformFileSystem::renameReplace( szSource.c_str(), szDestination.c_str() ) == SIR_FILESYSTEM_OK,
		"replace destination" );
	sirCheckFileSystem( SIRPlatformFileSystem::query( szDestination.c_str(), &info ) == SIR_FILESYSTEM_OK && info.nSizeBytes == 4,
		"replacement contents" );
	sirCheckFileSystem( SIRPlatformFileSystem::query( szSource.c_str(), &info ) == SIR_FILESYSTEM_NOT_FOUND, "source renamed" );
	sirFileTestVisitor_t visitor{};
	sirCheckFileSystem( SIRPlatformFileSystem::enumerate( szChild.c_str(), &sirVisitFile, &visitor ) == SIR_FILESYSTEM_OK &&
		visitor.nFound == 1, "enumeration" );
	visitor.bStop = true;
	sirCheckFileSystem( SIRPlatformFileSystem::enumerate( szChild.c_str(), &sirVisitFile, &visitor ) == SIR_FILESYSTEM_STOPPED,
		"early visitor stop" );
	sirCheckFileSystem( SIRPlatformFileSystem::removeDirectory( szChild.c_str() ) != SIR_FILESYSTEM_OK,
		"reject nonempty directory" );
	std::filesystem::remove( pathDestination );
	sirCheckFileSystem( SIRPlatformFileSystem::removeDirectory( szChild.c_str() ) == SIR_FILESYSTEM_OK, "remove empty child" );
	std::filesystem::remove_all( pathRoot );
	return 0;
}
