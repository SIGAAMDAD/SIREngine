/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Editor/Scene/SIREditorComplexWriter.h>
#include <string.h>

static bool sirEditorComplexWord( char c ) noexcept
{
	return ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_' || c == '.' || c == '+' ||
	       c == '-' || c == '/' || c == ':';
}

bool sirEditorValidateComplexValue( const char *pszValue, uint32_t nLength ) noexcept
{
	if ( pszValue == nullptr || nLength == 0 || nLength > 1024 * 1024 ) {
		return false;
	}
	char szStack[128] = {};
	uint32_t nDepth = 0;
	bool bQuote = false;
	bool bEscape = false;
	bool bSawToken = false;
	for ( uint32_t i = 0; i < nLength; ++i ) {
		const char c = pszValue[i];
		if ( c == '\0' || c == '\r' || ( uint8_t )c < 0x20 && c != '\n' && c != '\t' ) {
			return false;
		}
		if ( bQuote ) {
			if ( bEscape ) {
				bEscape = false;
			} else if ( c == '\\' ) {
				bEscape = true;
			} else if ( c == '"' ) {
				bQuote = false;
			} else if ( c == '\n' ) {
				return false;
			}
			continue;
		}
		if ( c == '"' ) {
			bQuote = true;
			bSawToken = true;
		} else if ( c == '(' || c == '[' || c == '{' ) {
			if ( nDepth == sizeof( szStack ) ) {
				return false;
			}
			szStack[nDepth++] = c;
			bSawToken = true;
		} else if ( c == ')' || c == ']' || c == '}' ) {
			if ( nDepth == 0 ) {
				return false;
			}
			const char open = szStack[--nDepth];
			if ( ( open == '(' && c != ')' ) || ( open == '[' && c != ']' ) || ( open == '{' && c != '}' ) ) {
				return false;
			}
		} else if ( c == ',' || c == '=' ) {
			if ( nDepth == 0 ) {
				return false;
			}
		} else if ( c == ' ' || c == '\t' || c == '\n' ) {
			continue;
		} else if ( sirEditorComplexWord( c ) ) {
			bSawToken = true;
		} else {
			return false;
		}
	}
	return bSawToken && !bQuote && !bEscape && nDepth == 0;
}

static bool sirEditorComplexOverlap( const void *pInput, size_t nBytes, uintptr_t nOutput, size_t nCapacity ) noexcept
{
	if ( nBytes == 0 ) {
		return false;
	}
	const uintptr_t nStart = ( uintptr_t )pInput;
	return nBytes > UINTPTR_MAX - nStart || ( nStart < nOutput + nCapacity && nOutput < nStart + nBytes );
}

sirEditorTextWriteResult_t sirEditorWriteComplexPatches( const char *pszSource, size_t nSourceBytes,
                                                         const sirEditorTextPropertyIndex_t& index, const sirEditorTextEdit_t *pEdits,
                                                         uint32_t nEdits, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept
{
	if ( pOutBytes == nullptr || pszSource == nullptr || ( nEdits != 0 && pEdits == nullptr ) ||
	     ( pszOutput == nullptr && nCapacity != 0 ) || index.nCount > index.nCapacity ||
	     ( index.nCount != 0 && index.pProperties == nullptr ) || nEdits > index.nCount || nSourceBytes == SIZE_MAX ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	const uintptr_t nDestination = ( uintptr_t )pszOutput;
	if ( pszOutput != nullptr && ( nCapacity > UINTPTR_MAX - nDestination ||
	                               sirEditorComplexOverlap( pOutBytes, sizeof( *pOutBytes ), nDestination, nCapacity ) ) ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	*pOutBytes = 0;
	size_t nNeeded = nSourceBytes;
	for ( uint32_t i = 0; i < nEdits; ++i ) {
		const sirEditorTextEdit_t& edit = pEdits[i];
		if ( edit.iProperty >= index.nCount || !sirEditorValidateComplexValue( edit.pszReplacement, edit.nReplacementBytes ) ) {
			return SIR_EDITOR_WRITE_INVALID_SCALAR;
		}
		const sirEditorTextProperty_t& property = index.pProperties[edit.iProperty];
		if ( property.nValueLength == 0 || property.nValueOffset > nSourceBytes ||
		     property.nValueLength > nSourceBytes - property.nValueOffset ) {
			return SIR_EDITOR_WRITE_INVALID_SOURCE;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			const sirEditorTextProperty_t& previous = index.pProperties[pEdits[j].iProperty];
			if ( pEdits[j].iProperty == edit.iProperty ||
			     ( ( uint64_t )previous.nValueOffset < ( uint64_t )property.nValueOffset + property.nValueLength &&
			       ( uint64_t )property.nValueOffset < ( uint64_t )previous.nValueOffset + previous.nValueLength ) ) {
				return SIR_EDITOR_WRITE_DUPLICATE_PATCH;
			}
		}
		if ( edit.nReplacementBytes > SIZE_MAX - nNeeded ) {
			return SIR_EDITOR_WRITE_OUT_OF_CAPACITY;
		}
		nNeeded += edit.nReplacementBytes;
		nNeeded -= property.nValueLength;
	}
	*pOutBytes = nNeeded;
	if ( pszOutput == nullptr && nCapacity == 0 ) {
		return SIR_EDITOR_WRITE_OK;
	}
	if ( pszOutput == nullptr || nNeeded == SIZE_MAX || nCapacity <= nNeeded ) {
		return SIR_EDITOR_WRITE_OUT_OF_CAPACITY;
	}
	if ( sirEditorComplexOverlap( pszSource, nSourceBytes, nDestination, nCapacity ) ||
	     ( nEdits != 0 && ( nEdits > SIZE_MAX / sizeof( *pEdits ) ||
	                        sirEditorComplexOverlap( pEdits, ( size_t )nEdits * sizeof( *pEdits ), nDestination, nCapacity ) ) ) ||
	     ( index.nCount != 0 && ( index.nCount > SIZE_MAX / sizeof( *index.pProperties ) ||
	                              sirEditorComplexOverlap( index.pProperties, ( size_t )index.nCount * sizeof( *index.pProperties ),
	                                                       nDestination, nCapacity ) ) ) ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nEdits; ++i ) {
		if ( sirEditorComplexOverlap( pEdits[i].pszReplacement, pEdits[i].nReplacementBytes, nDestination, nCapacity ) ) {
			return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
		}
	}
	size_t nRead = 0;
	size_t nWrite = 0;
	for ( uint32_t i = 0; i < nEdits; ++i ) {
		uint32_t iNext = UINT32_MAX;
		uint32_t nNextOffset = UINT32_MAX;
		for ( uint32_t j = 0; j < nEdits; ++j ) {
			const uint32_t nOffset = index.pProperties[pEdits[j].iProperty].nValueOffset;
			if ( nOffset >= nRead && nOffset < nNextOffset ) {
				iNext = j;
				nNextOffset = nOffset;
			}
		}
		if ( iNext == UINT32_MAX ) {
			return SIR_EDITOR_WRITE_INVALID_SOURCE;
		}
		const sirEditorTextEdit_t& edit = pEdits[iNext];
		const sirEditorTextProperty_t& property = index.pProperties[edit.iProperty];
		const size_t nCopy = ( size_t )nNextOffset - nRead;
		memcpy( pszOutput + nWrite, pszSource + nRead, nCopy );
		nWrite += nCopy;
		memcpy( pszOutput + nWrite, edit.pszReplacement, edit.nReplacementBytes );
		nWrite += edit.nReplacementBytes;
		nRead = property.nValueOffset + property.nValueLength;
	}
	memcpy( pszOutput + nWrite, pszSource + nRead, nSourceBytes - nRead );
	pszOutput[nNeeded] = '\0';
	return SIR_EDITOR_WRITE_OK;
}
