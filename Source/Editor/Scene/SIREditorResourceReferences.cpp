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

#include <Editor/Scene/SIREditorResourceReferences.h>

#include <limits.h>
#include <string.h>

static bool sirEditorReferenceToken( char c ) noexcept
{
	return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_';
}

static bool sirEditorReferenceSpace( char c ) noexcept
{
	return c == ' ' || c == '\t' || c == '\r';
}

static bool sirEditorReferenceOverlaps( const void *pLeft, size_t nLeft, const void *pRight, size_t nRight ) noexcept
{
	if ( pLeft == nullptr || pRight == nullptr || nLeft == 0 || nRight == 0 ) {
		return false;
	}
	const uintptr_t nA = ( uintptr_t )pLeft;
	const uintptr_t nB = ( uintptr_t )pRight;
	if ( nLeft > UINTPTR_MAX - nA || nRight > UINTPTR_MAX - nB ) {
		return true;
	}
	return nA < nB + nRight && nB < nA + nLeft;
}

static bool sirEditorReferenceIndexValid( const sirEditorResourceReferenceScan_t& context ) noexcept
{
	if ( context.pIndex == nullptr || context.pszSource == nullptr || context.nSourceBytes > UINT32_MAX ) {
		return false;
	}
	const sirEditorTextResourceIndex_t& index = *context.pIndex;
	if ( index.nCount > index.nCapacity || ( index.nCount != 0 && index.pResources == nullptr ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < index.nCount; ++i ) {
		const sirEditorTextResource_t& resource = index.pResources[i];
		if ( resource.nStableKey == 0 || resource.nSourceLine == 0 || resource.nIdLength == 0 ||
		     resource.nIdOffset > context.nSourceBytes || resource.nIdLength > context.nSourceBytes - resource.nIdOffset ||
		     ( resource.eKind != SIR_EDITOR_RESOURCE_EXTERNAL && resource.eKind != SIR_EDITOR_RESOURCE_EMBEDDED ) ) {
			return false;
		}
		if ( i != 0 && resource.nSourceLine <= index.pResources[i - 1].nSourceLine ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( resource.nStableKey == index.pResources[j].nStableKey ) {
				return false;
			}
		}
	}
	return true;
}

static sirEditorTextResourceResult_t sirEditorReferencePass( const sirEditorResourceReferenceScan_t& context, bool bWrite,
                                                             uint32_t *pOutCount ) noexcept
{
	const char *pszSource = context.pszSource;
	const char *pEnd = pszSource + context.nSourceBytes;
	bool bQuoted = false;
	bool bEscaped = false;
	bool bComment = false;
	bool bLineStart = true;
	uint32_t nLine = 1;
	uint64_t nOwnerKey = 0;
	uint32_t iNextResource = 0;
	uint32_t nReferences = 0;
	for ( const char *pCursor = pszSource; pCursor < pEnd; ++pCursor ) {
		const char c = *pCursor;
		if ( c == '\n' ) {
			++nLine;
			bComment = false;
			bLineStart = true;
			continue;
		}
		if ( bComment ) {
			continue;
		}
		if ( bEscaped ) {
			bEscaped = false;
			continue;
		}
		if ( bQuoted ) {
			if ( c == '\\' ) {
				bEscaped = true;
			} else if ( c == '"' ) {
				bQuoted = false;
			}
			continue;
		}
		if ( bLineStart && sirEditorReferenceSpace( c ) ) {
			continue;
		}
		if ( bLineStart ) {
			bLineStart = false;
			// Section headers are lowercase tags. Never infer a dependency owner
			// from a property array or from text inside a quoted multiline value.
			if ( c == '[' && pCursor + 1 < pEnd && pCursor[1] >= 'a' && pCursor[1] <= 'z' ) {
				nOwnerKey = 0;
				// Resource declarations are validated in ascending source-line order.
				// Consume each once instead of searching the entire index per section.
				while ( iNextResource < context.pIndex->nCount && context.pIndex->pResources[iNextResource].nSourceLine < nLine ) {
					++iNextResource;
				}
				if ( iNextResource < context.pIndex->nCount && context.pIndex->pResources[iNextResource].nSourceLine == nLine ) {
					const sirEditorTextResource_t& item = context.pIndex->pResources[iNextResource++];
					if ( item.eKind == SIR_EDITOR_RESOURCE_EMBEDDED ) {
						nOwnerKey = item.nStableKey;
					}
				}
			}
		}
		if ( c == '"' ) {
			bQuoted = true;
			continue;
		}
		if ( c == '#' || c == ';' ) {
			bComment = true;
			continue;
		}
		if ( pCursor > pszSource && sirEditorReferenceToken( pCursor[-1] ) ) {
			continue;
		}
		sirEditorTextResourceKind_t eKind = SIR_EDITOR_RESOURCE_EXTERNAL;
		if ( ( size_t )( pEnd - pCursor ) < 12 ) {
			continue;
		}
		if ( memcmp( pCursor, "ExtResource(", 12 ) == 0 ) {
			eKind = SIR_EDITOR_RESOURCE_EXTERNAL;
		} else if ( memcmp( pCursor, "SubResource(", 12 ) == 0 ) {
			eKind = SIR_EDITOR_RESOURCE_EMBEDDED;
		} else {
			continue;
		}
		const char *pCall = pCursor + 12;
		while ( pCall < pEnd && sirEditorReferenceSpace( *pCall ) ) {
			++pCall;
		}
		if ( pCall == pEnd || *pCall++ != '"' ) {
			return SIR_EDITOR_RESOURCE_INVALID_REFERENCE;
		}
		const char *pId = pCall;
		while ( pCall < pEnd && sirEditorReferenceToken( *pCall ) ) {
			++pCall;
		}
		if ( pCall == pId || pCall == pEnd || *pCall++ != '"' ) {
			return SIR_EDITOR_RESOURCE_INVALID_REFERENCE;
		}
		const uint32_t nIdLength = ( uint32_t )( pCall - pId - 1 );
		while ( pCall < pEnd && sirEditorReferenceSpace( *pCall ) ) {
			++pCall;
		}
		if ( pCall == pEnd || *pCall != ')' ) {
			return SIR_EDITOR_RESOURCE_INVALID_REFERENCE;
		}
		uint32_t iTarget = 0;
		if ( sirEditorFindTextResource( pszSource, *context.pIndex, eKind, pId, nIdLength, &iTarget ) != SIR_EDITOR_RESOURCE_OK ) {
			return SIR_EDITOR_RESOURCE_MISSING_REFERENCE;
		}
		if ( nReferences == UINT32_MAX ) {
			return SIR_EDITOR_RESOURCE_OUT_OF_CAPACITY;
		}
		if ( bWrite ) {
			const sirEditorTextResource_t& target = context.pIndex->pResources[iTarget];
			context.pOutput[nReferences] = { nOwnerKey, target.nStableKey, iTarget, ( uint32_t )( pCursor - pszSource ), nLine };
		}
		++nReferences;
		pCursor = pCall;
	}
	*pOutCount = nReferences;
	return SIR_EDITOR_RESOURCE_OK;
}

sirEditorTextResourceResult_t sirEditorScanResourceReferences( const sirEditorResourceReferenceScan_t& context,
                                                               uint32_t *pOutCount ) noexcept
{
	if ( pOutCount == nullptr ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	// Never initialize a count through an alias of the immutable input or index.
	if ( sirEditorReferenceOverlaps( pOutCount, sizeof( *pOutCount ), context.pszSource, context.nSourceBytes ) ||
	     sirEditorReferenceOverlaps( pOutCount, sizeof( *pOutCount ), context.pIndex, sizeof( sirEditorTextResourceIndex_t ) ) ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	if ( !sirEditorReferenceIndexValid( context ) || ( context.pOutput == nullptr && context.nCapacity != 0 ) ) {
		*pOutCount = 0;
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
#if SIZE_MAX == UINT32_MAX
	if ( context.nCapacity > SIZE_MAX / sizeof( sirEditorResourceReference_t ) ||
	     context.pIndex->nCount > SIZE_MAX / sizeof( sirEditorTextResource_t ) ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
#endif
	const size_t nOutputBytes = ( size_t )context.nCapacity * sizeof( sirEditorResourceReference_t );
	const size_t nIndexBytes = ( size_t )context.pIndex->nCount * sizeof( sirEditorTextResource_t );
	if ( sirEditorReferenceOverlaps( pOutCount, sizeof( *pOutCount ), context.pOutput, nOutputBytes ) ||
	     sirEditorReferenceOverlaps( pOutCount, sizeof( *pOutCount ), context.pIndex->pResources, nIndexBytes ) ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	*pOutCount = 0;
	if ( context.pOutput != nullptr && ( ( uintptr_t )context.pOutput % alignof( sirEditorResourceReference_t ) ) != 0 ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	if ( context.pOutput != nullptr &&
	     ( sirEditorReferenceOverlaps( context.pOutput, nOutputBytes, context.pszSource, context.nSourceBytes ) ||
	       sirEditorReferenceOverlaps( context.pOutput, nOutputBytes, context.pIndex->pResources, nIndexBytes ) ||
	       sirEditorReferenceOverlaps( context.pOutput, nOutputBytes, context.pIndex, sizeof( *context.pIndex ) ) ) ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	uint32_t nRequired = 0;
	const sirEditorTextResourceResult_t eFirst = sirEditorReferencePass( context, false, &nRequired );
	if ( eFirst != SIR_EDITOR_RESOURCE_OK ) {
		return eFirst;
	}
	*pOutCount = nRequired;
	if ( context.pOutput == nullptr ) {
		return SIR_EDITOR_RESOURCE_OK;
	}
	if ( context.nCapacity < nRequired ) {
		return SIR_EDITOR_RESOURCE_OUT_OF_CAPACITY;
	}
	uint32_t nWritten = 0;
	const sirEditorTextResourceResult_t eSecond = sirEditorReferencePass( context, true, &nWritten );
	return eSecond == SIR_EDITOR_RESOURCE_OK && nWritten == nRequired ? SIR_EDITOR_RESOURCE_OK : SIR_EDITOR_RESOURCE_INVALID_REFERENCE;
}
