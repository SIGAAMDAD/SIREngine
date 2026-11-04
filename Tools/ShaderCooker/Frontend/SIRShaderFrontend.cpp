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

#include <algorithm>
#include <fstream>
#include <iterator>
#include <cctype>
#include <set>
#include <unordered_map>

static constexpr uint64_t SIR_COOK_FNV_BASIS = 14695981039346656037ULL;
static constexpr uint64_t SIR_COOK_FNV_PRIME = 1099511628211ULL;
static constexpr uint32_t SIR_COOK_RECIPE_VERSION = 1;
static constexpr size_t SIR_COOK_MAX_FILE_BYTES = 1U << 20;
static constexpr size_t SIR_COOK_MAX_TOTAL_BYTES = 8U << 20;
static constexpr size_t SIR_COOK_MAX_EXPANDED_BYTES = 16U << 20;
static constexpr size_t SIR_COOK_MAX_FILES = 256;
static constexpr size_t SIR_COOK_MAX_DEPTH = 64;

typedef struct sirShaderCookContext_s
{
	std::filesystem::path sCanonicalRoot;
	sirShaderCookManifest_t *pManifest = nullptr;
	std::string *pDiagnostic = nullptr;
	std::set<std::string> szActive;
	std::set<std::string> szOnce;
	std::unordered_map<std::string, size_t> szKnownFiles;
	std::unordered_map<std::string, std::string> szCachedSources;
	std::unordered_map<std::string, std::string> szMacros;
	size_t nTotalBytes = 0;
} sirShaderCookContext_t;

static uint64_t sirCookHash( uint64_t nHash, const std::string& sText ) noexcept
{
	for ( unsigned char nByte : sText ) {
		nHash = ( nHash ^ nByte ) * SIR_COOK_FNV_PRIME;
	}
	return ( nHash ^ 0xffU ) * SIR_COOK_FNV_PRIME;
}

static bool sirCookIsIdentifier( const std::string& sName ) noexcept
{
	if ( sName.empty() || !( ( sName[0] >= 'A' && sName[0] <= 'Z' ) || ( sName[0] >= 'a' && sName[0] <= 'z' ) || sName[0] == '_' ) ) {
		return false;
	}
	for ( char nChar : sName ) {
		if ( !( ( nChar >= 'A' && nChar <= 'Z' ) || ( nChar >= 'a' && nChar <= 'z' ) || ( nChar >= '0' && nChar <= '9' ) ||
		        nChar == '_' ) ) {
			return false;
		}
	}
	return true;
}

static bool sirCookIsLogicalPath( const std::string& sPath ) noexcept
{
	if ( sPath.empty() || sPath.size() > 256 || sPath[0] == '/' ) {
		return false;
	}
	for ( char nChar : sPath ) {
		if ( !( ( nChar >= 'A' && nChar <= 'Z' ) || ( nChar >= 'a' && nChar <= 'z' ) || ( nChar >= '0' && nChar <= '9' ) || nChar == '_' ||
		        nChar == '-' || nChar == '.' || nChar == '/' ) ) {
			return false;
		}
	}
	return true;
}

static bool sirCookHasRootPrefix( const std::filesystem::path& sRoot, const std::filesystem::path& sFile )
{
	auto iRoot = sRoot.begin();
	auto iFile = sFile.begin();
	for ( ; iRoot != sRoot.end(); ++iRoot, ++iFile ) {
		if ( iFile == sFile.end() || *iRoot != *iFile ) {
			return false;
		}
	}
	return true;
}

static std::string sirCookNormalizeNewlines( const std::string& sText )
{
	std::string sNormalized;
	sNormalized.reserve( sText.size() );
	size_t iByte =
	    sText.size() >= 3 && ( unsigned char )sText[0] == 0xef && ( unsigned char )sText[1] == 0xbb && ( unsigned char )sText[2] == 0xbf
	        ? 3U
	        : 0U;
	for ( ; iByte < sText.size(); ++iByte ) {
		if ( sText[iByte] == '\r' ) {
			sNormalized.push_back( '\n' );
			if ( iByte + 1 < sText.size() && sText[iByte + 1] == '\n' ) {
				++iByte;
			}
		} else {
			sNormalized.push_back( sText[iByte] );
		}
	}
	return sNormalized;
}

// Return 0 for an ordinary line, 1 for a quoted include, 2 for pragma once,
// and -1 for a directive that cannot be deterministically expanded.
static int32_t sirCookReadDirective( const std::string& sLine, std::string *pInclude )
{
	size_t iChar = sLine.find_first_not_of( " \t" );
	if ( iChar == std::string::npos || sLine[iChar] != '#' ) {
		return 0;
	}
	++iChar;
	iChar = sLine.find_first_not_of( " \t", iChar );
	if ( iChar == std::string::npos ) {
		return 0;
	}
	const size_t nWordEnd = sLine.find_first_of( " \t", iChar );
	const std::string sWord = sLine.substr( iChar, nWordEnd - iChar );
	if ( sWord != "include" && sWord != "pragma" ) {
		return 0;
	}
	if ( nWordEnd == std::string::npos ) {
		return sWord == "include" ? -1 : 0;
	}
	iChar = sLine.find_first_not_of( " \t", nWordEnd );
	if ( sWord == "pragma" ) {
		return iChar != std::string::npos && sLine.compare( iChar, 4, "once" ) == 0 &&
		               ( iChar + 4 == sLine.size() || sLine[iChar + 4] == ' ' )
		           ? 2
				   : 0;
	}
	if ( iChar == std::string::npos || sLine[iChar] != '"' ) {
		return -1;
	}
	const size_t iEnd = sLine.find( '"', iChar + 1 );
	if ( iEnd == std::string::npos ) {
		return -1;
	}
	*pInclude = sLine.substr( iChar + 1, iEnd - iChar - 1 );
	const size_t iTail = sLine.find_first_not_of( " \t", iEnd + 1 );
	if ( iTail != std::string::npos && sLine.compare( iTail, 2, "//" ) != 0 ) {
		return -1;
	}
	return sirCookIsLogicalPath( *pInclude ) ? 1 : -1;
}

typedef struct sirShaderCookCondition_s
{
	bool bParentActive = false;
	bool bAnyTaken = false;
	bool bElseSeen = false;
} sirShaderCookCondition_t;

static std::string sirCookTrim( const std::string& sText )
{
	const size_t iStart = sText.find_first_not_of( " \t" );
	if ( iStart == std::string::npos ) {
		return {};
	}
	const size_t iEnd = sText.find_last_not_of( " \t" );
	return sText.substr( iStart, iEnd - iStart + 1 );
}

static void sirCookSplitDirective( const std::string& sLine, std::string *pWord, std::string *pArgument )
{
	pWord->clear();
	pArgument->clear();
	const size_t iFirst = sLine.find_first_not_of( " \t" );
	if ( iFirst == std::string::npos || sLine[iFirst] != '#' ) {
		return;
	}
	const size_t iName = sLine.find_first_not_of( " \t", iFirst + 1 );
	if ( iName == std::string::npos ) {
		return;
	}
	const size_t iEnd = sLine.find_first_of( " \t", iName );
	*pWord = sLine.substr( iName, iEnd - iName );
	if ( iEnd != std::string::npos ) {
		*pArgument = sirCookTrim( sLine.substr( iEnd + 1 ) );
	}
}

static bool sirCookEvaluateCondition( const std::string& sRaw, const sirShaderCookContext_t& context, bool *pValue )
{
	const std::string sExpression = sirCookTrim( sRaw );
	if ( sExpression == "0" || sExpression == "1" ) {
		*pValue = sExpression == "1";
		return true;
	}
	bool bInvert = false;
	std::string sTerm = sExpression;
	if ( !sTerm.empty() && sTerm[0] == '!' ) {
		bInvert = true;
		sTerm = sirCookTrim( sTerm.substr( 1 ) );
	}
	if ( sTerm.compare( 0, 7, "defined" ) == 0 ) {
		sTerm = sirCookTrim( sTerm.substr( 7 ) );
		if ( sTerm.size() >= 2 && sTerm.front() == '(' && sTerm.back() == ')' ) {
			sTerm = sirCookTrim( sTerm.substr( 1, sTerm.size() - 2 ) );
		}
		if ( !sirCookIsIdentifier( sTerm ) ) {
			return false;
		}
		*pValue = ( context.szMacros.find( sTerm ) != context.szMacros.end() ) != bInvert;
		return true;
	}
	const auto sirIsDecimal = []( const std::string& sNumber, bool *pNonZero ) {
		if ( sNumber.empty() ) {
			return false;
		}
		*pNonZero = false;
		for ( char nDigit : sNumber ) {
			if ( nDigit < '0' || nDigit > '9' ) {
				return false;
			}
			*pNonZero |= nDigit != '0';
		}
		return true;
	};
	bool bNumeric = false;
	if ( sirIsDecimal( sTerm, &bNumeric ) ) {
		*pValue = bNumeric != bInvert;
		return true;
	}
	if ( !sirCookIsIdentifier( sTerm ) ) {
		return false;
	}
	const auto found = context.szMacros.find( sTerm );
	if ( found == context.szMacros.end() ) {
		*pValue = bInvert;
		return true;
	}
	if ( !sirIsDecimal( found->second, &bNumeric ) ) {
		return false;
	}
	*pValue = bNumeric != bInvert;
	return true;
}

static sirShaderCookResult_t sirCookExpand( sirShaderCookContext_t *pContext, const std::string& sLogicalPath, size_t nDepth )
{
	if ( nDepth > SIR_COOK_MAX_DEPTH ) {
		*pContext->pDiagnostic = "maximum shader include depth exceeded";
		return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
	}
	const std::string sPath = std::filesystem::path( sLogicalPath ).lexically_normal().generic_string();
	if ( !sirCookIsLogicalPath( sPath ) ) {
		*pContext->pDiagnostic = "invalid source path: " + sLogicalPath;
		return SIR_SHADER_COOK_PATH_ESCAPE;
	}
	std::error_code eError;
	const std::filesystem::path sDiskFile = std::filesystem::weakly_canonical( pContext->sCanonicalRoot / sPath, eError );
	if ( eError || !sirCookHasRootPrefix( pContext->sCanonicalRoot, sDiskFile ) ) {
		*pContext->pDiagnostic = "shader include escapes source root: " + sPath;
		return SIR_SHADER_COOK_PATH_ESCAPE;
	}
	if ( pContext->szOnce.find( sPath ) != pContext->szOnce.end() ) {
		return SIR_SHADER_COOK_OK;
	}
	if ( pContext->szActive.find( sPath ) != pContext->szActive.end() ) {
		*pContext->pDiagnostic = "shader include cycle: " + sPath;
		return SIR_SHADER_COOK_INCLUDE_CYCLE;
	}
	std::string sSource;
	size_t nIndex = 0;
	const auto known = pContext->szKnownFiles.find( sPath );
	if ( known != pContext->szKnownFiles.end() ) {
		nIndex = known->second;
		sSource = pContext->szCachedSources.at( sPath );
	} else {
		if ( !std::filesystem::is_regular_file( sDiskFile, eError ) || eError ) {
			*pContext->pDiagnostic = "shader source is missing or not a file: " + sPath;
			return SIR_SHADER_COOK_IO_ERROR;
		}
		const auto nSize = std::filesystem::file_size( sDiskFile, eError );
		if ( eError ) {
			*pContext->pDiagnostic = "cannot stat shader source: " + sPath;
			return SIR_SHADER_COOK_IO_ERROR;
		}
		if ( nSize > SIR_COOK_MAX_FILE_BYTES || nSize > SIR_COOK_MAX_TOTAL_BYTES - pContext->nTotalBytes ||
		     pContext->szKnownFiles.size() >= SIR_COOK_MAX_FILES ) {
			*pContext->pDiagnostic = "shader source capacity exceeded: " + sPath;
			return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
		}
		std::ifstream stream( sDiskFile, std::ios::binary );
		if ( !stream ) {
			*pContext->pDiagnostic = "cannot open shader source: " + sPath;
			return SIR_SHADER_COOK_IO_ERROR;
		}
		const std::string sRaw( ( std::istreambuf_iterator<char>( stream ) ), std::istreambuf_iterator<char>() );
		if ( stream.bad() || sRaw.size() != nSize || sRaw.find( '\0' ) != std::string::npos ) {
			*pContext->pDiagnostic = "invalid or incomplete shader source: " + sPath;
			return SIR_SHADER_COOK_IO_ERROR;
		}
		pContext->nTotalBytes += sRaw.size();
		sSource = sirCookNormalizeNewlines( sRaw );
		nIndex = pContext->pManifest->szFiles.size();
		pContext->pManifest->szFiles.push_back( { sPath, sirCookHash( SIR_COOK_FNV_BASIS, sSource ), {} } );
		pContext->szKnownFiles.emplace( sPath, nIndex );
		pContext->szCachedSources.emplace( sPath, sSource );
	}
	pContext->szActive.insert( sPath );
	pContext->pManifest->sExpandedSource += "#line 1 \"" + sPath + "\"\n";

	std::vector<sirShaderCookCondition_t> szConditions;
	bool bActive = true;
	bool bInBlockComment = false;
	size_t iLine = 0;
	uint32_t nLine = 1;
	while ( iLine < sSource.size() ) {
		const size_t iEnd = sSource.find( '\n', iLine );
		const std::string sLine = sSource.substr( iLine, iEnd == std::string::npos ? std::string::npos : iEnd - iLine );
		std::string sWord;
		std::string sArgument;
		if ( !bInBlockComment ) {
			sirCookSplitDirective( sLine, &sWord, &sArgument );
		}
		const bool bConditional =
		    sWord == "if" || sWord == "ifdef" || sWord == "ifndef" || sWord == "elif" || sWord == "else" || sWord == "endif";
		if ( bConditional ) {
			if ( sWord == "if" || sWord == "ifdef" || sWord == "ifndef" ) {
				bool bCondition = false;
				if ( bActive ) {
					if ( sWord == "if" ) {
						if ( !sirCookEvaluateCondition( sArgument, *pContext, &bCondition ) ) {
							*pContext->pDiagnostic = "unsupported conditional expression: " + sPath + ":" + std::to_string( nLine );
							return SIR_SHADER_COOK_BAD_DIRECTIVE;
						}
					} else {
						if ( !sirCookIsIdentifier( sArgument ) ) {
							*pContext->pDiagnostic = "invalid conditional identifier: " + sPath + ":" + std::to_string( nLine );
							return SIR_SHADER_COOK_BAD_DIRECTIVE;
						}
						bCondition = pContext->szMacros.find( sArgument ) != pContext->szMacros.end();
						if ( sWord == "ifndef" ) {
							bCondition = !bCondition;
						}
					}
				}
				szConditions.push_back( { bActive, bCondition, false } );
				bActive = bActive && bCondition;
			} else {
				if ( szConditions.empty() || ( sWord != "endif" && szConditions.back().bElseSeen ) ) {
					*pContext->pDiagnostic = "unbalanced shader conditional: " + sPath + ":" + std::to_string( nLine );
					return SIR_SHADER_COOK_BAD_DIRECTIVE;
				}
				sirShaderCookCondition_t& condition = szConditions.back();
				if ( sWord == "endif" ) {
					bActive = condition.bParentActive;
					szConditions.pop_back();
				} else if ( sWord == "else" ) {
					if ( !sArgument.empty() ) {
						*pContext->pDiagnostic = "invalid shader else directive";
						return SIR_SHADER_COOK_BAD_DIRECTIVE;
					}
					condition.bElseSeen = true;
					bActive = condition.bParentActive && !condition.bAnyTaken;
					condition.bAnyTaken = true;
				} else {
					bool bBranch = false;
					if ( condition.bParentActive && !condition.bAnyTaken && !sirCookEvaluateCondition( sArgument, *pContext, &bBranch ) ) {
						*pContext->pDiagnostic = "unsupported shader elif expression";
						return SIR_SHADER_COOK_BAD_DIRECTIVE;
					}
					bActive = condition.bParentActive && !condition.bAnyTaken && bBranch;
					condition.bAnyTaken |= bActive;
				}
			}
			pContext->pManifest->sExpandedSource += sLine + '\n';
		} else if ( !bActive ) {
			pContext->pManifest->sExpandedSource.push_back( '\n' );
		} else if ( sWord == "define" || sWord == "undef" ) {
			const size_t iSpace = sArgument.find_first_of( " \t(" );
			const std::string sName = sArgument.substr( 0, iSpace );
			if ( !sirCookIsIdentifier( sName ) ) {
				*pContext->pDiagnostic = "invalid shader macro name";
				return SIR_SHADER_COOK_BAD_DIRECTIVE;
			}
			if ( sWord == "undef" ) {
				pContext->szMacros.erase( sName );
			} else {
				const std::string sValue = iSpace == std::string::npos ? "1" : sirCookTrim( sArgument.substr( iSpace ) );
				pContext->szMacros[sName] = sValue.empty() ? "1" : sValue;
			}
			pContext->pManifest->sExpandedSource += sLine + '\n';
		} else {
			std::string sInclude;
			const int32_t nDirective = bInBlockComment ? 0 : sirCookReadDirective( sLine, &sInclude );
			if ( nDirective == -1 ) {
				*pContext->pDiagnostic = "unsupported include directive: " + sPath + ":" + std::to_string( nLine );
				return SIR_SHADER_COOK_BAD_DIRECTIVE;
			}
			if ( nDirective == 2 ) {
				pContext->szOnce.insert( sPath );
				pContext->pManifest->sExpandedSource.push_back( '\n' );
			} else if ( nDirective == 1 ) {
				const std::string sChild = ( std::filesystem::path( sPath ).parent_path() / sInclude ).lexically_normal().generic_string();
				pContext->pManifest->szFiles[nIndex].szIncludes.push_back( sChild );
				const sirShaderCookResult_t eResult = sirCookExpand( pContext, sChild, nDepth + 1 );
				if ( eResult != SIR_SHADER_COOK_OK ) {
					return eResult;
				}
				pContext->pManifest->sExpandedSource += "#line " + std::to_string( nLine + 1 ) + " \"" + sPath + "\"\n";
			} else {
				pContext->pManifest->sExpandedSource += sLine + '\n';
			}
		}
		for ( size_t iChar = 0; iChar + 1 < sLine.size(); ++iChar ) {
			if ( bInBlockComment ) {
				if ( sLine.compare( iChar, 2, "*/" ) == 0 ) {
					bInBlockComment = false;
					++iChar;
				}
			} else if ( sLine.compare( iChar, 2, "//" ) == 0 ) {
				break;
			} else if ( sLine.compare( iChar, 2, "/*" ) == 0 ) {
				bInBlockComment = true;
				++iChar;
			} else if ( sLine[iChar] == '"' ) {
				const size_t iQuote = sLine.find( '"', iChar + 1 );
				if ( iQuote != std::string::npos ) {
					iChar = iQuote;
				}
			}
		}
		if ( pContext->pManifest->sExpandedSource.size() > SIR_COOK_MAX_EXPANDED_BYTES ) {
			*pContext->pDiagnostic = "expanded shader source capacity exceeded";
			return SIR_SHADER_COOK_CAPACITY_EXCEEDED;
		}
		++nLine;
		iLine = iEnd == std::string::npos ? sSource.size() : iEnd + 1;
	}
	if ( !szConditions.empty() ) {
		*pContext->pDiagnostic = "unterminated shader conditional: " + sPath;
		return SIR_SHADER_COOK_BAD_DIRECTIVE;
	}
	pContext->szActive.erase( sPath );
	return SIR_SHADER_COOK_OK;
}

sirShaderCookResult_t sirShaderCookPreprocess( const sirShaderCookRequest_t& request, sirShaderCookManifest_t *pManifest,
                                               std::string *pDiagnostic )
{
	if ( pManifest == nullptr || pDiagnostic == nullptr ) {
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	*pManifest = {};
	pDiagnostic->clear();
	if ( !sirCookIsLogicalPath( request.sEntryFile ) || !sirCookIsIdentifier( request.sEntryPoint ) || request.szDefines.size() > 64 ) {
		*pDiagnostic = "invalid entry path, entry point or define count";
		return SIR_SHADER_COOK_INVALID_ARGUMENT;
	}
	bool bSpirv = false;
	const sirShaderCookResult_t eTarget = sirShaderCookResolveTarget( request.eBackend, request.nStage, &pManifest->sProfile, &bSpirv );
	if ( eTarget != SIR_SHADER_COOK_OK ) {
		*pDiagnostic = "shader target or stage is unsupported";
		return eTarget;
	}
	pManifest->bSpirv = bSpirv;
	pManifest->nRecipeVersion = SIR_COOK_RECIPE_VERSION;
	pManifest->sEntryPoint = request.sEntryPoint;
	pManifest->szDefines = request.szDefines;
	std::sort( pManifest->szDefines.begin(), pManifest->szDefines.end(),
	           []( const auto& left, const auto& right ) { return left.sName < right.sName; } );
	for ( size_t iDefine = 0; iDefine < pManifest->szDefines.size(); ++iDefine ) {
		const sirShaderCookDefine_t& definition = pManifest->szDefines[iDefine];
		if ( !sirCookIsIdentifier( definition.sName ) || ( iDefine != 0 && pManifest->szDefines[iDefine - 1].sName == definition.sName ) ) {
			*pDiagnostic = "invalid or duplicate shader define";
			return SIR_SHADER_COOK_INVALID_ARGUMENT;
		}
		for ( char nChar : definition.sValue ) {
			if ( nChar < '!' || nChar > '~' || nChar == '"' || nChar == '\\' ) {
				*pDiagnostic = "unsafe shader define value";
				return SIR_SHADER_COOK_INVALID_ARGUMENT;
			}
		}
		pManifest->nDefinesHash = sirCookHash( pManifest->nDefinesHash == 0 ? SIR_COOK_FNV_BASIS : pManifest->nDefinesHash,
		                                       definition.sName + "=" + definition.sValue );
	}
	if ( pManifest->nDefinesHash == 0 ) {
		pManifest->nDefinesHash = SIR_COOK_FNV_BASIS;
	}
	std::error_code eError;
	sirShaderCookContext_t context = {};
	context.sCanonicalRoot = std::filesystem::canonical( request.sRoot, eError );
	if ( eError || !std::filesystem::is_directory( context.sCanonicalRoot ) ) {
		*pDiagnostic = "shader root must be an existing directory";
		return SIR_SHADER_COOK_IO_ERROR;
	}
	context.pManifest = pManifest;
	context.pDiagnostic = pDiagnostic;
	for ( const sirShaderCookDefine_t& definition : pManifest->szDefines ) {
		context.szMacros.emplace( definition.sName, definition.sValue );
	}
	const sirShaderCookResult_t eResult = sirCookExpand( &context, request.sEntryFile, 0 );
	if ( eResult != SIR_SHADER_COOK_OK ) {
		*pManifest = {};
		return eResult;
	}
	std::sort( pManifest->szFiles.begin(), pManifest->szFiles.end(),
	           []( const auto& left, const auto& right ) { return left.sPath < right.sPath; } );
	uint64_t nClosure = SIR_COOK_FNV_BASIS;
	for ( sirShaderCookFile_t& file : pManifest->szFiles ) {
		std::sort( file.szIncludes.begin(), file.szIncludes.end() );
		file.szIncludes.erase( std::unique( file.szIncludes.begin(), file.szIncludes.end() ), file.szIncludes.end() );
		nClosure = sirCookHash( nClosure, file.sPath );
		nClosure = sirCookHash( nClosure, std::to_string( file.nContentHash ) );
		for ( const std::string& sInclude : file.szIncludes ) {
			nClosure = sirCookHash( nClosure, sInclude );
		}
	}
	pManifest->nSourceHash = sirCookHash( SIR_COOK_FNV_BASIS, pManifest->sExpandedSource );
	pManifest->nClosureHash =
	    sirCookHash( nClosure, request.sEntryPoint + ":" + pManifest->sProfile + ":" + std::to_string( pManifest->nDefinesHash ) + ":" +
		                           std::to_string( ( uint32_t )request.eBackend ) + ":" + std::to_string( SIR_COOK_RECIPE_VERSION ) );
	return SIR_SHADER_COOK_OK;
}
