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

#include <Editor/Scene/SIREditorTextWriter.h>

#include <string.h>

static bool sirEditorIsDigit( char nCharacter ) noexcept
{
	return nCharacter >= '0' && nCharacter <= '9';
}

static bool sirEditorIsHex( char nCharacter ) noexcept
{
	return sirEditorIsDigit( nCharacter ) || ( nCharacter >= 'a' && nCharacter <= 'f' ) || ( nCharacter >= 'A' && nCharacter <= 'F' );
}

// A serialized destination may not overwrite the metadata still driving the
// edit transaction. Pointer-range overflow is treated as invalid input.
static bool sirEditorWriterOutputOverlaps( const void *pInput, size_t nInputBytes, uintptr_t nOutputPtr, size_t nCapacity ) noexcept
{
	if ( nInputBytes == 0 ) {
		return false;
	}
	const uintptr_t nInputPtr = ( uintptr_t )pInput;
	if ( nInputBytes > UINTPTR_MAX - nInputPtr ) {
		return true;
	}
	return nInputPtr < nOutputPtr + nCapacity && nOutputPtr < nInputPtr + nInputBytes;
}

static bool sirEditorScalarValid( const char *pszText, uint32_t nLength ) noexcept
{
	if ( pszText == nullptr || nLength == 0 ) {
		return false;
	}
	if ( ( nLength == 4 && memcmp( pszText, "true", 4 ) == 0 ) || ( nLength == 5 && memcmp( pszText, "false", 5 ) == 0 ) ) {
		return true;
	}
	if ( pszText[0] == '"' ) {
		if ( nLength < 2 || pszText[nLength - 1] != '"' ) {
			return false;
		}
		for ( uint32_t iCharacter = 1; iCharacter < nLength - 1; ++iCharacter ) {
			const char nCurrent = pszText[iCharacter];
			if ( nCurrent == '\n' || nCurrent == '\r' || ( unsigned char )nCurrent < 0x20 || nCurrent == '"' ) {
				return false;
			}
			if ( nCurrent != '\\' ) {
				continue;
			}
			if ( ++iCharacter == nLength - 1 ) {
				return false;
			}
			const char nEscaped = pszText[iCharacter];
			if ( nEscaped == 'u' ) {
				if ( iCharacter + 4 >= nLength - 1 ) {
					return false;
				}
				for ( uint32_t iDigit = 0; iDigit < 4; ++iDigit ) {
					if ( !sirEditorIsHex( pszText[++iCharacter] ) ) {
						return false;
					}
				}
			} else if ( nEscaped != '"' && nEscaped != '\\' && nEscaped != 'n' && nEscaped != 'r' && nEscaped != 't' ) {
				return false;
			}
		}
		return true;
	}
	uint32_t iCharacter = 0;
	if ( pszText[0] == '+' || pszText[0] == '-' ) {
		++iCharacter;
	}
	bool bDigits = false;
	while ( iCharacter < nLength && sirEditorIsDigit( pszText[iCharacter] ) ) {
		++iCharacter;
		bDigits = true;
	}
	if ( iCharacter < nLength && pszText[iCharacter] == '.' ) {
		++iCharacter;
		while ( iCharacter < nLength && sirEditorIsDigit( pszText[iCharacter] ) ) {
			++iCharacter;
			bDigits = true;
		}
	}
	if ( !bDigits ) {
		return false;
	}
	if ( iCharacter < nLength && ( pszText[iCharacter] == 'e' || pszText[iCharacter] == 'E' ) ) {
		++iCharacter;
		if ( iCharacter < nLength && ( pszText[iCharacter] == '+' || pszText[iCharacter] == '-' ) ) {
			++iCharacter;
		}
		const uint32_t iExponent = iCharacter;
		while ( iCharacter < nLength && sirEditorIsDigit( pszText[iCharacter] ) ) {
			++iCharacter;
		}
		if ( iCharacter == iExponent ) {
			return false;
		}
	}
	return iCharacter == nLength;
}

sirEditorTextWriteResult_t sirEditorWriteScalarPatches( const char *pszSource, size_t nSourceBytes,
                                                        const sirEditorTextPropertyIndex_t& index, const sirEditorTextEdit_t *pEdits,
                                                        uint32_t nEdits, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept
{
	if ( pOutBytes == nullptr || pszSource == nullptr || ( nEdits != 0 && pEdits == nullptr ) ||
	     ( pszOutput == nullptr && nCapacity != 0 ) || ( index.nCount != 0 && index.pProperties == nullptr ) ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	if ( pszOutput != nullptr ) {
		const uintptr_t nOutputPtr = ( uintptr_t )pszOutput;
		if ( nCapacity > UINTPTR_MAX - nOutputPtr ||
		     sirEditorWriterOutputOverlaps( pOutBytes, sizeof( *pOutBytes ), nOutputPtr, nCapacity ) ) {
			return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
		}
	}
	*pOutBytes = 0;
	if ( index.nCount > index.nCapacity || nEdits > index.nCount || nSourceBytes == SIZE_MAX ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	for ( uint32_t iProperty = 0; iProperty < index.nCount; ++iProperty ) {
		const sirEditorTextProperty_t& property = index.pProperties[iProperty];
		if ( property.nValueLength == 0 || property.nValueOffset > nSourceBytes ||
		     property.nValueLength > nSourceBytes - property.nValueOffset ) {
			return SIR_EDITOR_WRITE_INVALID_SOURCE;
		}
	}
	size_t nOutputBytes = nSourceBytes;
	for ( uint32_t iEdit = 0; iEdit < nEdits; ++iEdit ) {
		const sirEditorTextEdit_t& edit = pEdits[iEdit];
		if ( edit.iProperty >= index.nCount || !sirEditorScalarValid( edit.pszReplacement, edit.nReplacementBytes ) ) {
			return SIR_EDITOR_WRITE_INVALID_SCALAR;
		}
		const sirEditorTextProperty_t& property = index.pProperties[edit.iProperty];
		for ( uint32_t iPrior = 0; iPrior < iEdit; ++iPrior ) {
			const sirEditorTextProperty_t& prior = index.pProperties[pEdits[iPrior].iProperty];
			if ( edit.iProperty == pEdits[iPrior].iProperty ||
			     ( property.nValueOffset < ( uint64_t )prior.nValueOffset + prior.nValueLength &&
			       prior.nValueOffset < ( uint64_t )property.nValueOffset + property.nValueLength ) ) {
				return SIR_EDITOR_WRITE_DUPLICATE_PATCH;
			}
		}
		if ( edit.nReplacementBytes > SIZE_MAX - nOutputBytes ) {
			return SIR_EDITOR_WRITE_OUT_OF_CAPACITY;
		}
		nOutputBytes += edit.nReplacementBytes;
		nOutputBytes -= property.nValueLength;
	}
	*pOutBytes = nOutputBytes;
	if ( pszOutput == nullptr && nCapacity == 0 ) {
		return SIR_EDITOR_WRITE_OK;
	}
	if ( pszOutput == nullptr || nOutputBytes == SIZE_MAX || nCapacity <= nOutputBytes ) {
		return SIR_EDITOR_WRITE_OUT_OF_CAPACITY;
	}
	const uintptr_t nSourcePtr = ( uintptr_t )pszSource;
	const uintptr_t nOutputPtr = ( uintptr_t )pszOutput;
	if ( ( nSourceBytes > UINTPTR_MAX - nSourcePtr ) || ( nCapacity > UINTPTR_MAX - nOutputPtr ) ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	if ( nSourcePtr < nOutputPtr + nCapacity && nOutputPtr < nSourcePtr + nSourceBytes ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	if ( ( nEdits != 0 && SIZE_MAX / nEdits < sizeof( sirEditorTextEdit_t ) ) ||
	     ( index.nCount != 0 && SIZE_MAX / index.nCount < sizeof( sirEditorTextProperty_t ) ) ||
	     sirEditorWriterOutputOverlaps( pEdits, ( size_t )nEdits * sizeof( sirEditorTextEdit_t ), nOutputPtr, nCapacity ) ||
	     sirEditorWriterOutputOverlaps( index.pProperties, ( size_t )index.nCount * sizeof( sirEditorTextProperty_t ), nOutputPtr,
	                                    nCapacity ) ) {
		return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
	}
	// All replacement spans must stay outside the output destination. A prior
	// copy could otherwise overwrite a later edit's input before it is read.
	for ( uint32_t iEdit = 0; iEdit < nEdits; ++iEdit ) {
		const sirEditorTextEdit_t& edit = pEdits[iEdit];
		if ( sirEditorWriterOutputOverlaps( edit.pszReplacement, edit.nReplacementBytes, nOutputPtr, nCapacity ) ) {
			return SIR_EDITOR_WRITE_INVALID_ARGUMENT;
		}
	}
	size_t nSourceOffset = 0;
	size_t nDestinationOffset = 0;
	uint32_t nWritten = 0;
	while ( nWritten < nEdits ) {
		uint32_t iNext = UINT32_MAX;
		size_t nNextOffset = SIZE_MAX;
		for ( uint32_t iEdit = 0; iEdit < nEdits; ++iEdit ) {
			const size_t nOffset = index.pProperties[pEdits[iEdit].iProperty].nValueOffset;
			if ( nOffset >= nSourceOffset && nOffset < nNextOffset ) {
				nNextOffset = nOffset;
				iNext = iEdit;
			}
		}
		if ( iNext == UINT32_MAX ) {
			return SIR_EDITOR_WRITE_INVALID_SOURCE;
		}
		const sirEditorTextEdit_t& edit = pEdits[iNext];
		const sirEditorTextProperty_t& property = index.pProperties[edit.iProperty];
		const size_t nCopyBytes = nNextOffset - nSourceOffset;
		memcpy( pszOutput + nDestinationOffset, pszSource + nSourceOffset, nCopyBytes );
		nDestinationOffset += nCopyBytes;
		memcpy( pszOutput + nDestinationOffset, edit.pszReplacement, edit.nReplacementBytes );
		nDestinationOffset += edit.nReplacementBytes;
		nSourceOffset = property.nValueOffset + property.nValueLength;
		++nWritten;
	}
	memcpy( pszOutput + nDestinationOffset, pszSource + nSourceOffset, nSourceBytes - nSourceOffset );
	pszOutput[nOutputBytes] = '\0';
	return SIR_EDITOR_WRITE_OK;
}
