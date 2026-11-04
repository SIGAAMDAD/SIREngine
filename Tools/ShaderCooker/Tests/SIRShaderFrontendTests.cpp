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

#if defined( __linux__ )
#include <unistd.h>
#endif

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

	// A name beginning with "defined" remains an ordinary macro identifier.
	// Treating definedFOO as defined(FOO) would change the cooked include DAG.
	if ( !checkSource( "#define FOO 1\n#if definedFOO\n#include \"missing.hlsli\"\n#else\nint correct;\n#endif\n", SIR_SHADER_COOK_OK,
	                   "defined-prefix is an independent identifier" ) ||
	     !checkSource( "#define definedFOO 1\n#if definedFOO\nint correct;\n#else\n#include \"missing.hlsli\"\n#endif\n",
	                   SIR_SHADER_COOK_OK, "explicit defined-prefix macro truthiness" ) ||
	     !checkSource( "#define FOO 1\n#if !definedFOO\nint correct;\n#else\n#include \"missing.hlsli\"\n#endif\n", SIR_SHADER_COOK_OK,
	                   "negated undefined defined-prefix identifier" ) ) {
		return false;
	}

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

	if ( !checkSource( "#pragma\n", SIR_SHADER_COOK_OK, "pragma without argument" ) ||
	     !checkSource( "#unrecognized\n", SIR_SHADER_COOK_OK, "unknown directive retained" ) ||
	     !checkSource( "#include shared.hlsli\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "unquoted include" ) ||
	     !checkSource( "#include \"shared.hlsli\" // tail\n", SIR_SHADER_COOK_OK, "include comment suffix" ) ||
	     !checkSource( "#pragma once   \n", SIR_SHADER_COOK_OK, "pragma with whitespace suffix" ) ||
	     !checkSource( "#if defined(_FLAG)\nint present;\n#else\nint absent;\n#endif\n", SIR_SHADER_COOK_OK,
	                   "defined identifier underscore" ) ||
	     !checkSource( "#define _FLAG 1\n#if defined(_FLAG)\nint present;\n#endif\n", SIR_SHADER_COOK_OK,
	                   "defined true underscore identifier" ) ||
	     !checkSource( "#if defined()\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "empty defined token" ) ||
	     !checkSource( "#if 1\n#else\nint absent;\n#endif\n", SIR_SHADER_COOK_OK, "else already taken" ) ||
	     !checkSource( "#if 1\n#elif 1\nint absent;\n#endif\n", SIR_SHADER_COOK_OK, "elif already taken" ) ||
	     !checkSource( "#if 0\n#elif 1\nint present;\n#endif\n", SIR_SHADER_COOK_OK, "elif selected" ) ||
	     !checkSource( "#if 0\n#elif 0\n#else\nint present;\n#endif\n", SIR_SHADER_COOK_OK, "elif false then else" ) ||
	     !checkSource( "#define _FLAG 12\n#if !_FLAG\nint absent;\n#endif\n", SIR_SHADER_COOK_OK, "inverted nonzero macro" ) ||
	     !checkSource( "float x = \"unterminated\n", SIR_SHADER_COOK_OK, "unterminated ordinary string" ) ||
	     !checkSource( "int x;", SIR_SHADER_COOK_OK, "source without final newline" ) ||
	     !checkSource( std::string( "\xef\xbb\xbf" ) + "int x;\r", SIR_SHADER_COOK_OK, "UTF-8 BOM and CR" ) ||
	     !checkSource( "#include \"../../outside.hlsli\"\n", SIR_SHADER_COOK_PATH_ESCAPE, "parent escapes source root" ) ) {
		return false;
	}

	if ( !checkSource( "#include   \n", SIR_SHADER_COOK_BAD_DIRECTIVE, "include whitespace without path" ) ||
	     !checkSource( "#pragma   \n", SIR_SHADER_COOK_OK, "blank pragma" ) ||
	     !checkSource( "#pragma oncemore\n", SIR_SHADER_COOK_OK, "unrecognized pragma suffix" ) ||
	     !checkSource( "#if defined _FLAG\nint absent;\n#endif\n", SIR_SHADER_COOK_OK, "defined without parentheses" ) ||
	     !checkSource( "#if defined(_FLAG\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "missing defined parenthesis" ) ||
	     !checkSource( "#if defined _FLAG)\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "extra defined parenthesis" ) ||
	     !checkSource( "#if (1)\n#endif\n", SIR_SHADER_COOK_BAD_DIRECTIVE, "parenthesized expression unsupported" ) ||
	     !checkSource( "#if 0\n#if 1\n#elif 1\n#else\n#endif\n#endif\n", SIR_SHADER_COOK_OK, "inactive ancestor with elif and else" ) ||
	     !checkSource( "#if 0\n#if 0\n#else\n#endif\n#endif\n", SIR_SHADER_COOK_OK, "inactive parent for else branch" ) ||
	     !checkSource( "#define _FLAG 1\n#if _FLAG\nint present;\n#endif\n", SIR_SHADER_COOK_OK, "underscore-valued macro condition" ) ) {
		return false;
	}

	// A pragma-one header remains single-copy even with tab suffix.
	sirCookPut( sRoot / "case/once_tabs.hlsli", "#pragma once\t\nint unique_definition;\n" );
	if ( !checkSource( "#include \"once_tabs.hlsli\"\n#include \"once_tabs.hlsli\"\n", SIR_SHADER_COOK_OK,
	                   "pragma once followed by tab" ) ) {
		return false;
	}
	const size_t iUnique = manifest.sExpandedSource.find( "int unique_definition;" );
	if ( !sirCookCheck( iUnique != std::string::npos &&
	                        manifest.sExpandedSource.find( "int unique_definition;", iUnique + 1 ) == std::string::npos,
	                    "one-time header actually expanded once" ) ) {
		return false;
	}

	request.sEntryPoint = "{INVALID";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "identifier beyond lowercase alphabet" ) ) {
		return false;
	}
	request.sEntryPoint = "main";
	request.szDefines.push_back( { "_FLAG", "\x7f" } );
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "nonprintable define" ) ) {
		return false;
	}
	request.szDefines[0].sValue = "\"";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "quote in define" ) ) {
		return false;
	}
	request.szDefines[0].sValue = "\\";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "backslash in define" ) ) {
		return false;
	}
	request.szDefines.clear();

	request.sEntryFile = "/absolute/source.hlsl";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "absolute entry path" ) ) {
		return false;
	}
	request.sEntryFile = "case/main.hlsl";
	request.sRoot = sRoot / "case/main.hlsl";
	if ( !sirCookCheck( sirShaderCookPreprocess( request, &manifest, &sDiagnostic ) == SIR_SHADER_COOK_IO_ERROR,
	                    "regular file is not source root" ) ) {
		return false;
	}
	request.sRoot = sRoot;

	const std::filesystem::path sExternal = sRoot.parent_path() / "sir_shader_outside_fixture.hlsli";
	sirCookPut( sExternal, "int x;\n" );
	std::error_code eSymlink;
	std::filesystem::create_symlink( sExternal, sRoot / "case/escape.hlsli", eSymlink );
	if ( !eSymlink && !checkSource( "#include \"escape.hlsli\"\n", SIR_SHADER_COOK_PATH_ESCAPE, "symlink escapes root" ) ) {
		std::filesystem::remove( sExternal );
		return false;
	}
	std::filesystem::remove( sRoot / "case/escape.hlsli" );
	std::filesystem::remove( sExternal );

	// On unprivileged Linux hosts a readable directory entry can still refer
	// to a regular file that is impossible to open. Preserve the exact error.
#if defined( __linux__ )
	if ( geteuid() != 0 ) {
		const std::filesystem::path sUnreadable = sRoot / "case/unreadable.hlsli";
		sirCookPut( sUnreadable, "int hidden;\n" );
		std::error_code ePermissions;
		std::filesystem::permissions( sUnreadable, std::filesystem::perms::none, ePermissions );
		if ( !ePermissions ) {
			const bool bCorrect =
			    checkSource( "#include \"unreadable.hlsli\"\n", SIR_SHADER_COOK_IO_ERROR, "unreadable regular shader source" );
			std::filesystem::permissions( sUnreadable, std::filesystem::perms::owner_all, ePermissions );
			if ( !bCorrect || ePermissions ) {
				return false;
			}
		}
	}
#endif

	// Exercise the distinct aggregate-input and unique-file admission limits.
	std::string sUniqueIncludes;
	for ( uint32_t iFile = 0; iFile < 9; ++iFile ) {
		const std::string sName = "large" + std::to_string( iFile ) + ".hlsli";
		sirCookPut( sRoot / "case" / sName, std::string( ( 1U << 20 ) - 64, 'x' ) + "\n" );
		sUniqueIncludes += "#include \"" + sName + "\"\n";
	}
	if ( !checkSource( sUniqueIncludes, SIR_SHADER_COOK_CAPACITY_EXCEEDED, "aggregate source byte limit" ) ) {
		return false;
	}

	sUniqueIncludes.clear();
	for ( uint32_t iFile = 0; iFile < 257; ++iFile ) {
		const std::string sName = "part" + std::to_string( iFile ) + ".hlsli";
		sirCookPut( sRoot / "case" / sName, "int shared;\n" );
		sUniqueIncludes += "#include \"" + sName + "\"\n";
	}
	if ( !checkSource( sUniqueIncludes, SIR_SHADER_COOK_CAPACITY_EXCEEDED, "unique file-count limit" ) ) {
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

static bool sirCookTargetMatrix( void )
{
	const uint32_t szStages[] = { SIR_GRAPHICS_STAGE_VERTEX,   SIR_GRAPHICS_STAGE_FRAGMENT,     SIR_GRAPHICS_STAGE_COMPUTE,
	                              SIR_GRAPHICS_STAGE_GEOMETRY, SIR_GRAPHICS_STAGE_TESS_CONTROL, SIR_GRAPHICS_STAGE_TESS_EVALUATION,
	                              SIR_GRAPHICS_STAGE_MESH,     SIR_GRAPHICS_STAGE_TASK };
	const char *szProfiles[] = { "vs_6_6", "ps_6_6", "cs_6_6", "gs_6_6", "hs_6_6", "ds_6_6", "ms_6_6", "as_6_6" };
	for ( const sirGraphicsBackend_t eBackend : { SIR_GRAPHICS_BACKEND_VULKAN, SIR_GRAPHICS_BACKEND_D3D12 } ) {
		for ( uint32_t iStage = 0; iStage < 8; ++iStage ) {
			std::string sProfile = "stale";
			bool bSpirv = false;
			const sirShaderCookResult_t eResult = sirShaderCookResolveTarget( eBackend, szStages[iStage], &sProfile, &bSpirv );
			if ( !sirCookCheck( eResult == SIR_SHADER_COOK_OK && sProfile == szProfiles[iStage] &&
			                        bSpirv == ( eBackend == SIR_GRAPHICS_BACKEND_VULKAN ),
			                    "complete shader target matrix" ) ) {
				return false;
			}
		}
	}
	std::string sProfile = "stale";
	bool bSpirv = true;
	if ( !sirCookCheck( sirShaderCookResolveTarget( SIR_GRAPHICS_BACKEND_OPENGL, SIR_GRAPHICS_STAGE_VERTEX, &sProfile, &bSpirv ) ==
	                            SIR_SHADER_COOK_UNSUPPORTED &&
	                        sProfile.empty() && !bSpirv,
	                    "no unsupported legacy recipe" ) ||
	     !sirCookCheck( sirShaderCookResolveTarget( SIR_GRAPHICS_BACKEND_VULKAN, SIR_GRAPHICS_STAGE_VERTEX, &sProfile, nullptr ) ==
	                        SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "null spirv flag rejected" ) ||
	     !sirCookCheck( sirShaderCookResolveTarget( SIR_GRAPHICS_BACKEND_VULKAN, SIR_GRAPHICS_STAGE_VERTEX, nullptr, &bSpirv ) ==
	                        SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "null profile rejected" ) ||
	     !sirCookCheck( sirShaderCookResolveTarget( SIR_GRAPHICS_BACKEND_D3D12, 0, &sProfile, &bSpirv ) == SIR_SHADER_COOK_INVALID_ARGUMENT,
	                    "zero shader stage rejected" ) ) {
		return false;
	}
	return true;
}

int main( void )
{
	const std::filesystem::path sRoot = std::filesystem::temp_directory_path() / "sir_shader_frontend_tests";
	std::filesystem::remove_all( sRoot );
	std::filesystem::create_directories( sRoot );
	const bool bPassed = sirCookRun( sRoot ) && sirCookBoundaryCases( sRoot ) && sirCookTargetMatrix();
	std::filesystem::remove_all( sRoot );
	if ( bPassed ) {
		std::cout << "SIRShaderFrontendTests passed\n";
	}
	return bPassed ? 0 : 1;
}
