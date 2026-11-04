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

#include <Editor/Scene/SIREditorTextProperties.h>

#include <limits.h>
#include <string.h>

static constexpr uint32_t SIR_EDITOR_PROPERTY_MAX_COUNT = 65536;
static constexpr uint32_t SIR_EDITOR_PROPERTY_MAX_NESTING = 64;

static bool sirEditorPropertySpace( char c ) noexcept
{
	return c == ' ' || c == '\t' || c == '\r';
}

static bool sirEditorPropertyRangeOverlap( const void *pFirst, size_t nFirst, const void *pSecond, size_t nSecond ) noexcept
{
	const uintptr_t nA = ( uintptr_t )pFirst;
	const uintptr_t nB = ( uintptr_t )pSecond;
	if ( nFirst > UINTPTR_MAX - nA || nSecond > UINTPTR_MAX - nB ) {
		return true;
	}
	return nA < nB + nSecond && nB < nA + nFirst;
}

static bool sirEditorPropertyNodeTag( const char *pStart, const char *pEnd ) noexcept
{
	const size_t nLength = ( size_t )( pEnd - pStart );
	return nLength >= 6 && memcmp( pStart, "[node", 5 ) == 0 && ( pStart[5] == ' ' || pStart[5] == '\t' || pStart[5] == ']' );
}

static sirEditorTextPropertyResult_t sirEditorPropertyConsumeValue( const char *pSource, const char *pEnd, const char **ppEnd,
                                                                    uint32_t *pLine ) noexcept
{
	char szNesting[SIR_EDITOR_PROPERTY_MAX_NESTING];
	uint32_t nDepth = 0;
	bool bQuoted = false;
	bool bEscape = false;
	const char *pCursor = pSource;
	while ( pCursor < pEnd ) {
		const char c = *pCursor;
		if ( c == '\0' ) {
			return SIR_EDITOR_PROPERTY_MALFORMED_VALUE;
		}
		if ( c == '\n' ) {
			++*pLine;
			if ( nDepth == 0 && !bQuoted ) {
				*ppEnd = pCursor;
				return SIR_EDITOR_PROPERTY_OK;
			}
		}
		if ( bEscape ) {
			bEscape = false;
		} else if ( bQuoted && c == '\\' ) {
			bEscape = true;
		} else if ( c == '"' ) {
			bQuoted = !bQuoted;
		} else if ( !bQuoted ) {
			if ( c == '(' || c == '[' || c == '{' ) {
				if ( nDepth == SIR_EDITOR_PROPERTY_MAX_NESTING ) {
					return SIR_EDITOR_PROPERTY_MALFORMED_VALUE;
				}
				szNesting[nDepth++] = c;
			} else if ( c == ')' || c == ']' || c == '}' ) {
				if ( nDepth == 0 ) {
					return SIR_EDITOR_PROPERTY_MALFORMED_VALUE;
				}
				const char previous = szNesting[--nDepth];
				if ( ( c == ')' && previous != '(' ) || ( c == ']' && previous != '[' ) || ( c == '}' && previous != '{' ) ) {
					return SIR_EDITOR_PROPERTY_MALFORMED_VALUE;
				}
			}
		}
		++pCursor;
	}
	if ( bQuoted || bEscape || nDepth != 0 ) {
		return SIR_EDITOR_PROPERTY_MALFORMED_VALUE;
	}
	*ppEnd = pEnd;
	return SIR_EDITOR_PROPERTY_OK;
}

sirEditorTextPropertyResult_t sirEditorScanNodeTextProperties( const char *pszSource, size_t nSourceBytes,
                                                               const sirEditorTextSceneIndex_t &scene,
                                                               sirEditorTextPropertyIndex_t *pIndex ) noexcept
{
	if ( pIndex == nullptr ) {
		return SIR_EDITOR_PROPERTY_INVALID_ARGUMENT;
	}
	pIndex->nCount = 0;
	pIndex->nErrorLine = 0;
	const bool bMeasure = pIndex->pProperties == nullptr && pIndex->nCapacity == 0;
	if ( pszSource == nullptr || nSourceBytes == 0 || nSourceBytes > UINT32_MAX || scene.nNodeCount == 0 ||
	     scene.nNodeCount > scene.storage.nNodeCapacity || scene.storage.pNodes == nullptr ||
	     ( pIndex->nCapacity != 0 && pIndex->pProperties == nullptr ) ) {
		return SIR_EDITOR_PROPERTY_INVALID_ARGUMENT;
	}
	if ( !bMeasure &&
	     ( sirEditorPropertyRangeOverlap( pszSource, nSourceBytes, pIndex->pProperties,
	                                      ( size_t )pIndex->nCapacity * sizeof( sirEditorTextProperty_t ) ) ||
	       sirEditorPropertyRangeOverlap( scene.storage.pNodes, ( size_t )scene.nNodeCount * sizeof( sirEditorTextSceneNode_t ),
	                                      pIndex->pProperties, ( size_t )pIndex->nCapacity * sizeof( sirEditorTextProperty_t ) ) ) ) {
		return SIR_EDITOR_PROPERTY_INVALID_ARGUMENT;
	}

	const char *pEnd = pszSource + nSourceBytes;
	const char *pCursor = pszSource;
	uint32_t nLine = 1;
	uint32_t nCurrentNode = UINT32_MAX;
	uint32_t nNextNode = 0;
	uint32_t nProperties = 0;
	sirEditorTextPropertyResult_t eResult = SIR_EDITOR_PROPERTY_OK;

	while ( pCursor < pEnd ) {
		const char *pStart = pCursor;
		while ( pStart < pEnd && sirEditorPropertySpace( *pStart ) ) {
			++pStart;
		}
		const char *pLineEnd = ( const char * )memchr( pStart, '\n', ( size_t )( pEnd - pStart ) );
		if ( pLineEnd == nullptr ) {
			pLineEnd = pEnd;
		}
		if ( pStart == pLineEnd || *pStart == '#' || *pStart == ';' ) {
			pCursor = pLineEnd;
		} else if ( *pStart == '[' ) {
			if ( sirEditorPropertyNodeTag( pStart, pLineEnd ) ) {
				if ( nNextNode >= scene.nNodeCount || scene.storage.pNodes[nNextNode].nSourceLine != nLine ) {
					eResult = SIR_EDITOR_PROPERTY_INVALID_SOURCE;
					break;
				}
				nCurrentNode = nNextNode++;
			} else {
				nCurrentNode = UINT32_MAX;
			}
			pCursor = pLineEnd;
		} else if ( nCurrentNode == UINT32_MAX ) {
			pCursor = pLineEnd;
		} else {
			const char *pEquals = ( const char * )memchr( pStart, '=', ( size_t )( pLineEnd - pStart ) );
			if ( pEquals == nullptr ) {
				eResult = SIR_EDITOR_PROPERTY_INVALID_SOURCE;
				break;
			}
			const char *pNameEnd = pEquals;
			while ( pNameEnd > pStart && sirEditorPropertySpace( pNameEnd[-1] ) ) {
				--pNameEnd;
			}
			if ( pNameEnd == pStart ) {
				eResult = SIR_EDITOR_PROPERTY_INVALID_SOURCE;
				break;
			}
			for ( const char *pChar = pStart; pChar < pNameEnd; ++pChar ) {
				if ( sirEditorPropertySpace( *pChar ) || *pChar == '\0' ) {
					eResult = SIR_EDITOR_PROPERTY_INVALID_SOURCE;
					break;
				}
			}
			if ( eResult != SIR_EDITOR_PROPERTY_OK ) {
				break;
			}
			const char *pValue = pEquals + 1;
			while ( pValue < pLineEnd && sirEditorPropertySpace( *pValue ) ) {
				++pValue;
			}
			if ( pValue == pLineEnd ) {
				eResult = SIR_EDITOR_PROPERTY_MALFORMED_VALUE;
				break;
			}
			const uint32_t nStartLine = nLine;
			const char *pValueEnd = nullptr;
			eResult = sirEditorPropertyConsumeValue( pValue, pEnd, &pValueEnd, &nLine );
			if ( eResult != SIR_EDITOR_PROPERTY_OK ) {
				break;
			}
			while ( pValueEnd > pValue && ( pValueEnd[-1] == ' ' || pValueEnd[-1] == '\t' || pValueEnd[-1] == '\r' ) ) {
				--pValueEnd;
			}
			if ( nProperties == SIR_EDITOR_PROPERTY_MAX_COUNT ) {
				eResult = SIR_EDITOR_PROPERTY_TOO_MANY;
				break;
			}
			if ( !bMeasure && nProperties >= pIndex->nCapacity ) {
				eResult = SIR_EDITOR_PROPERTY_OUT_OF_CAPACITY;
				break;
			}
			if ( !bMeasure ) {
				sirEditorTextProperty_t &property = pIndex->pProperties[nProperties];
				property.nNodeId = scene.storage.pNodes[nCurrentNode].node.nId;
				property.nSourceLine = nStartLine;
				property.nNameOffset = ( uint32_t )( pStart - pszSource );
				property.nNameLength = ( uint32_t )( pNameEnd - pStart );
				property.nValueOffset = ( uint32_t )( pValue - pszSource );
				property.nValueLength = ( uint32_t )( pValueEnd - pValue );
			}
			++nProperties;
			pCursor = pValueEnd;
			if ( pCursor < pEnd && *pCursor == '\n' ) {
				++pCursor;
			}
			// The value scanner already accounted for every consumed newline.
			continue;
		}
		if ( pCursor < pEnd && *pCursor == '\n' ) {
			++pCursor;
			++nLine;
		} else if ( pCursor == pEnd ) {
			break;
		}
	}
	if ( eResult == SIR_EDITOR_PROPERTY_OK && nNextNode != scene.nNodeCount ) {
		eResult = SIR_EDITOR_PROPERTY_INVALID_SOURCE;
	}
	if ( eResult != SIR_EDITOR_PROPERTY_OK ) {
		pIndex->nErrorLine = nLine;
		return eResult;
	}
	pIndex->nCount = nProperties;
	return SIR_EDITOR_PROPERTY_OK;
}
