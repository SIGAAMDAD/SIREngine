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

static bool sirCookBoundaryCases( const std::filesystem::path& sRoot )
{
	sirShaderCookRequest_t request = {};
	request.sRoot = sRoot;
	request.sEntryFile = "case/main.hlsl";
	request.sEntryPoint = "main";
	request.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	request.nStage = SIR_GRAPHICS_STAGE_COMPUTE;

	sirShaderCookManifest_t manifest = {};
	std::string sDiagnostic;
	const auto checkSource = [&]( const std::string& sSource, sirShaderCookResult_t eExpected, const char *pszDescription ) {
		sirCookPut( sRoot / "case/main.hlsl", sSource );
		const sirShaderCookResult_t eActual = sirShaderCookPreprocess( request, &manifest, &sDiagnostic );
		if ( eActual != eExpected ) {
			std::cerr << "FAIL " << pszDescription << ": expected " << ( uint32_t )eExpected << ", got " << ( uint32_t )eActual << ": "
			          << sDiagnostic << '\n';
			return false;
		}
		return true;
	};

	if ( !checkSource( "#if !defined(ABSENT)\nint present = 1;\n#endif\n", SIR_SHADER_COOK_OK, "negated defined" ) ||
	     !checkSource( "#if UNKNOWN_MACRO\n#include \"missing.h\"\n#else\nint present = 1;\n#endif\n", SIR_SHADER_COOK_OK,
	                   "undefined macro treated as zero" ) ||
	     !checkSource( "#define MARKER 0\n#if !MARKER\nint present = 1;\n#endif\n", SIR_SHADER_COOK_OK, "inverted zero macro" ) ||
	     !checkSource( "#define MARKER nope\n#if MARKER\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "unsupported macro expression" ) ||
	     !checkSource( "#if defined(1BAD)\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "invalid defined identifier" ) ||
	     !checkSource( "#ifdef 1BAD\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "invalid ifdef identifier" ) ||
	     !checkSource( "#if 0\n#else extra\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "else argument" ) ||
	     !checkSource( "#if 0\n#elif A && B\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "invalid elif expression" ) ||
	     !checkSource( "#if 1\n#else\n#elif 1\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "elif after else" ) ||
	     !checkSource( "#else\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "orphan else" ) ||
	     !checkSource( "#define 1BAD 1\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "invalid define name" ) ||
	     !checkSource( "#undef 1BAD\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "invalid undef name" ) ||
	     !checkSource( "#include\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "missing include argument" ) ||
	     !checkSource( "#include \"unterminated\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "unterminated include string" ) ||
	     !checkSource( "#include \"good.h\" invalid\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "include junk suffix" ) ||
	     !checkSource( "#include \"bad+name.h\"\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "invalid include characters" ) ||
	     !checkSource( "#include \"../outside.h\"\n", SIR_SHADER_COOK_IO_ERROR, "missing parent include" ) ||
	     !checkSource( "#pragma once\n#pragma unrelated\n", SIR_SHADER_COOK_OK, "pragma handling" ) ||
	     !checkSource( "/*\n#include \"missing.h\"\n*/\nint active = 1;\n", SIR_SHADER_COOK_OK, "block comment include" ) ||
	     !checkSource( "// #include \"missing.h\"\n", SIR_SHADER_COOK_OK, "line comment include" ) ||
	     !checkSource( "#include \"notthere.h\"\n", SIR_SHADER_COOK_IO_ERROR, "nonexistent child" ) ||
	     !checkSource( "#if 0\n#if A && B\n#endif\n#endif\n", SIR_SHADER_COOK_OK, "inactive nested expression" ) ) {
		return false;
	}

	request.sEntryFile = "";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "empty entry path" ) ) {
		return false;
	}
	request.sEntryFile = "case/main.hlsl";
	request.sEntryPoint = "bad-name";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "invalid entrypoint identifier" ) ) {
		return false;
	}
	request.sEntryPoint = "main";
	request.szDefines.resize( 65, { "DEFINED", "1" } );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "define capacity" ) ) {
		return false;
	}
	request.szDefines.clear();
	request.sRoot = sRoot / "does-not-exist";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_IO_ERROR,
	                    "nonexistent shader root" ) ) {
		return false;
	}
	request.sRoot = sRoot;

	// Reject embedded NUL even when the underlying file exists and passes stat.
	if ( !checkSource( std::string( "int main() {\0}\n", 15 ), SIR_SHADER_COOK_IO_ERROR, "embedded NUL" ) ) {
		return false;
	}
	if ( !checkSource( std::string( ( 1U << 20 ) + 1, 'x' ), SIR_SHADER_COOK_CAPACITY_EXCEEDED, "file byte limit" ) ) {
		return false;
	}

	if ( !checkSource( "#define EMPTY\n#ifdef EMPTY\nint present = 1;\n#endif\n", SIR_SHADER_COOK_OK, "empty macro exists" ) ||
	     !checkSource( "#define EMPTY\n#if EMPTY\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "empty macro not numeric" ) ||
	     !checkSource( "#if 2\nint present = 1;\n#endif\n", SIR_SHADER_COOK_OK, "numeric nonzero condition" ) ||
	     !checkSource( "#if\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "empty conditional" ) ||
	     !checkSource( "#\n", SIR_SHADER_COOK_OK, "empty directive" ) || !checkSource( "   \t\n", SIR_SHADER_COOK_OK, "whitespace line" ) ||
	     !checkSource( "#pragma unrelated\n", SIR_SHADER_COOK_OK, "unrelated pragma" ) ||
	     !checkSource( "#include \"" + std::string( 252, 'a' ) + "\"\n", SIR_SHADER_COOK_PATH_ESCAPE, "normalized child path too long" ) ) {
		return false;
	}

	// One small physical file may be included repeatedly, but its expanded
	// representation must stay within the admitted 16 MiB budget.
	sirCookPut( sRoot / "case/shared.hlsli", std::string( ( 1U << 20 ) - 128, 'x' ) + "\n" );
	std::string sRepeated;
	for ( uint32_t iInclude = 0; iInclude < 18; ++iInclude ) {
		sRepeated += "#include \"shared.hlsli\"\n";
	}
	if ( !checkSource( sRepeated, SIR_SHADER_COOK_CAPACITY_EXCEEDED, "expanded source limit" ) ) {
		return false;
	}

	// Depth is independent of the number-of-files and byte budgets.
	for ( uint32_t iDepth = 0; iDepth < 66; ++iDepth ) {
		sirCookPut( sRoot / "case" / ( "depth" + std::to_string( iDepth ) + ".hlsli" ),
		            "#include \"depth" + std::to_string( iDepth + 1 ) + ".hlsli\"\n" );
	}
	if ( !checkSource( "#include \"depth0.hlsli\"\n", SIR_SHADER_COOK_CAPACITY_EXCEEDED, "include depth limit" ) ) {
		return false;
	}

	return true;
}

int main( void )
{
	const std::filesystem::path sRoot = std::filesystem::temp_directory_path() / "sir_shader_frontend_tests";
	std::filesystem::remove_all( sRoot );
	std::filesystem::create_directories( sRoot );
	const bool bPassed = sirCookRun( sRoot ) && sirCookBoundaryCases( sRoot );
	std::filesystem::remove_all( sRoot );
	if ( bPassed ) {
		std::cout << "SIRShaderFrontendTests passed\n";
	}
	return bPassed ? 0 : 1;
}
