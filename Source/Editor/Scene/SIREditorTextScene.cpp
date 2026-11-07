#include <Editor/Scene/SIREditorTextScene.h>

#include <limits.h>

typedef struct sirEditorTextField_s
{
	const char *pData;
	uint32_t nLength;
} sirEditorTextField_t;

static bool sirEditorTextRangesOverlap( const void *pLeft, size_t nLeftBytes, const void *pRight, size_t nRightBytes ) noexcept
{
	const uintptr_t nLeft = ( uintptr_t )pLeft;
	const uintptr_t nRight = ( uintptr_t )pRight;
	if ( nLeftBytes > UINTPTR_MAX - nLeft || nRightBytes > UINTPTR_MAX - nRight ) {
		return true;
	}
	return nLeft < nRight + nRightBytes && nRight < nLeft + nLeftBytes;
}

static bool sirEditorIsSpace( char c ) noexcept
{
	return c == ' ' || c == '\t' || c == '\r';
}

static bool sirEditorFieldEquals( sirEditorTextField_t field, const char *pszValue ) noexcept
{
	size_t nLength = strlen( pszValue );
	return nLength == field.nLength && memcmp( field.pData, pszValue, nLength ) == 0;
}

static bool sirEditorQuotedField( sirEditorTextField_t field ) noexcept
{
	return field.nLength >= 2 && field.pData[0] == '"' && field.pData[field.nLength - 1] == '"';
}

static uint64_t sirEditorHash64( const char *pData, size_t nBytes ) noexcept
{
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( size_t iByte = 0; iByte < nBytes; ++iByte ) {
		nHash ^= ( uint8_t )pData[iByte];
		nHash *= UINT64_C( 1099511628211 );
	}
	return nHash;
}

static uint32_t sirEditorHashType( sirEditorTextField_t field ) noexcept
{
	const uint64_t nHash = sirEditorHash64( field.pData + 1, field.nLength - 2 );
	uint32_t nResult = ( uint32_t )( nHash ^ ( nHash >> 32 ) );
	return nResult == 0 ? 1 : nResult;
}

static bool sirEditorParseUnsigned( sirEditorTextField_t field, uint32_t *pValue ) noexcept
{
	if ( field.nLength == 0 ) {
		return false;
	}
	uint32_t nValue = 0;
	for ( uint32_t iChar = 0; iChar < field.nLength; ++iChar ) {
		char c = field.pData[iChar];
		if ( c < '0' || c > '9' ) {
			return false;
		}
		uint32_t nDigit = ( uint32_t )( c - '0' );
		if ( nValue > ( UINT32_MAX - nDigit ) / 10 ) {
			return false;
		}
		nValue = nValue * 10 + nDigit;
	}
	*pValue = nValue;
	return nValue != 0;
}

static sirEditorTextSceneResult_t sirEditorDecodeQuoted( sirEditorTextField_t field, char *pDestination, uint32_t nCapacity,
                                                         uint32_t *pLength ) noexcept
{
	if ( !sirEditorQuotedField( field ) || pLength == nullptr ) {
		return SIR_EDITOR_TEXT_INVALID_NODE;
	}
	if ( nCapacity == 0 ) {
		return SIR_EDITOR_TEXT_OUT_OF_CAPACITY;
	}
	uint32_t nUsed = 0;
	for ( uint32_t iChar = 1; iChar < field.nLength - 1; ++iChar ) {
		char c = field.pData[iChar];
		if ( c == '\\' ) {
			if ( ++iChar >= field.nLength - 1 ) {
				return SIR_EDITOR_TEXT_INVALID_NODE;
			}
			char cEscape = field.pData[iChar];
			if ( cEscape == 'n' ) {
				c = '\n';
			} else if ( cEscape == 't' ) {
				c = '\t';
			} else if ( cEscape == '"' || cEscape == '\\' ) {
				c = cEscape;
			} else {
				return SIR_EDITOR_TEXT_INVALID_NODE;
			}
		}
		if ( c == '\0' ) {
			return SIR_EDITOR_TEXT_INVALID_NODE;
		}
		if ( nUsed >= nCapacity - 1 ) {
			return SIR_EDITOR_TEXT_OUT_OF_CAPACITY;
		}
		pDestination[nUsed++] = c;
	}
	pDestination[nUsed] = '\0';
	*pLength = nUsed;
	return SIR_EDITOR_TEXT_OK;
}

static bool sirEditorScanField( const char **ppCursor, const char *pEnd, sirEditorTextField_t *pKey, sirEditorTextField_t *pValue ) noexcept
{
	const char *pCursor = *ppCursor;
	while ( pCursor < pEnd && sirEditorIsSpace( *pCursor ) ) {
		++pCursor;
	}
	if ( pCursor == pEnd ) {
		*ppCursor = pCursor;
		pKey->nLength = 0;
		return true;
	}
	const char *pStart = pCursor;
	while ( pCursor < pEnd && ( ( *pCursor >= 'a' && *pCursor <= 'z' ) || ( *pCursor >= 'A' && *pCursor <= 'Z' ) ||
	                            ( *pCursor >= '0' && *pCursor <= '9' ) || *pCursor == '_' ) ) {
		++pCursor;
	}
	if ( pStart == pCursor ) {
		return false;
	}
	pKey->pData = pStart;
	pKey->nLength = ( uint32_t )( pCursor - pStart );
	while ( pCursor < pEnd && sirEditorIsSpace( *pCursor ) ) {
		++pCursor;
	}
	if ( pCursor == pEnd || *pCursor != '=' ) {
		return false;
	}
	++pCursor;
	while ( pCursor < pEnd && sirEditorIsSpace( *pCursor ) ) {
		++pCursor;
	}
	const char *pValueStart = pCursor;
	bool bQuoted = false;
	bool bEscape = false;
	uint32_t nDepth = 0;
	while ( pCursor < pEnd ) {
		char c = *pCursor;
		if ( bEscape ) {
			bEscape = false;
		} else if ( bQuoted && c == '\\' ) {
			bEscape = true;
		} else if ( c == '"' ) {
			bQuoted = !bQuoted;
		} else if ( !bQuoted ) {
			if ( c == '[' || c == '(' || c == '{' ) {
				++nDepth;
			} else if ( c == ']' || c == ')' || c == '}' ) {
				if ( nDepth == 0 ) {
					return false;
				}
				--nDepth;
			} else if ( sirEditorIsSpace( c ) && nDepth == 0 ) {
				break;
			}
		}
		++pCursor;
	}
	if ( pCursor == pValueStart || bQuoted || nDepth != 0 || bEscape ) {
		return false;
	}
	pValue->pData = pValueStart;
	pValue->nLength = ( uint32_t )( pCursor - pValueStart );
	*ppCursor = pCursor;
	return true;
}

static bool sirEditorFindTag( const char *pStart, const char *pEnd, const char **ppTagEnd ) noexcept
{
	uint32_t nDepth = 0;
	bool bQuoted = false;
	bool bEscape = false;
	for ( const char *pCursor = pStart; pCursor < pEnd; ++pCursor ) {
		char c = *pCursor;
		if ( bEscape ) {
			bEscape = false;
		} else if ( bQuoted && c == '\\' ) {
			bEscape = true;
		} else if ( c == '"' ) {
			bQuoted = !bQuoted;
		} else if ( !bQuoted ) {
			if ( c == '[' ) {
				++nDepth;
			} else if ( c == ']' ) {
				if ( nDepth == 0 ) {
					return false;
				}
				if ( --nDepth == 0 ) {
					*ppTagEnd = pCursor;
					return true;
				}
			}
		}
	}
	return false;
}

static sirEditorTextSceneResult_t sirEditorConsumeNode( const char *pszSource, sirEditorTextSceneIndex_t *pIndex,
                                                        sirEditorTextField_t fields[4], uint32_t nLine ) noexcept
{
	if ( pIndex->nNodeCount >= pIndex->storage.nNodeCapacity || pIndex->nPathBytes >= pIndex->storage.nPathCapacity ) {
		return SIR_EDITOR_TEXT_OUT_OF_CAPACITY;
	}
	if ( !sirEditorQuotedField( fields[0] ) || !sirEditorQuotedField( fields[1] ) ||
	     memchr( fields[1].pData + 1, '\\', fields[1].nLength - 2 ) != nullptr ) {
		return SIR_EDITOR_TEXT_INVALID_NODE;
	}
	const uint32_t nPathStart = pIndex->nPathBytes;
	uint32_t nParentLength = 0;
	uint32_t nPrefixLength = 0;
	uint64_t nParentId = 0;
	bool bRoot = pIndex->nNodeCount == 0;
	if ( bRoot ) {
		if ( fields[2].pData != nullptr ) {
			return SIR_EDITOR_TEXT_INVALID_NODE;
		}
	} else {
		if ( fields[2].pData == nullptr ) {
			return SIR_EDITOR_TEXT_INVALID_NODE;
		}
		const sirEditorTextSceneResult_t eParent = sirEditorDecodeQuoted( fields[2], pIndex->storage.pszPaths + nPathStart,
		                                                                  pIndex->storage.nPathCapacity - nPathStart, &nParentLength );
		if ( eParent != SIR_EDITOR_TEXT_OK ) {
			return eParent;
		}
		if ( nParentLength == 1 && pIndex->storage.pszPaths[nPathStart] == '.' ) {
			// In Godot text scenes, parent="." identifies the scene root.
			// Native parent ID zero is reserved for the root itself.
			nParentId = pIndex->storage.pNodes[0].node.nId;
		} else {
			bool bFound = false;
			for ( uint32_t iParent = 0; iParent < pIndex->nNodeCount; ++iParent ) {
				const sirEditorTextSceneNode_t& candidate = pIndex->storage.pNodes[iParent];
				if ( candidate.nPathLength == nParentLength && memcmp( pIndex->storage.pszPaths + candidate.nPathOffset,
				                                                       pIndex->storage.pszPaths + nPathStart, nParentLength ) == 0 ) {
					nParentId = candidate.node.nId;
					nPrefixLength = nParentLength;
					bFound = true;
					break;
				}
			}
			if ( !bFound ) {
				return SIR_EDITOR_TEXT_MISSING_PARENT;
			}
		}
	}
	char *pszDestination = pIndex->storage.pszPaths + nPathStart;
	if ( nPrefixLength != 0 ) {
		pszDestination[nPrefixLength] = '/';
		++nPrefixLength;
	}
	const uint32_t nNameCapacity = pIndex->storage.nPathCapacity - nPathStart - nPrefixLength;
	uint32_t nNameLength = 0;
	const sirEditorTextSceneResult_t eName =
	    sirEditorDecodeQuoted( fields[0], pszDestination + nPrefixLength, nNameCapacity, &nNameLength );
	if ( eName != SIR_EDITOR_TEXT_OK ) {
		return eName;
	}
	if ( nNameLength == 0 || memchr( pszDestination + nPrefixLength, '/', nNameLength ) != nullptr ) {
		return SIR_EDITOR_TEXT_INVALID_NODE;
	}
	uint32_t nPathLength = nPrefixLength + nNameLength;
	// The root's canonical parent path is empty, matching Godot's parent="." convention.
	if ( bRoot ) {
		nPathLength = 0;
		pszDestination[0] = '\0';
	}
	for ( uint32_t iPrevious = 0; iPrevious < pIndex->nNodeCount; ++iPrevious ) {
		const sirEditorTextSceneNode_t& previous = pIndex->storage.pNodes[iPrevious];
		if ( previous.nPathLength == nPathLength &&
		     memcmp( pszDestination, pIndex->storage.pszPaths + previous.nPathOffset, nPathLength ) == 0 ) {
			return SIR_EDITOR_TEXT_DUPLICATE_NODE;
		}
	}
	uint32_t nUniqueId = 0;
	if ( fields[3].pData != nullptr && !sirEditorParseUnsigned( fields[3], &nUniqueId ) ) {
		return SIR_EDITOR_TEXT_INVALID_NODE;
	}
	uint64_t nId = nUniqueId == 0 ? ( sirEditorHash64( pszDestination, nPathLength ) & UINT64_C( 0x7fffffffffffffff ) )
	                              : ( UINT64_C( 0x8000000000000000 ) | nUniqueId );
	if ( nId == 0 ) {
		nId = 1;
	}
	for ( uint32_t iPrevious = 0; iPrevious < pIndex->nNodeCount; ++iPrevious ) {
		if ( pIndex->storage.pNodes[iPrevious].node.nId == nId ) {
			return SIR_EDITOR_TEXT_DUPLICATE_NODE;
		}
	}
	sirEditorTextSceneNode_t& entry = pIndex->storage.pNodes[pIndex->nNodeCount++];
	entry = {};
	entry.node.nId = nId;
	entry.node.nParentId = nParentId;
	entry.node.nTypeId = sirEditorHashType( fields[1] );
	entry.nSourceLine = nLine;
	entry.nNameOffset = ( uint32_t )( fields[0].pData + 1 - pszSource );
	entry.nNameLength = fields[0].nLength - 2;
	entry.nPathOffset = nPathStart;
	entry.nPathLength = nPathLength;
	entry.nGodotUniqueId = nUniqueId;
	pIndex->nPathBytes += nPathLength + 1;
	return SIR_EDITOR_TEXT_OK;
}

static sirEditorTextSceneResult_t sirEditorFailTextScene( sirEditorTextSceneIndex_t *pIndex, uint32_t nLine,
                                                          sirEditorTextSceneResult_t eResult ) noexcept
{
	pIndex->nErrorLine = nLine;
	pIndex->nNodeCount = 0;
	pIndex->nPathBytes = 0;
	return eResult;
}

sirEditorTextSceneResult_t sirEditorScanTextScene( const char *pszSource, size_t nSourceBytes, sirEditorTextSceneIndex_t *pIndex ) noexcept
{
	if ( pszSource == nullptr || pIndex == nullptr || pIndex->storage.pNodes == nullptr || pIndex->storage.pszPaths == nullptr ||
	     pIndex->storage.nNodeCapacity == 0 || pIndex->storage.nPathCapacity == 0 || nSourceBytes > UINT32_MAX ) {
		return SIR_EDITOR_TEXT_INVALID_ARGUMENT;
	}
	if ( ( ( uintptr_t )pIndex->storage.pNodes % alignof( sirEditorTextSceneNode_t ) ) != 0 ) {
		return SIR_EDITOR_TEXT_INVALID_ARGUMENT;
	}
#if SIZE_MAX == UINT32_MAX
	if ( pIndex->storage.nNodeCapacity > SIZE_MAX / sizeof( sirEditorTextSceneNode_t ) ) {
		return SIR_EDITOR_TEXT_INVALID_ARGUMENT;
	}
#endif
	const size_t nNodesBytes = ( size_t )pIndex->storage.nNodeCapacity * sizeof( sirEditorTextSceneNode_t );
	if ( sirEditorTextRangesOverlap( pszSource, nSourceBytes, pIndex->storage.pNodes, nNodesBytes ) ||
	     sirEditorTextRangesOverlap( pszSource, nSourceBytes, pIndex->storage.pszPaths, pIndex->storage.nPathCapacity ) ||
	     sirEditorTextRangesOverlap( pIndex->storage.pNodes, nNodesBytes, pIndex->storage.pszPaths, pIndex->storage.nPathCapacity ) ) {
		return SIR_EDITOR_TEXT_INVALID_ARGUMENT;
	}
	pIndex->nNodeCount = 0;
	pIndex->nPathBytes = 0;
	pIndex->nErrorLine = 0;
	if ( memchr( pszSource, '\0', nSourceBytes ) != nullptr ) {
		return SIR_EDITOR_TEXT_INVALID_ARGUMENT;
	}
	bool bHeader = false;
	bool bMultilineQuote = false;
	bool bEscape = false;
	uint32_t nLine = 1;
	const char *pCursor = pszSource;
	const char *pEnd = pszSource + nSourceBytes;
	while ( pCursor < pEnd ) {
		const char *pLineEnd = ( const char * )memchr( pCursor, '\n', ( size_t )( pEnd - pCursor ) );
		if ( pLineEnd == nullptr ) {
			pLineEnd = pEnd;
		}
		const char *pFirst = pCursor;
		while ( pFirst < pLineEnd && sirEditorIsSpace( *pFirst ) ) {
			++pFirst;
		}
		if ( pFirst < pLineEnd && *pFirst == '[' && !bMultilineQuote ) {
			const char *pTagEnd = nullptr;
			if ( !sirEditorFindTag( pFirst, pLineEnd, &pTagEnd ) ) {
				return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_INVALID_TAG );
			}
			const char *pTrailing = pTagEnd + 1;
			while ( pTrailing < pLineEnd && sirEditorIsSpace( *pTrailing ) ) {
				++pTrailing;
			}
			if ( pTrailing != pLineEnd && *pTrailing != ';' && *pTrailing != '#' ) {
				return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_INVALID_TAG );
			}
			const char *pNameStart = pFirst + 1;
			const char *pNameEnd = pNameStart;
			while ( pNameEnd < pTagEnd && !sirEditorIsSpace( *pNameEnd ) ) {
				++pNameEnd;
			}
			sirEditorTextField_t tag = { pNameStart, ( uint32_t )( pNameEnd - pNameStart ) };
			if ( !bHeader ) {
				if ( !sirEditorFieldEquals( tag, "gd_scene" ) ) {
					return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_INVALID_HEADER );
				}
				bHeader = true;
			} else if ( sirEditorFieldEquals( tag, "node" ) ) {
				sirEditorTextField_t fields[4] = {};
				const char *pField = pNameEnd;
				while ( pField < pTagEnd ) {
					sirEditorTextField_t key = {};
					sirEditorTextField_t value = {};
					if ( !sirEditorScanField( &pField, pTagEnd, &key, &value ) ) {
						return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_INVALID_TAG );
					}
					if ( key.nLength == 0 ) {
						break;
					}
					if ( sirEditorFieldEquals( key, "instance" ) || sirEditorFieldEquals( key, "instance_placeholder" ) ) {
						return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_UNSUPPORTED_INSTANCE );
					}
					const char *szKeys[4] = { "name", "type", "parent", "unique_id" };
					for ( uint32_t iKey = 0; iKey < 4; ++iKey ) {
						if ( sirEditorFieldEquals( key, szKeys[iKey] ) ) {
							if ( fields[iKey].pData != nullptr ) {
								return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_INVALID_TAG );
							}
							fields[iKey] = value;
						}
					}
				}
				if ( fields[1].pData == nullptr ) {
					return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_UNSUPPORTED_INSTANCE );
				}
				const sirEditorTextSceneResult_t eNode = sirEditorConsumeNode( pszSource, pIndex, fields, nLine );
				if ( eNode != SIR_EDITOR_TEXT_OK ) {
					return sirEditorFailTextScene( pIndex, nLine, eNode );
				}
			}
		} else {
			// Quoted property values can span physical lines; tags inside them are data.
			for ( const char *pChar = pCursor; pChar < pLineEnd; ++pChar ) {
				if ( bEscape ) {
					bEscape = false;
				} else if ( bMultilineQuote && *pChar == '\\' ) {
					bEscape = true;
				} else if ( !bMultilineQuote && ( *pChar == '#' || *pChar == ';' ) ) {
					break;
				} else if ( *pChar == '"' ) {
					bMultilineQuote = !bMultilineQuote;
				}
			}
		}
		pCursor = pLineEnd == pEnd ? pEnd : pLineEnd + 1;
		++nLine;
	}
	if ( bMultilineQuote || bEscape ) {
		return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_INVALID_TAG );
	}
	if ( !bHeader || pIndex->nNodeCount == 0 ) {
		return sirEditorFailTextScene( pIndex, nLine, SIR_EDITOR_TEXT_INVALID_HEADER );
	}
	return SIR_EDITOR_TEXT_OK;
}

sirEditorResult_t sirEditorReconcileTextScene( SIREditorSceneDocument& document, const sirEditorTextSceneIndex_t& index,
                                               sirEditorNode_t *pSnapshot, uint32_t nSnapshotCapacity ) noexcept
{
	if ( index.nNodeCount == 0 || index.nNodeCount > index.storage.nNodeCapacity || index.storage.pNodes == nullptr ||
	     pSnapshot == nullptr ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
#if SIZE_MAX == UINT32_MAX
	if ( nSnapshotCapacity > SIZE_MAX / sizeof( sirEditorNode_t ) ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
#endif
	if ( sirEditorTextRangesOverlap( pSnapshot, ( size_t )nSnapshotCapacity * sizeof( sirEditorNode_t ), index.storage.pNodes,
	                                 ( size_t )index.nNodeCount * sizeof( sirEditorTextSceneNode_t ) ) ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	uint32_t nOldCount = 0;
	uint64_t nRevision = 0;
	sirEditorResult_t eResult = document.copySnapshot( pSnapshot, nSnapshotCapacity, &nOldCount, &nRevision );
	if ( eResult != SIR_EDITOR_OK ) {
		return eResult;
	}
	uint32_t nChangeCount = 0;
	for ( uint32_t iOld = 0; iOld < nOldCount; ++iOld ) {
		bool bFound = false;
		for ( uint32_t iNew = 0; iNew < index.nNodeCount; ++iNew ) {
			if ( pSnapshot[iOld].nId == index.storage.pNodes[iNew].node.nId ) {
				bFound = true;
				break;
			}
		}
		if ( !bFound ) {
			++nChangeCount;
		}
	}
	for ( uint32_t iNew = 0; iNew < index.nNodeCount; ++iNew ) {
		const sirEditorNode_t& node = index.storage.pNodes[iNew].node;
		bool bMatch = false;
		for ( uint32_t iOld = 0; iOld < nOldCount; ++iOld ) {
			if ( pSnapshot[iOld].nId == node.nId ) {
				bMatch = pSnapshot[iOld].nParentId == node.nParentId && pSnapshot[iOld].nTypeId == node.nTypeId;
				break;
			}
		}
		if ( !bMatch ) {
			++nChangeCount;
		}
	}
	if ( nChangeCount == 0 ) {
		return SIR_EDITOR_OK;
	}
	eResult = document.beginEdit( nRevision );
	if ( eResult != SIR_EDITOR_OK ) {
		return eResult;
	}
	for ( uint32_t iOld = 0; iOld < nOldCount; ++iOld ) {
		bool bFound = false;
		for ( uint32_t iNew = 0; iNew < index.nNodeCount; ++iNew ) {
			if ( pSnapshot[iOld].nId == index.storage.pNodes[iNew].node.nId ) {
				bFound = true;
				break;
			}
		}
		if ( !bFound ) {
			eResult = document.queueRemove( pSnapshot[iOld].nId );
			if ( eResult != SIR_EDITOR_OK ) {
				document.cancelEdit();
				return eResult;
			}
		}
	}
	for ( uint32_t iNew = 0; iNew < index.nNodeCount; ++iNew ) {
		const sirEditorNode_t& node = index.storage.pNodes[iNew].node;
		bool bMatch = false;
		for ( uint32_t iOld = 0; iOld < nOldCount; ++iOld ) {
			if ( pSnapshot[iOld].nId == node.nId ) {
				bMatch = pSnapshot[iOld].nParentId == node.nParentId && pSnapshot[iOld].nTypeId == node.nTypeId;
				break;
			}
		}
		if ( !bMatch ) {
			eResult = document.queueUpsert( node );
			if ( eResult != SIR_EDITOR_OK ) {
				document.cancelEdit();
				return eResult;
			}
		}
	}
	eResult = document.commitEdit();
	if ( eResult != SIR_EDITOR_OK ) {
		document.cancelEdit();
	}
	return eResult;
}

static sirEditorTextResourceResult_t sirEditorFailResourceIndex( sirEditorTextResourceIndex_t *pIndex, uint32_t nLine,
                                                                 sirEditorTextResourceResult_t eResult ) noexcept
{
	pIndex->nCount = 0;
	pIndex->nErrorLine = nLine;
	return eResult;
}

static bool sirEditorValidResourceId( sirEditorTextField_t field ) noexcept
{
	if ( !sirEditorQuotedField( field ) || field.nLength <= 2 ) {
		return false;
	}
	for ( uint32_t iChar = 1; iChar + 1 < field.nLength; ++iChar ) {
		const char c = field.pData[iChar];
		if ( ( c < 'a' || c > 'z' ) && ( c < 'A' || c > 'Z' ) && ( c < '0' || c > '9' ) && c != '_' ) {
			return false;
		}
	}
	return true;
}

static bool sirEditorResourceTextEquals( const char *pszSource, const sirEditorTextResource_t& resource, const char *pId,
                                         uint32_t nIdLength ) noexcept
{
	return resource.nIdLength == nIdLength && memcmp( pszSource + resource.nIdOffset, pId, nIdLength ) == 0;
}

sirEditorTextResourceResult_t sirEditorFindTextResource( const char *pszSource, const sirEditorTextResourceIndex_t& index,
                                                         sirEditorTextResourceKind_t eKind, const char *pszId, uint32_t nIdLength,
                                                         uint32_t *pOutIndex ) noexcept
{
	if ( pszSource == nullptr || pszId == nullptr || pOutIndex == nullptr || nIdLength == 0 || index.nCount > index.nCapacity ||
	     ( index.nCount != 0 && index.pResources == nullptr ) ||
	     ( eKind != SIR_EDITOR_RESOURCE_EXTERNAL && eKind != SIR_EDITOR_RESOURCE_EMBEDDED ) ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	for ( uint32_t iResource = 0; iResource < index.nCount; ++iResource ) {
		const sirEditorTextResource_t& resource = index.pResources[iResource];
		if ( resource.eKind == eKind && sirEditorResourceTextEquals( pszSource, resource, pszId, nIdLength ) ) {
			*pOutIndex = iResource;
			return SIR_EDITOR_RESOURCE_OK;
		}
	}
	return SIR_EDITOR_RESOURCE_MISSING_REFERENCE;
}

static sirEditorTextResourceResult_t sirEditorConsumeResourceTag( const char *pszSource, const char *pFields, const char *pTagEnd,
                                                                  uint32_t nLine, sirEditorTextResourceKind_t eKind,
                                                                  sirEditorTextResourceIndex_t *pIndex ) noexcept
{
	sirEditorTextField_t fields[4] = {};
	const char *pCursor = pFields;
	const char *szKeys[4] = { "id", "path", "type", "uid" };
	while ( pCursor < pTagEnd ) {
		sirEditorTextField_t key = {};
		sirEditorTextField_t value = {};
		if ( !sirEditorScanField( &pCursor, pTagEnd, &key, &value ) ) {
			return SIR_EDITOR_RESOURCE_INVALID_TAG;
		}
		if ( key.nLength == 0 ) {
			break;
		}
		for ( uint32_t iKey = 0; iKey < 4; ++iKey ) {
			if ( sirEditorFieldEquals( key, szKeys[iKey] ) ) {
				if ( fields[iKey].pData != nullptr ) {
					return SIR_EDITOR_RESOURCE_INVALID_TAG;
				}
				fields[iKey] = value;
				break;
			}
		}
	}
	if ( !sirEditorValidResourceId( fields[0] ) || !sirEditorQuotedField( fields[2] ) || fields[2].nLength <= 2 ||
	     ( eKind == SIR_EDITOR_RESOURCE_EXTERNAL && ( !sirEditorQuotedField( fields[1] ) || fields[1].nLength <= 2 ) ) ||
	     ( fields[3].pData != nullptr && ( !sirEditorQuotedField( fields[3] ) || fields[3].nLength <= 2 ) ) ) {
		return SIR_EDITOR_RESOURCE_INVALID_DECLARATION;
	}
	if ( pIndex->nCount >= pIndex->nCapacity ) {
		return SIR_EDITOR_RESOURCE_OUT_OF_CAPACITY;
	}

	const char *pId = fields[0].pData + 1;
	const uint32_t nIdLength = fields[0].nLength - 2;
	uint32_t iDuplicate = 0;
	if ( sirEditorFindTextResource( pszSource, *pIndex, eKind, pId, nIdLength, &iDuplicate ) == SIR_EDITOR_RESOURCE_OK ) {
		return SIR_EDITOR_RESOURCE_DUPLICATE_ID;
	}

	sirEditorTextResource_t& resource = pIndex->pResources[pIndex->nCount++];
	resource = {};
	resource.eKind = eKind;
	resource.nSourceLine = nLine;
	resource.nIdOffset = ( uint32_t )( pId - pszSource );
	resource.nIdLength = nIdLength;
	resource.nTypeOffset = ( uint32_t )( fields[2].pData + 1 - pszSource );
	resource.nTypeLength = fields[2].nLength - 2;
	if ( fields[1].pData != nullptr ) {
		resource.nPathOffset = ( uint32_t )( fields[1].pData + 1 - pszSource );
		resource.nPathLength = fields[1].nLength - 2;
	}
	if ( fields[3].pData != nullptr ) {
		resource.nUidOffset = ( uint32_t )( fields[3].pData + 1 - pszSource );
		resource.nUidLength = fields[3].nLength - 2;
	}
	resource.nStableKey = sirEditorHash64( pId, nIdLength ) ^
	                      ( eKind == SIR_EDITOR_RESOURCE_EXTERNAL ? UINT64_C( 0x4ac9e2d51f4c9761 ) : UINT64_C( 0x7d5a2190c3f2e8b4 ) );
	if ( resource.nStableKey == 0 ) {
		resource.nStableKey = 1;
	}
	return SIR_EDITOR_RESOURCE_OK;
}

static bool sirEditorResourceTokenStart( char c ) noexcept
{
	return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_';
}

static sirEditorTextResourceResult_t sirEditorValidateResourceCalls( const char *pszSource, size_t nSourceBytes,
                                                                     sirEditorTextResourceIndex_t *pIndex ) noexcept
{
	const char *pEnd = pszSource + nSourceBytes;
	bool bQuoted = false;
	bool bEscape = false;
	bool bComment = false;
	uint32_t nLine = 1;
	for ( const char *pCursor = pszSource; pCursor < pEnd; ++pCursor ) {
		const char c = *pCursor;
		if ( c == '\n' ) {
			++nLine;
			bComment = false;
		}
		if ( bComment ) {
			continue;
		}
		if ( bEscape ) {
			bEscape = false;
			continue;
		}
		if ( bQuoted ) {
			if ( c == '\\' ) {
				bEscape = true;
			} else if ( c == '"' ) {
				bQuoted = false;
			}
			continue;
		}
		if ( c == '"' ) {
			bQuoted = true;
			continue;
		}
		if ( c == '#' || c == ';' ) {
			bComment = true;
			continue;
		}
		if ( pCursor != pszSource && sirEditorResourceTokenStart( pCursor[-1] ) ) {
			continue;
		}
		sirEditorTextResourceKind_t eKind = SIR_EDITOR_RESOURCE_EXTERNAL;
		uint32_t nFunctionLength = 12;
		if ( ( size_t )( pEnd - pCursor ) >= 12 && memcmp( pCursor, "ExtResource(", 12 ) == 0 ) {
			eKind = SIR_EDITOR_RESOURCE_EXTERNAL;
		} else if ( ( size_t )( pEnd - pCursor ) >= 12 && memcmp( pCursor, "SubResource(", 12 ) == 0 ) {
			eKind = SIR_EDITOR_RESOURCE_EMBEDDED;
		} else {
			continue;
		}
		const char *pCall = pCursor + nFunctionLength;
		while ( pCall < pEnd && sirEditorIsSpace( *pCall ) ) {
			++pCall;
		}
		if ( pCall == pEnd || *pCall++ != '"' ) {
			return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_REFERENCE );
		}
		const char *pId = pCall;
		while ( pCall < pEnd && *pCall != '"' && *pCall != '\n' ) {
			if ( !sirEditorResourceTokenStart( *pCall ) ) {
				return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_REFERENCE );
			}
			++pCall;
		}
		if ( pCall == pId || pCall == pEnd || *pCall++ != '"' ) {
			return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_REFERENCE );
		}
		const uint32_t nIdLength = ( uint32_t )( pCall - pId - 1 );
		while ( pCall < pEnd && sirEditorIsSpace( *pCall ) ) {
			++pCall;
		}
		if ( pCall == pEnd || *pCall != ')' ) {
			return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_REFERENCE );
		}
		uint32_t iResource = 0;
		if ( sirEditorFindTextResource( pszSource, *pIndex, eKind, pId, nIdLength, &iResource ) != SIR_EDITOR_RESOURCE_OK ) {
			return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_MISSING_REFERENCE );
		}
		pCursor = pCall;
	}
	return SIR_EDITOR_RESOURCE_OK;
}

sirEditorTextResourceResult_t sirEditorScanTextResources( const char *pszSource, size_t nSourceBytes,
                                                          sirEditorTextResourceIndex_t *pIndex ) noexcept
{
	if ( pIndex == nullptr ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	pIndex->nCount = 0;
	pIndex->nErrorLine = 0;
#if SIZE_MAX == UINT32_MAX
	if ( pIndex->nCapacity > SIZE_MAX / sizeof( sirEditorTextResource_t ) ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
#endif
	if ( pszSource == nullptr || nSourceBytes > UINT32_MAX || ( pIndex->nCapacity != 0 && pIndex->pResources == nullptr ) ||
	     ( ( uintptr_t )pIndex->pResources % alignof( sirEditorTextResource_t ) ) != 0 ||
	     ( pIndex->nCapacity != 0 && sirEditorTextRangesOverlap( pszSource, nSourceBytes, pIndex->pResources,
	                                                             ( size_t )pIndex->nCapacity * sizeof( sirEditorTextResource_t ) ) ) ||
	     ( pIndex->nCapacity != 0 && sirEditorTextRangesOverlap( pIndex, sizeof( *pIndex ), pIndex->pResources,
	                                                             ( size_t )pIndex->nCapacity * sizeof( sirEditorTextResource_t ) ) ) ||
	     memchr( pszSource, '\0', nSourceBytes ) != nullptr ) {
		return SIR_EDITOR_RESOURCE_INVALID_ARGUMENT;
	}
	bool bHeader = false;
	bool bMultilineQuote = false;
	bool bEscape = false;
	uint32_t nLine = 1;
	const char *pCursor = pszSource;
	const char *pEnd = pszSource + nSourceBytes;
	while ( pCursor < pEnd ) {
		const char *pLineEnd = ( const char * )memchr( pCursor, '\n', ( size_t )( pEnd - pCursor ) );
		if ( pLineEnd == nullptr ) {
			pLineEnd = pEnd;
		}
		const char *pFirst = pCursor;
		while ( pFirst < pLineEnd && sirEditorIsSpace( *pFirst ) ) {
			++pFirst;
		}
		if ( pFirst < pLineEnd && *pFirst == '[' && !bMultilineQuote ) {
			const char *pTagEnd = nullptr;
			if ( !sirEditorFindTag( pFirst, pLineEnd, &pTagEnd ) ) {
				return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_TAG );
			}
			const char *pTrailing = pTagEnd + 1;
			while ( pTrailing < pLineEnd && sirEditorIsSpace( *pTrailing ) ) {
				++pTrailing;
			}
			if ( pTrailing != pLineEnd && *pTrailing != '#' && *pTrailing != ';' ) {
				return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_TAG );
			}
			const char *pNameStart = pFirst + 1;
			const char *pNameEnd = pNameStart;
			while ( pNameEnd < pTagEnd && !sirEditorIsSpace( *pNameEnd ) ) {
				++pNameEnd;
			}
			const sirEditorTextField_t tag = { pNameStart, ( uint32_t )( pNameEnd - pNameStart ) };
			if ( !bHeader ) {
				if ( !sirEditorFieldEquals( tag, "gd_scene" ) ) {
					return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_HEADER );
				}
				bHeader = true;
			} else if ( sirEditorFieldEquals( tag, "ext_resource" ) || sirEditorFieldEquals( tag, "sub_resource" ) ) {
				const sirEditorTextResourceKind_t eKind =
				    sirEditorFieldEquals( tag, "ext_resource" ) ? SIR_EDITOR_RESOURCE_EXTERNAL : SIR_EDITOR_RESOURCE_EMBEDDED;
				const sirEditorTextResourceResult_t eResult =
				    sirEditorConsumeResourceTag( pszSource, pNameEnd, pTagEnd, nLine, eKind, pIndex );
				if ( eResult != SIR_EDITOR_RESOURCE_OK ) {
					return sirEditorFailResourceIndex( pIndex, nLine, eResult );
				}
			}
		} else if ( pFirst < pLineEnd && *pFirst != '#' && *pFirst != ';' ) {
			for ( const char *pChar = pCursor; pChar < pLineEnd; ++pChar ) {
				if ( bEscape ) {
					bEscape = false;
				} else if ( bMultilineQuote && *pChar == '\\' ) {
					bEscape = true;
				} else if ( !bMultilineQuote && ( *pChar == '#' || *pChar == ';' ) ) {
					break;
				} else if ( *pChar == '"' ) {
					bMultilineQuote = !bMultilineQuote;
				}
			}
		}
		pCursor = pLineEnd == pEnd ? pEnd : pLineEnd + 1;
		++nLine;
	}
	if ( !bHeader ) {
		return sirEditorFailResourceIndex( pIndex, nLine, SIR_EDITOR_RESOURCE_INVALID_HEADER );
	}
	return sirEditorValidateResourceCalls( pszSource, nSourceBytes, pIndex );
}
