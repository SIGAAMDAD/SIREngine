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

#include <Tools/ShaderCooker/Frontend/SIRShaderFrontend.h>
#include <Tools/ShaderCooker/Targets/SIRShaderTargets.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined( _WIN32 )
#include <process.h>
#else
#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static uint64_t sirCookFileFingerprint( const std::filesystem::path& sFile )
{
	std::ifstream stream( sFile, std::ios::binary );
	if ( !stream ) {
		return 0;
	}
	uint64_t nHash = 14695981039346656037ULL;
	uint64_t nBytes = 0;
	std::array<char, 65536> szBuffer = {};
	while ( stream ) {
		stream.read( szBuffer.data(), szBuffer.size() );
		const std::streamsize nCount = stream.gcount();
		for ( std::streamsize iByte = 0; iByte < nCount; ++iByte ) {
			nHash = ( nHash ^ ( unsigned char )szBuffer[iByte] ) * 1099511628211ULL;
		}
		nBytes += ( uint64_t )nCount;
	}
	return stream.bad() || nBytes == 0 ? 0 : nHash;
}

static int sirCookRunCompiler( const std::vector<std::string>& szArguments )
{
	if ( szArguments.empty() ) {
		return -1;
	}
	std::vector<const char *> szArgv;
	for ( const std::string& sArgument : szArguments ) {
		szArgv.push_back( sArgument.c_str() );
	}
	szArgv.push_back( nullptr );
#if defined( _WIN32 )
	return _spawnv( _P_WAIT, szArguments[0].c_str(), szArgv.data() );
#else
	const pid_t nPid = fork();
	if ( nPid == -1 ) {
		return -1;
	}
	if ( nPid == 0 ) {
		execv( szArguments[0].c_str(), const_cast<char *const *>( szArgv.data() ) );
		_exit( 127 );
	}
	int nStatus = 0;
	pid_t nWaited = 0;
	do {
		nWaited = waitpid( nPid, &nStatus, 0 );
	} while ( nWaited == -1 && errno == EINTR );
	return nWaited == nPid && WIFEXITED( nStatus ) ? WEXITSTATUS( nStatus ) : -1;
#endif
}

static bool sirCookValidateBinary( const std::filesystem::path& sBinary, sirGraphicsShaderFormat_t eFormat )
{
	std::error_code eError;
	const uintmax_t nBytes = std::filesystem::file_size( sBinary, eError );
	if ( eError || nBytes < ( eFormat == SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS ? 8U : 32U ) || nBytes > ( 64U << 20 ) ||
	     ( ( eFormat == SIR_GRAPHICS_SHADER_SPIRV || eFormat == SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS ) && nBytes % 4 != 0 ) ) {
		return false;
	}
	std::ifstream stream( sBinary, std::ios::binary );
	char szMagic[4] = {};
	stream.read( szMagic, sizeof( szMagic ) );
	if ( !stream.good() ) {
		return false;
	}
	if ( eFormat == SIR_GRAPHICS_SHADER_SPIRV ) {
		return std::memcmp( szMagic, "\x03\x02\x23\x07", 4 ) == 0;
	}
	if ( eFormat == SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS ) {
		const uint32_t nVersion = ( uint32_t )( uint8_t )szMagic[0] | ( ( uint32_t )( uint8_t )szMagic[1] << 8 ) |
		                          ( ( uint32_t )( uint8_t )szMagic[2] << 16 ) | ( ( uint32_t )( uint8_t )szMagic[3] << 24 );
		if ( ( nVersion >> 16 ) != 0xfffe && ( nVersion >> 16 ) != 0xffff ) {
			return false;
		}
		char szEnd[4] = {};
		stream.seekg( ( std::streamoff )nBytes - 4 );
		stream.read( szEnd, 4 );
		return stream.good() && std::memcmp( szEnd, "\xff\xff\x00\x00", 4 ) == 0;
	}
	return ( eFormat == SIR_GRAPHICS_SHADER_DXBC || eFormat == SIR_GRAPHICS_SHADER_DXIL ) && std::memcmp( szMagic, "DXBC", 4 ) == 0;
}

static bool sirCookParseCommand( int nArgs, const char *const *pszArgs, sirShaderCookRequest_t *pRequest, std::filesystem::path *pCompiler,
                                 std::filesystem::path *pOutput )
{
	for ( int iArg = 1; iArg < nArgs; ++iArg ) {
		const std::string sFlag = pszArgs[iArg];
		if ( iArg + 1 >= nArgs ) {
			return false;
		}
		const std::string sValue = pszArgs[++iArg];
		if ( sFlag == "--root" ) {
			pRequest->sRoot = sValue;
		} else if ( sFlag == "--source" ) {
			pRequest->sEntryFile = sValue;
		} else if ( sFlag == "--entry" ) {
			pRequest->sEntryPoint = sValue;
		} else if ( sFlag == "--compiler" ) {
			*pCompiler = sValue;
		} else if ( sFlag == "--output" ) {
			*pOutput = sValue;
		} else if ( sFlag == "--backend" ) {
			pRequest->eBackend = sValue == "vulkan"  ? SIR_GRAPHICS_BACKEND_VULKAN
			                     : sValue == "d3d12" ? SIR_GRAPHICS_BACKEND_D3D12
			                     : sValue == "d3d11" ? SIR_GRAPHICS_BACKEND_D3D11
			                     : sValue == "d3d10" ? SIR_GRAPHICS_BACKEND_D3D10
			                     : sValue == "d3d9"  ? SIR_GRAPHICS_BACKEND_D3D9
			                     : sValue == "d3d8"  ? SIR_GRAPHICS_BACKEND_D3D8
			                                         : SIR_GRAPHICS_BACKEND_AUTO;
		} else if ( sFlag == "--stage" ) {
			pRequest->nStage = sValue == "vertex"     ? SIR_GRAPHICS_STAGE_VERTEX
			                   : sValue == "fragment" ? SIR_GRAPHICS_STAGE_FRAGMENT
			                   : sValue == "compute"  ? SIR_GRAPHICS_STAGE_COMPUTE
			                   : sValue == "geometry" ? SIR_GRAPHICS_STAGE_GEOMETRY
			                   : sValue == "hull"     ? SIR_GRAPHICS_STAGE_TESS_CONTROL
			                   : sValue == "domain"   ? SIR_GRAPHICS_STAGE_TESS_EVALUATION
			                   : sValue == "mesh"     ? SIR_GRAPHICS_STAGE_MESH
			                   : sValue == "task"     ? SIR_GRAPHICS_STAGE_TASK
			                                          : 0;
		} else if ( sFlag == "--define" ) {
			const size_t iEqual = sValue.find( '=' );
			if ( iEqual == std::string::npos ) {
				return false;
			}
			pRequest->szDefines.push_back( { sValue.substr( 0, iEqual ), sValue.substr( iEqual + 1 ) } );
		} else {
			return false;
		}
	}
	return !pRequest->sRoot.empty() && !pRequest->sEntryFile.empty() && !pRequest->sEntryPoint.empty() && !pCompiler->empty() &&
	       !pOutput->empty() && pOutput->has_filename();
}

int main( int nArgs, const char *const *pszArgs )
{
	sirShaderCookRequest_t request = {};
	std::filesystem::path sCompiler;
	std::filesystem::path sOutput;
	if ( !sirCookParseCommand( nArgs, pszArgs, &request, &sCompiler, &sOutput ) ) {
		std::fprintf( stderr,
		              "Usage: SIRShaderCooker --root dir --source relative.hlsl --entry name --backend vulkan|d3d12|d3d11|d3d10|d3d9|d3d8 "
		              "--stage compute|vertex|fragment|geometry|hull|domain|mesh|task --compiler /abs/path/dxc-or-fxc "
		              "--output file [--define NAME=VALUE]\n" );
		return 2;
	}
	std::error_code eError;
	sCompiler = std::filesystem::canonical( sCompiler, eError );
	if ( eError || !std::filesystem::is_regular_file( sCompiler ) ) {
		std::fprintf( stderr, "ShaderCooker: offline DXC executable missing\n" );
		return 3;
	}
	const uint64_t nCompilerFingerprint = sirCookFileFingerprint( sCompiler );
	if ( nCompilerFingerprint == 0 ) {
		std::fprintf( stderr, "ShaderCooker: cannot fingerprint offline compiler\n" );
		return 3;
	}
	sirShaderCookManifest_t manifest = {};
	std::string sDiagnostic;
	const sirShaderCookResult_t eResult = sirShaderCookPreprocess( request, &manifest, &sDiagnostic );
	if ( eResult != SIR_SHADER_COOK_OK ) {
		std::fprintf( stderr, "ShaderCooker: %s (error %u)\n", sDiagnostic.c_str(), ( uint32_t )eResult );
		return 4;
	}
	std::filesystem::create_directories( sOutput.parent_path().empty() ? "." : sOutput.parent_path(), eError );
	if ( eError ) {
		std::fprintf( stderr, "ShaderCooker: cannot create output directory\n" );
		return 5;
	}
	const std::filesystem::path sManifest = sOutput.string() + ".manifest";
	std::filesystem::remove( sManifest, eError );
	std::filesystem::remove( sOutput, eError );
	const std::filesystem::path sInput = sOutput.string() + ".input.hlsl";
	{
		std::ofstream input( sInput, std::ios::binary | std::ios::trunc );
		input.write( manifest.sExpandedSource.data(), ( std::streamsize )manifest.sExpandedSource.size() );
		if ( !input ) {
			std::fprintf( stderr, "ShaderCooker: cannot write normalized source\n" );
			return 5;
		}
	}
	std::vector<std::string> szArgs;
	if ( manifest.nCompilerKind == SIR_SHADER_COMPILER_DXC ) {
		szArgs = { sCompiler.string(),   "-nologo", "-HV",           "2021", "-O3", "-T", manifest.sProfile, "-E",
		           manifest.sEntryPoint, "-Fo",     sOutput.string() };
		if ( manifest.bSpirv ) {
			szArgs.push_back( "-spirv" );
			szArgs.push_back( "-fspv-target-env=vulkan1.3" );
		}
		for ( const sirShaderCookDefine_t& definition : manifest.szDefines ) {
			szArgs.push_back( "-D" );
			szArgs.push_back( definition.sName + "=" + definition.sValue );
		}
	} else if ( manifest.nCompilerKind == SIR_SHADER_COMPILER_FXC ) {
		szArgs = { sCompiler.string(), "/nologo", "/O3", "/T", manifest.sProfile, "/E", manifest.sEntryPoint, "/Fo", sOutput.string() };
		for ( const sirShaderCookDefine_t& definition : manifest.szDefines ) {
			szArgs.push_back( "/D" );
			szArgs.push_back( definition.sName + "=" + definition.sValue );
		}
	} else {
		std::filesystem::remove( sInput, eError );
		return 4;
	}
	szArgs.push_back( sInput.string() );
	const int nCompilerResult = sirCookRunCompiler( szArgs );
	std::filesystem::remove( sInput, eError );
	if ( nCompilerResult != 0 ) {
		std::filesystem::remove( sOutput, eError );
		std::fprintf( stderr, "ShaderCooker: DXC exited with %d\n", nCompilerResult );
		return 6;
	}
	const uint64_t nBinaryHash = sirCookFileFingerprint( sOutput );
	if ( nBinaryHash == 0 || !sirCookValidateBinary( sOutput, manifest.eFormat ) ) {
		std::filesystem::remove( sOutput, eError );
		std::fprintf( stderr, "ShaderCooker: DXC did not emit a valid target binary\n" );
		return 7;
	}
	std::ofstream meta( sManifest, std::ios::binary | std::ios::trunc );
	meta << "SIR_SHADER_COOK_RECIPE=1\n"
	     << "BACKEND=" << ( uint32_t )request.eBackend << '\n'
	     << "MODEL=" << ( uint32_t )manifest.eModel << '\n'
	     << "FORMAT=" << ( uint32_t )manifest.eFormat << '\n'
	     << "COMPILER_KIND=" << manifest.nCompilerKind << '\n'
	     << "PROFILE=" << manifest.sProfile << '\n'
	     << "ENTRY=" << manifest.sEntryPoint << '\n'
	     << "SOURCE=" << manifest.nSourceHash << '\n'
	     << "INCLUDES=" << manifest.nClosureHash << '\n'
	     << "DEFINES=" << manifest.nDefinesHash << '\n'
	     << "COMPILER=" << nCompilerFingerprint << '\n'
	     << "BINARY=" << nBinaryHash << '\n';
	if ( !meta ) {
		meta.close();
		std::filesystem::remove( sManifest, eError );
		std::filesystem::remove( sOutput, eError );
		std::fprintf( stderr, "ShaderCooker: cannot write source-bound compiler manifest\n" );
		return 8;
	}
	return 0;
}
