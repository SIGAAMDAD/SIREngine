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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static bool sirCookCheck( bool bCondition, const char *pszMessage )
{
	if ( !bCondition ) {
		std::cerr << "FAIL: " << pszMessage << '\n';
	}
	return bCondition;
}

static void sirCookPut( const std::filesystem::path& sPath, const std::string& sContent )
{
	std::filesystem::create_directories( sPath.parent_path() );
	std::ofstream stream( sPath, std::ios::binary );
	stream << sContent;
}

static bool sirCookRun( const std::filesystem::path& sRoot )
{
	sirShaderCookRequest_t request = {};
	request.sRoot = sRoot;
	request.sEntryFile = "shader/main.hlsl";
	request.sEntryPoint = "main";
	request.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	request.nStage = SIR_GRAPHICS_STAGE_COMPUTE;
	request.szDefines.push_back( { "Z_COUNT", "32" } );
	request.szDefines.push_back( { "A_ENABLE", "1" } );
	sirShaderCookManifest_t first = {};
	sirShaderCookManifest_t second = {};
	std::string sError;

	sirCookPut( sRoot / "shader/include/math.hlsli", "#pragma once\r\nfloat4 value() { return 1; }\r\n" );
	sirCookPut( sRoot / "shader/main.hlsl",
	            "#include \"include/math.hlsli\"\r\n#include \"include/math.hlsli\"\r\n[numthreads(1,1,1)] void main() {}\r\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &first, &sError ) == SIR_SHADER_COOK_OK, "valid source DAG" ) ) {
		std::cerr << sError << '\n';
		return false;
	}
	if ( !sirCookCheck( first.szFiles.size() == 2 && first.sProfile == "cs_6_6" && first.bSpirv &&
	                        first.sExpandedSource.find( "float4 value()" ) != std::string::npos,
	                    "flattened DXC input" ) ) {
		return false;
	}
	request.szDefines.clear();
	request.szDefines.push_back( { "A_ENABLE", "1" } );
	request.szDefines.push_back( { "Z_COUNT", "32" } );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK &&
	                        first.nClosureHash == second.nClosureHash && first.nSourceHash == second.nSourceHash,
	                    "define canonicalization" ) ) {
		return false;
	}
	sirCookPut( sRoot / "shader/include/math.hlsli", "#pragma once\nfloat4 value() { return 2; }\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK &&
	                        first.nClosureHash != second.nClosureHash,
	                    "dependency invalidation" ) ) {
		return false;
	}
	// Conditional includes are evaluated before touching the dependency graph.
	// An inactive include may be absent; an active include is still hermetic.
	sirCookPut( sRoot / "shader/include/math.hlsli", "float4 value() { return 2; }\n" );
	sirCookPut( sRoot / "shader/main.hlsl",
	            "#if 0\n#include <missing.hlsli>\n#else\n#include \"include/math.hlsli\"\n#endif\n"
	            "#ifdef A_ENABLE\n#include \"include/math.hlsli\"\n#endif\n"
	            "#ifndef A_ENABLE\n#include \"missing.hlsli\"\n#endif\n"
	            "#if defined(A_ENABLE)\n#if 0\n#include \"missing.hlsli\"\n#elif 1\n#define INNER 1\n#endif\n#endif\n"
	            "#if INNER\n#include \"include/math.hlsli\"\n#endif\n"
	            "#undef INNER\n#if defined(INNER)\n#include \"missing.hlsli\"\n#endif\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK && second.szFiles.size() == 2 &&
	                        second.szFiles[0].szIncludes.empty() && second.szFiles[1].szIncludes.size() == 1,
	                    "conditional includes and unique dependency DAG" ) ) {
		std::cerr << sError << '\n';
		return false;
	}
	sirCookPut( sRoot / "shader/main.hlsl", "#if Z_COUNT\n#include \"include/math.hlsli\"\n#endif\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK && second.szFiles.size() == 2,
	                    "numeric define truthiness" ) ) {
		return false;
	}
	sirCookPut( sRoot / "shader/main.hlsl",
	            "#if 0\n#include <missing.hlsli>\n#else\n#include \"include/math.hlsli\"\n#endif\n"
	            "#ifdef A_ENABLE\n#include \"include/math.hlsli\"\n#endif\n"
	            "#ifndef A_ENABLE\n#include \"missing.hlsli\"\n#endif\n"
	            "#if defined(A_ENABLE)\n#if 0\n#include \"missing.hlsli\"\n#elif 1\n#define INNER 1\n#endif\n#endif\n"
	            "#if INNER\n#include \"include/math.hlsli\"\n#endif\n"
	            "#undef INNER\n#if defined(INNER)\n#include \"missing.hlsli\"\n#endif\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK, "recook conditional source" ) ) {
		return false;
	}
	const uint64_t nActiveHash = second.nClosureHash;
	sirCookPut( sRoot / "shader/missing.hlsli", "// inactive\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK && second.nClosureHash == nActiveHash,
	                    "inactive dependency does not invalidate source" ) ) {
		return false;
	}
	std::filesystem::remove( sRoot / "shader/missing.hlsli" );
	sirCookPut( sRoot / "shader/main.hlsl", "#if A_ENABLE && Z_COUNT\n#include \"include/math.hlsli\"\n#endif\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_BAD_DIRECTIVE,
	                    "complex conditional must fail closed" ) ) {
		return false;
	}
	sirCookPut( sRoot / "shader/main.hlsl", "#if 1\n#include \"include/math.hlsli\"\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_BAD_DIRECTIVE,
	                    "unterminated conditional rejected" ) ) {
		return false;
	}
	sirCookPut( sRoot / "shader/main.hlsl", "#endif\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_BAD_DIRECTIVE, "orphan endif rejected" ) ) {
		return false;
	}
	sirCookPut(
	    sRoot / "shader/main.hlsl",
	    "#if 0\n#include \"missing.hlsli\"\n#elif 1\n#include \"include/math.hlsli\"\n#else\n#include \"missing2.hlsli\"\n#endif\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK && second.szFiles.size() == 2,
	                    "elif and else path selection" ) ) {
		return false;
	}
	sirCookPut( sRoot / "shader/main.hlsl", "#include \"include/math.hlsli\"\n[numthreads(1,1,1)] void main() {}\n" );
	request.eBackend = SIR_GRAPHICS_BACKEND_D3D12;
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_OK && second.sProfile == "cs_6_6" &&
	                        !second.bSpirv,
	                    "DXIL recipe" ) ) {
		return false;
	}
	request.eBackend = SIR_GRAPHICS_BACKEND_OPENGL;
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_UNSUPPORTED && second.szFiles.empty(),
	                    "GL requires cross compiler" ) ) {
		return false;
	}
	request.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	request.nStage = 3;
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_INVALID_ARGUMENT, "invalid stage mask" ) ) {
		return false;
	}
	request.nStage = SIR_GRAPHICS_STAGE_COMPUTE;
	request.szDefines.push_back( { "A_ENABLE", "2" } );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_INVALID_ARGUMENT, "duplicate define" ) ) {
		return false;
	}
	request.szDefines.pop_back();
	request.szDefines[0].sName = "BAD-NAME";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_INVALID_ARGUMENT, "invalid define" ) ) {
		return false;
	}
	request.szDefines[0].sName = "A_ENABLE";
	request.szDefines[0].sValue = "A B";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_INVALID_ARGUMENT, "unsafe define" ) ) {
		return false;
	}
	request.szDefines[0].sValue = "1";
	request.sEntryFile = "../../etc/passwd";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_PATH_ESCAPE, "directory traversal" ) ) {
		return false;
	}
	request.sEntryFile = "shader/main.hlsl";
	sirCookPut( sRoot / "shader/include/math.hlsli", "#include \"../main.hlsl\"\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_INCLUDE_CYCLE, "include cycle" ) ) {
		return false;
	}
	sirCookPut( sRoot / "shader/main.hlsl", "#include <system.hlsl>\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_BAD_DIRECTIVE, "non-hermetic include" ) ) {
		return false;
	}
	sirCookPut( sRoot / "shader/main.hlsl", "#include \"missing.hlsli\"\n" );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &second, &sError ) == SIR_SHADER_COOK_IO_ERROR, "missing include" ) ) {
		return false;
	}
	if ( !sirCookCheck( sirShaderCookPreprocess( request, nullptr, &sError ) == SIR_SHADER_COOK_INVALID_ARGUMENT &&
	                        sirShaderCookPreprocess( request, &second, nullptr ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "null output" ) ) {
		return false;
	}
	std::string sProfile;
	bool bSpirv = false;
	if ( !sirCookCheck( sirShaderCookResolveTarget( SIR_GRAPHICS_BACKEND_D3D12, SIR_GRAPHICS_STAGE_VERTEX, &sProfile, &bSpirv ) ==
	                            SIR_SHADER_COOK_OK &&
	                        sProfile == "vs_6_6",
	                    "vertex target" ) ) {
		return false;
	}
	return sirCookCheck( sirShaderCookResolveTarget( SIR_GRAPHICS_BACKEND_D3D11, SIR_GRAPHICS_STAGE_VERTEX, &sProfile, &bSpirv ) ==
	                             SIR_SHADER_COOK_UNSUPPORTED &&
	                         sirShaderCookResolveTarget( SIR_GRAPHICS_BACKEND_VULKAN, SIR_GRAPHICS_STAGE_VERTEX, nullptr, &bSpirv ) ==
	                             SIR_SHADER_COOK_INVALID_ARGUMENT,
	                     "legacy recipe and null guards" );
}

int main( void )
{
	const std::filesystem::path sRoot = std::filesystem::temp_directory_path() / "sir_shader_frontend_tests";
	std::filesystem::remove_all( sRoot );
	std::filesystem::create_directories( sRoot );
	const bool bPassed = sirCookRun( sRoot );
	std::filesystem::remove_all( sRoot );
	if ( bPassed ) {
		std::cout << "SIRShaderFrontendTests passed\n";
	}
	return bPassed ? 0 : 1;
}
