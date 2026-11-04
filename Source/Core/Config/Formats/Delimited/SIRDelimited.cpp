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

#include <Core/Config/Formats/Delimited/SIRDelimited.h>
#include <limits.h>
#include <string.h>

SIRDelimitedReader::SIRDelimitedReader( void ) noexcept
    : m_pAllocator( nullptr ), m_Limits{}, m_Options{}, m_Input{}, m_Source{}, m_pRowBytes( nullptr ), m_pFields( nullptr ),
      m_nRowCapacity( 0 ), m_nFieldCapacity( 0 ), m_nRowBytes( 0 ), m_nFields( 0 ), m_nFieldStart( 0 ), m_nRows( 0 ), m_nOffset( 0 ),
      m_nFetched( 0 ), m_nLine( 1 ), m_nColumn( 1 ), m_nChunkStart( 0 ), m_nChunkLength( 0 ), m_eState( SIR_DELIMITED_START ),
      m_eFailure( SIR_CONFIG_OK ), m_bOpened( false ), m_bStreaming( false ), m_bEnd( false ), m_bRowStarted( false ), m_bSkipLF( false ),
      m_bPrevCR( false ), m_szChunk{}
{
}

SIRDelimitedReader::~SIRDelimitedReader( void ) noexcept
{
	close();
}

bool SIRDelimitedReader::validOptions( const sirDelimitedOptions_t &options ) const noexcept
{
	return options.nDelimiter != '\r' && options.nDelimiter != '\n' && options.nDelimiter != options.nQuote && options.nQuote != '\r' &&
	       options.nQuote != '\n';
}

sirConfigResult_t SIRDelimitedReader::openCommon( const sirConfigParseContext_t &context, const sirDelimitedOptions_t &options ) noexcept
{
	if ( m_bOpened || !validOptions( options ) || context.pScratchAllocator == nullptr ||
	     context.pScratchAllocator->pfnAllocate == nullptr || context.pScratchAllocator->pfnDeallocate == nullptr ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}

	const sirConfigLimits_t &limits = context.limits;
	if ( limits.nMaxInputBytes == 0 || limits.nMaxScratchLiveBytes == 0 || limits.nMaxScalarBytes == 0 || limits.nMaxRowBytes == 0 ||
	     limits.nMaxColumns == 0 || limits.nMaxRows == 0 ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}

	m_pAllocator = context.pScratchAllocator;
	m_Limits = limits;
	m_Options = options;
	m_bOpened = true;
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedReader::openBuffer( const sirConfigInput_t &input, const sirConfigParseContext_t &context,
                                                  const sirDelimitedOptions_t &options ) noexcept
{
	if ( input.nBytes != 0 && input.pBytes == nullptr ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	if ( input.nBytes > context.limits.nMaxInputBytes ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}

	sirConfigResult_t eResult = openCommon( context, options );
	if ( eResult != SIR_CONFIG_OK ) {
		return eResult;
	}

	m_Input = input;
	m_nFetched = input.nBytes;
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedReader::openStream( const sirConfigSource_t &source, const sirConfigParseContext_t &context,
                                                  const sirDelimitedOptions_t &options ) noexcept
{
	if ( source.pfnRead == nullptr ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}

	sirConfigResult_t eResult = openCommon( context, options );
	if ( eResult != SIR_CONFIG_OK ) {
		return eResult;
	}

	m_Source = source;
	m_bStreaming = true;
	return SIR_CONFIG_OK;
}

void SIRDelimitedReader::close( void ) noexcept
{
	if ( m_pAllocator != nullptr ) {
		if ( m_pRowBytes != nullptr ) {
			m_pAllocator->pfnDeallocate( m_pAllocator->pContext, m_pRowBytes, m_nRowCapacity, alignof( uint8_t ) );
		}
		if ( m_pFields != nullptr ) {
			m_pAllocator->pfnDeallocate( m_pAllocator->pContext, m_pFields, ( size_t )m_nFieldCapacity * sizeof( sirDelimitedField_t ),
			                             alignof( sirDelimitedField_t ) );
		}
	}

	m_pAllocator = nullptr;
	m_pRowBytes = nullptr;
	m_pFields = nullptr;
	m_nRowCapacity = 0;
	m_nFieldCapacity = 0;
	m_bOpened = false;
	m_bStreaming = false;
	m_bEnd = false;
	m_bRowStarted = false;
	m_bSkipLF = false;
	m_bPrevCR = false;
	m_Input = {};
	m_Source = {};
	m_nRowBytes = 0;
	m_nFields = 0;
	m_nFieldStart = 0;
	m_nRows = 0;
	m_nOffset = 0;
	m_nFetched = 0;
	m_nLine = 1;
	m_nColumn = 1;
	m_nChunkStart = 0;
	m_nChunkLength = 0;
	m_eState = SIR_DELIMITED_START;
	m_eFailure = SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedReader::readByte( uint8_t *pByte, bool *pEnd ) noexcept
{
	*pEnd = false;
	if ( !m_bStreaming ) {
		if ( m_nOffset == m_Input.nBytes ) {
			*pEnd = true;
			return SIR_CONFIG_OK;
		}
		*pByte = m_Input.pBytes[m_nOffset++];
	} else {
		if ( m_nChunkStart == m_nChunkLength ) {
			if ( m_bEnd ) {
				*pEnd = true;
				return SIR_CONFIG_OK;
			}
			uint64_t nRead = 0;
			sirConfigResult_t eResult = m_Source.pfnRead( m_Source.pContext, m_szChunk, sizeof( m_szChunk ), &nRead );
			if ( eResult == SIR_CONFIG_END ) {
				if ( nRead != 0 ) {
					return SIR_CONFIG_IO_ERROR;
				}
				m_bEnd = true;
				*pEnd = true;
				return SIR_CONFIG_OK;
			}
			if ( eResult != SIR_CONFIG_OK ) {
				return eResult;
			}
			if ( nRead == 0 || nRead > sizeof( m_szChunk ) ) {
				return SIR_CONFIG_IO_ERROR;
			}
			if ( nRead > m_Limits.nMaxInputBytes - m_nFetched ) {
				return SIR_CONFIG_LIMIT_EXCEEDED;
			}
			m_nFetched += nRead;
			m_nChunkLength = ( uint32_t )nRead;
			m_nChunkStart = 0;
		}
		*pByte = m_szChunk[m_nChunkStart++];
		++m_nOffset;
	}

	if ( *pByte == '\r' ) {
		++m_nLine;
		m_nColumn = 1;
	} else if ( *pByte == '\n' ) {
		if ( !m_bPrevCR ) {
			++m_nLine;
		}
		m_nColumn = 1;
	} else {
		++m_nColumn;
	}
	m_bPrevCR = *pByte == '\r';
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedReader::appendByte( uint8_t nByte ) noexcept
{
	if ( m_nRowBytes >= m_Limits.nMaxRowBytes ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}
	if ( m_nRowBytes - m_nFieldStart >= m_Limits.nMaxScalarBytes ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}

	if ( m_nRowBytes == m_nRowCapacity ) {
		const uint32_t nLimit = m_Limits.nMaxRowBytes;
		const uint32_t nCapacity = m_nRowCapacity == 0 ? 64 : ( m_nRowCapacity > UINT32_MAX / 2 ? UINT32_MAX : m_nRowCapacity * 2 );
		const uint32_t nNext = nCapacity > nLimit ? nLimit : nCapacity;
		if ( nNext <= m_nRowCapacity ) {
			return SIR_CONFIG_LIMIT_EXCEEDED;
		}
		uint8_t *pNext = ( uint8_t * )m_pAllocator->pfnAllocate( m_pAllocator->pContext, nNext, alignof( uint8_t ) );
		if ( pNext == nullptr ) {
			return SIR_CONFIG_OUT_OF_MEMORY;
		}
		if ( m_nRowBytes != 0 ) {
			memcpy( pNext, m_pRowBytes, m_nRowBytes );
		}
		if ( m_pRowBytes != nullptr ) {
			m_pAllocator->pfnDeallocate( m_pAllocator->pContext, m_pRowBytes, m_nRowCapacity, alignof( uint8_t ) );
		}
		m_pRowBytes = pNext;
		m_nRowCapacity = nNext;
	}

	m_pRowBytes[m_nRowBytes++] = nByte;
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedReader::completeField( void ) noexcept
{
	if ( m_nFields >= m_Limits.nMaxColumns ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}

	if ( m_nFields == m_nFieldCapacity ) {
		const uint32_t nCapacity = m_nFieldCapacity == 0 ? 8 : ( m_nFieldCapacity > UINT32_MAX / 2 ? UINT32_MAX : m_nFieldCapacity * 2 );
		const uint32_t nNext = nCapacity > m_Limits.nMaxColumns ? m_Limits.nMaxColumns : nCapacity;
		if ( nNext <= m_nFieldCapacity || ( size_t )nNext > SIZE_MAX / sizeof( sirDelimitedField_t ) ) {
			return SIR_CONFIG_LIMIT_EXCEEDED;
		}
		const size_t nBytes = ( size_t )nNext * sizeof( sirDelimitedField_t );
		sirDelimitedField_t *pNext =
		    ( sirDelimitedField_t * )m_pAllocator->pfnAllocate( m_pAllocator->pContext, nBytes, alignof( sirDelimitedField_t ) );
		if ( pNext == nullptr ) {
			return SIR_CONFIG_OUT_OF_MEMORY;
		}
		if ( m_nFields != 0 ) {
			memcpy( pNext, m_pFields, ( size_t )m_nFields * sizeof( sirDelimitedField_t ) );
		}
		if ( m_pFields != nullptr ) {
			m_pAllocator->pfnDeallocate( m_pAllocator->pContext, m_pFields, ( size_t )m_nFieldCapacity * sizeof( sirDelimitedField_t ),
			                             alignof( sirDelimitedField_t ) );
		}
		m_pFields = pNext;
		m_nFieldCapacity = nNext;
	}

	sirDelimitedField_t &field = m_pFields[m_nFields++];
	field.nOffset = m_nFieldStart;
	field.value.nBytes = m_nRowBytes - m_nFieldStart;
	field.value.pBytes = nullptr;
	m_nFieldStart = m_nRowBytes;
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedReader::completeRow( sirDelimitedRowView_t *pRow ) noexcept
{
	if ( m_nRows >= m_Limits.nMaxRows ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}
	for ( uint32_t iField = 0; iField < m_nFields; ++iField ) {
		sirDelimitedField_t &field = m_pFields[iField];
		field.value.pBytes = m_pRowBytes == nullptr ? "" : ( const char * )m_pRowBytes + field.nOffset;
	}
	pRow->pFields = m_pFields;
	pRow->nFields = m_nFields;
	++m_nRows;
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedReader::fail( sirConfigResult_t eResult, sirConfigDiagnostic_t *pDiagnostic ) noexcept
{
	if ( eResult != SIR_CONFIG_OK && eResult != SIR_CONFIG_END ) {
		m_eFailure = eResult;
	}
	if ( pDiagnostic != nullptr ) {
		pDiagnostic->eResult = eResult;
		pDiagnostic->nByteOffset = m_nOffset;
		pDiagnostic->nLine = m_nLine;
		pDiagnostic->nColumn = m_nColumn;
		pDiagnostic->nSourceId = 0;
		pDiagnostic->nFormatErrorCode = ( uint32_t )m_eState;
	}
	return eResult;
}

sirConfigResult_t SIRDelimitedReader::nextRow( sirDelimitedRowView_t *pRow, sirConfigDiagnostic_t *pDiagnostic ) noexcept
{
	if ( !m_bOpened || pRow == nullptr ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	pRow->pFields = nullptr;
	pRow->nFields = 0;
	if ( m_eFailure != SIR_CONFIG_OK ) {
		return fail( m_eFailure, pDiagnostic );
	}
	if ( m_bEnd && m_nChunkStart == m_nChunkLength ) {
		return fail( SIR_CONFIG_END, pDiagnostic );
	}

	m_nRowBytes = 0;
	m_nFields = 0;
	m_nFieldStart = 0;
	m_eState = SIR_DELIMITED_START;
	m_bRowStarted = false;

	for ( ;; ) {
		uint8_t nByte = 0;
		bool bEnd = false;
		sirConfigResult_t eResult = readByte( &nByte, &bEnd );
		if ( eResult != SIR_CONFIG_OK ) {
			return fail( eResult, pDiagnostic );
		}
		if ( bEnd ) {
			if ( m_eState == SIR_DELIMITED_QUOTED ) {
				return fail( SIR_CONFIG_SYNTAX_ERROR, pDiagnostic );
			}
			if ( !m_bRowStarted ) {
				return fail( SIR_CONFIG_END, pDiagnostic );
			}
			eResult = completeField();
			if ( eResult == SIR_CONFIG_OK ) {
				eResult = completeRow( pRow );
			}
			return fail( eResult, pDiagnostic );
		}

		if ( m_bSkipLF ) {
			m_bSkipLF = false;
			if ( nByte == '\n' ) {
				continue;
			}
		}
		m_bRowStarted = true;
		if ( m_eState == SIR_DELIMITED_QUOTED ) {
			if ( nByte == m_Options.nQuote ) {
				m_eState = SIR_DELIMITED_AFTER_QUOTE;
			} else {
				eResult = appendByte( nByte );
			}
		} else if ( m_eState == SIR_DELIMITED_AFTER_QUOTE ) {
			if ( nByte == m_Options.nQuote ) {
				eResult = appendByte( nByte );
				m_eState = SIR_DELIMITED_QUOTED;
			} else if ( nByte != m_Options.nDelimiter && nByte != '\n' && nByte != '\r' ) {
				eResult = SIR_CONFIG_SYNTAX_ERROR;
			}
		} else if ( nByte == m_Options.nQuote && m_eState == SIR_DELIMITED_START ) {
			m_eState = SIR_DELIMITED_QUOTED;
		} else if ( nByte == m_Options.nQuote && m_Options.bStrictQuotes ) {
			eResult = SIR_CONFIG_SYNTAX_ERROR;
		} else if ( nByte != m_Options.nDelimiter && nByte != '\n' && nByte != '\r' ) {
			eResult = appendByte( nByte );
			m_eState = SIR_DELIMITED_UNQUOTED;
		}

		if ( eResult != SIR_CONFIG_OK ) {
			return fail( eResult, pDiagnostic );
		}
		if ( m_eState != SIR_DELIMITED_QUOTED ) {
			if ( nByte == m_Options.nDelimiter || nByte == '\n' || nByte == '\r' ) {
				eResult = completeField();
				if ( eResult != SIR_CONFIG_OK ) {
					return fail( eResult, pDiagnostic );
				}
				m_eState = SIR_DELIMITED_START;
				if ( nByte == '\n' || nByte == '\r' ) {
					m_bSkipLF = nByte == '\r';
					return fail( completeRow( pRow ), pDiagnostic );
				}
			}
		}
	}
}

SIRDelimitedWriter::SIRDelimitedWriter( void ) noexcept
    : m_Sink{}, m_Limits{}, m_Options{}, m_nOutputBytes( 0 ), m_nRows( 0 ), m_eFailure( SIR_CONFIG_OK ), m_bOpened( false )
{
}

sirConfigResult_t SIRDelimitedWriter::open( const sirConfigSink_t &sink, const sirConfigParseContext_t &context,
                                            const sirDelimitedOptions_t &options ) noexcept
{
	if ( m_bOpened || sink.pfnWrite == nullptr || context.limits.nMaxOutputBytes == 0 || context.limits.nMaxRows == 0 ||
	     context.limits.nMaxColumns == 0 || context.limits.nMaxScalarBytes == 0 || context.limits.nMaxRowBytes == 0 ||
	     options.nDelimiter == options.nQuote || options.nDelimiter == '\n' || options.nDelimiter == '\r' || options.nQuote == '\n' ||
	     options.nQuote == '\r' ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	m_Sink = sink;
	m_Limits = context.limits;
	m_Options = options;
	m_bOpened = true;
	return SIR_CONFIG_OK;
}

bool SIRDelimitedWriter::needsQuote( const sirStringView_t &field ) const noexcept
{
	for ( uint32_t iByte = 0; iByte < field.nBytes; ++iByte ) {
		const uint8_t nByte = ( uint8_t )field.pBytes[iByte];
		if ( nByte == m_Options.nQuote || nByte == m_Options.nDelimiter || nByte == '\r' || nByte == '\n' ) {
			return true;
		}
	}
	return false;
}

sirConfigResult_t SIRDelimitedWriter::computeRowBytes( const sirStringView_t *pFields, uint32_t nFieldCount,
                                                       uint64_t *pBytes ) const noexcept
{
	if ( nFieldCount == 0 || nFieldCount > m_Limits.nMaxColumns || pFields == nullptr ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	uint64_t nTotal = ( uint64_t )( nFieldCount - 1 ) + ( m_Options.bWriteCRLF ? 2u : 1u );
	for ( uint32_t iField = 0; iField < nFieldCount; ++iField ) {
		const sirStringView_t &field = pFields[iField];
		if ( ( field.pBytes == nullptr && field.nBytes != 0 ) || field.nBytes > m_Limits.nMaxScalarBytes ) {
			return SIR_CONFIG_INVALID_ARGUMENT;
		}
		const bool bQuoted = needsQuote( field );
		uint64_t nRequired = field.nBytes + ( bQuoted ? 2u : 0u );
		if ( bQuoted ) {
			for ( uint32_t iByte = 0; iByte < field.nBytes; ++iByte ) {
				if ( ( uint8_t )field.pBytes[iByte] == m_Options.nQuote ) {
					++nRequired;
				}
			}
		}
		if ( nTotal > UINT64_MAX - nRequired ) {
			return SIR_CONFIG_LIMIT_EXCEEDED;
		}
		nTotal += nRequired;
	}
	if ( nTotal > m_Limits.nMaxRowBytes ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}
	*pBytes = nTotal;
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedWriter::writeSpan( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	if ( nBytes == 0 ) {
		return SIR_CONFIG_OK;
	}
	sirConfigResult_t eResult = m_Sink.pfnWrite( m_Sink.pContext, pBytes, nBytes );
	if ( eResult != SIR_CONFIG_OK ) {
		m_eFailure = eResult;
		return eResult;
	}
	m_nOutputBytes += nBytes;
	return SIR_CONFIG_OK;
}

sirConfigResult_t SIRDelimitedWriter::writeByte( uint8_t nByte ) noexcept
{
	return writeSpan( &nByte, 1 );
}

sirConfigResult_t SIRDelimitedWriter::writeRow( const sirStringView_t *pFields, uint32_t nFieldCount ) noexcept
{
	if ( !m_bOpened ) {
		return SIR_CONFIG_INVALID_ARGUMENT;
	}
	if ( m_eFailure != SIR_CONFIG_OK ) {
		return m_eFailure;
	}
	if ( m_nRows >= m_Limits.nMaxRows ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}

	uint64_t nRequired = 0;
	sirConfigResult_t eResult = computeRowBytes( pFields, nFieldCount, &nRequired );
	if ( eResult != SIR_CONFIG_OK ) {
		return eResult;
	}
	if ( nRequired > m_Limits.nMaxOutputBytes - m_nOutputBytes ) {
		return SIR_CONFIG_LIMIT_EXCEEDED;
	}

	for ( uint32_t iField = 0; iField < nFieldCount; ++iField ) {
		if ( iField != 0 ) {
			eResult = writeByte( m_Options.nDelimiter );
			if ( eResult != SIR_CONFIG_OK ) {
				return eResult;
			}
		}
		const sirStringView_t &field = pFields[iField];
		const bool bQuoted = needsQuote( field );
		if ( bQuoted ) {
			eResult = writeByte( m_Options.nQuote );
			if ( eResult != SIR_CONFIG_OK ) {
				return eResult;
			}
		}
		if ( !bQuoted ) {
			eResult = writeSpan( ( const uint8_t * )field.pBytes, field.nBytes );
			if ( eResult != SIR_CONFIG_OK ) {
				return eResult;
			}
		} else {
			uint32_t iStart = 0;
			for ( uint32_t iByte = 0; iByte < field.nBytes; ++iByte ) {
				if ( ( uint8_t )field.pBytes[iByte] == m_Options.nQuote ) {
					eResult = writeSpan( ( const uint8_t * )field.pBytes + iStart, iByte - iStart );
					if ( eResult != SIR_CONFIG_OK ) {
						return eResult;
					}
					eResult = writeByte( m_Options.nQuote );
					if ( eResult != SIR_CONFIG_OK ) {
						return eResult;
					}
					eResult = writeByte( m_Options.nQuote );
					if ( eResult != SIR_CONFIG_OK ) {
						return eResult;
					}
					iStart = iByte + 1;
				}
			}
			eResult = writeSpan( ( const uint8_t * )field.pBytes + iStart, field.nBytes - iStart );
			if ( eResult != SIR_CONFIG_OK ) {
				return eResult;
			}
		}
		if ( bQuoted ) {
			eResult = writeByte( m_Options.nQuote );
			if ( eResult != SIR_CONFIG_OK ) {
				return eResult;
			}
		}
	}

	const uint8_t szNewline[2] = { '\r', '\n' };
	const uint8_t nLF = '\n';
	eResult = m_Options.bWriteCRLF ? writeSpan( szNewline, 2 ) : writeSpan( &nLF, 1 );
	if ( eResult == SIR_CONFIG_OK ) {
		++m_nRows;
	}
	return eResult;
}

sirConfigResult_t SIRDelimitedWriter::finish( void ) noexcept
{
	return !m_bOpened ? SIR_CONFIG_INVALID_ARGUMENT : m_eFailure;
}

void SIRDelimitedWriter::close( void ) noexcept
{
	m_bOpened = false;
	m_Sink = {};
	m_eFailure = SIR_CONFIG_OK;
	m_nRows = 0;
	m_nOutputBytes = 0;
}

sirDelimitedOptions_t SIRCsv::defaultOptions( void ) noexcept
{
	return { ',', '"', true, true };
}

sirDelimitedOptions_t SIRTsv::defaultOptions( void ) noexcept
{
	return { '\t', '"', true, true };
}
