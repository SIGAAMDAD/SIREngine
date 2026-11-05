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

#include <Core/Config/SIRConfigBudget.h>
#include <Core/Config/Formats/Delimited/SIRDelimited.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>

static uint32_t s_nFailures = 0;

static void check( bool bCondition, const char *szWhat ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "FAIL: %s\n", szWhat );
		++s_nFailures;
	}
}

static void *backingAllocate( void *, size_t nBytes, size_t nAlignment ) noexcept
{
	const size_t nAlign = nAlignment > alignof( max_align_t ) ? nAlignment : alignof( max_align_t );
	return ::operator new( nBytes, std::align_val_t( nAlign ), std::nothrow );
}

static void backingFree( void *, void *pMemory, size_t, size_t nAlignment ) noexcept
{
	const size_t nAlign = nAlignment > alignof( max_align_t ) ? nAlignment : alignof( max_align_t );
	::operator delete( pMemory, std::align_val_t( nAlign ) );
}

typedef struct sirTestSource_s
{
	const uint8_t *pBytes;
	uint64_t nSize;
	uint64_t nOffset;
	uint32_t nChunk;
} sirTestSource_t;

static sirConfigResult_t sourceRead( void *pContext, uint8_t *pBytes, uint64_t nCapacity, uint64_t *pBytesRead ) noexcept
{
	sirTestSource_t *pSource = ( sirTestSource_t * )pContext;
	const uint64_t nRemaining = pSource->nSize - pSource->nOffset;
	if ( nRemaining == 0 ) {
		*pBytesRead = 0;
		return SIR_CONFIG_END;
	}
	uint64_t nCopy = nRemaining < nCapacity ? nRemaining : nCapacity;
	if ( nCopy > pSource->nChunk ) {
		nCopy = pSource->nChunk;
	}
	memcpy( pBytes, pSource->pBytes + pSource->nOffset, ( size_t )nCopy );
	pSource->nOffset += nCopy;
	*pBytesRead = nCopy;
	return SIR_CONFIG_OK;
}

typedef struct sirTestSink_s
{
	char szData[1024];
	uint32_t nLength;
	uint32_t nFailAt;
} sirTestSink_t;

static sirConfigResult_t sinkWrite( void *pContext, const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	sirTestSink_t *pSink = ( sirTestSink_t * )pContext;
	if ( pSink->nLength + nBytes > sizeof( pSink->szData ) || pSink->nLength + nBytes > pSink->nFailAt ) {
		return SIR_CONFIG_IO_ERROR;
	}
	memcpy( pSink->szData + pSink->nLength, pBytes, ( size_t )nBytes );
	pSink->nLength += ( uint32_t )nBytes;
	return SIR_CONFIG_OK;
}

static bool equalField( const sirDelimitedRowView_t& row, uint32_t iField, const char *pBytes, uint32_t nBytes ) noexcept
{
	return iField < row.nFields && row.pFields[iField].value.nBytes == nBytes &&
	       ( nBytes == 0 || memcmp( row.pFields[iField].value.pBytes, pBytes, nBytes ) == 0 );
}

static void testBudgets( const sirContainerAllocator_t *pBacking ) noexcept
{
	SIRConfigBudget budget;
	check( budget.init( pBacking, 100 ), "budget init" );
	const sirContainerAllocator_t *pAllocator = budget.getAllocator();
	void *pFirst = pAllocator->pfnAllocate( pAllocator->pContext, 65, 16 );
	check( pFirst != nullptr, "budget first allocate" );
	check( budget.getLiveBytes() == 65 && budget.getPeakBytes() == 65, "budget current and peak" );
	void *pDenied = pAllocator->pfnAllocate( pAllocator->pContext, 36, 8 );
	check( pDenied == nullptr && budget.getRejectedAllocations() == 1, "budget denies overflow" );
	void *pSecond = pAllocator->pfnAllocate( pAllocator->pContext, 35, 32 );
	check( pSecond != nullptr && ( ( uintptr_t )pSecond & 31u ) == 0, "budget over-alignment" );
	check( budget.getPeakBytes() == 100, "budget peak full" );
	check( !budget.shutdown(), "budget rejects live shutdown" );
	pAllocator->pfnDeallocate( pAllocator->pContext, pSecond, 35, 32 );
	pAllocator->pfnDeallocate( pAllocator->pContext, pFirst, 65, 16 );
	check( budget.getLiveBytes() == 0, "budget fully reclaimed" );
	check( budget.shutdown(), "budget shutdown" );
}

static void testDelimited( const sirConfigParseContext_t& context, const sirDelimitedOptions_t& options ) noexcept
{
	const char szText[] = "a,\"b,c\",\"x\"\"y\"\r\n\"multi\nline\",,z\nlast,field,\n";
	SIRDelimitedReader reader;
	sirConfigInput_t input = { ( const uint8_t * )szText, sizeof( szText ) - 1, {} };
	check( reader.openBuffer( input, context, options ) == SIR_CONFIG_OK, "buffer open" );
	sirDelimitedRowView_t row = {};
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OK && row.nFields == 3, "first CSV row" );
	check( equalField( row, 0, "a", 1 ) && equalField( row, 1, "b,c", 3 ) && equalField( row, 2, "x\"y", 3 ), "quoted CSV values" );
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OK && row.nFields == 3, "second CSV row" );
	check( equalField( row, 0, "multi\nline", 10 ) && equalField( row, 1, "", 0 ) && equalField( row, 2, "z", 1 ), "multiline and empty" );
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OK && row.nFields == 3, "third CSV row" );
	check( equalField( row, 2, "", 0 ), "trailing empty field" );
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_END, "EOF after newline" );
	reader.close();
}

static void testStream( const sirConfigParseContext_t& context, const sirDelimitedOptions_t& options ) noexcept
{
	const char szText[] = "x,\"ab\r\ncd\"\r\ny,z\r\n";
	for ( uint32_t nChunk = 1; nChunk <= 7; ++nChunk ) {
		sirTestSource_t source = { ( const uint8_t * )szText, sizeof( szText ) - 1, 0, nChunk };
		sirConfigSource_t callback = { &source, sourceRead };
		SIRDelimitedReader reader;
		check( reader.openStream( callback, context, options ) == SIR_CONFIG_OK, "stream open" );
		sirDelimitedRowView_t row = {};
		check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OK && row.nFields == 2, "stream row 1" );
		check( equalField( row, 1, "ab\r\ncd", 6 ), "quoted CRLF preserved" );
		check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OK && row.nFields == 2, "stream row 2" );
		check( equalField( row, 0, "y", 1 ) && equalField( row, 1, "z", 1 ), "stream values" );
		check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_END, "stream end" );
		reader.close();
	}
}

static void testWriter( const sirConfigParseContext_t& context, const sirDelimitedOptions_t& options ) noexcept
{
	sirTestSink_t output = { {}, 0, 1024 };
	sirConfigSink_t sink = { &output, sinkWrite };
	SIRDelimitedWriter writer;
	check( writer.open( sink, context, options ) == SIR_CONFIG_OK, "writer open" );
	sirStringView_t fields[] = { { "a", 1 }, { "b,c", 3 }, { "d\"e", 3 } };
	check( writer.writeRow( fields, 3 ) == SIR_CONFIG_OK, "writer row" );
	check( writer.finish() == SIR_CONFIG_OK, "writer finish" );
	const char szExpected[] = "a,\"b,c\",\"d\"\"e\"\r\n";
	check( output.nLength == sizeof( szExpected ) - 1 && memcmp( output.szData, szExpected, sizeof( szExpected ) - 1 ) == 0,
	       "writer escaped output" );
	writer.close();

	sirTestSink_t failing = { {}, 0, 2 };
	sirConfigSink_t badSink = { &failing, sinkWrite };
	check( writer.open( badSink, context, options ) == SIR_CONFIG_OK, "writer reopen" );
	check( writer.writeRow( fields, 3 ) == SIR_CONFIG_IO_ERROR, "writer error propagates" );
	check( writer.writeRow( fields, 3 ) == SIR_CONFIG_IO_ERROR, "writer poisoned" );
	writer.close();
}

static void testFailures( sirConfigParseContext_t context, const sirDelimitedOptions_t& options ) noexcept
{
	const char szBroken[] = "a,\"unfinished";
	SIRDelimitedReader reader;
	sirConfigInput_t broken = { ( const uint8_t * )szBroken, sizeof( szBroken ) - 1, {} };
	check( reader.openBuffer( broken, context, options ) == SIR_CONFIG_OK, "broken open" );
	sirDelimitedRowView_t row = {};
	sirConfigDiagnostic_t diagnostic = {};
	check( reader.nextRow( &row, &diagnostic ) == SIR_CONFIG_SYNTAX_ERROR, "unterminated quoted string" );
	check( diagnostic.nByteOffset == broken.nBytes, "diagnostic byte offset" );
	reader.close();

	context.limits.nMaxRowBytes = 3;
	const char szLong[] = "abcdef\n";
	sirConfigInput_t longInput = { ( const uint8_t * )szLong, sizeof( szLong ) - 1, {} };
	check( reader.openBuffer( longInput, context, options ) == SIR_CONFIG_OK, "limited open" );
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_LIMIT_EXCEEDED, "row length budget" );
	reader.close();

	context.limits.nMaxRowBytes = 100;
	context.limits.nMaxColumns = 1;
	const char szColumns[] = "a,b\n";
	sirConfigInput_t columnInput = { ( const uint8_t * )szColumns, sizeof( szColumns ) - 1, {} };
	check( reader.openBuffer( columnInput, context, options ) == SIR_CONFIG_OK, "column limit open" );
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_LIMIT_EXCEEDED, "column count budget" );
	reader.close();
}

static void testTSV( const sirConfigParseContext_t& context ) noexcept
{
	const sirDelimitedOptions_t options = SIRTsv::defaultOptions();
	const char szText[] = "name\tvalue\r\nalpha\t\"tab\tinside\"\n";
	sirConfigInput_t input = { ( const uint8_t * )szText, sizeof( szText ) - 1, {} };
	SIRDelimitedReader reader;
	check( reader.openBuffer( input, context, options ) == SIR_CONFIG_OK, "TSV open" );
	sirDelimitedRowView_t row = {};
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OK && row.nFields == 2, "TSV header" );
	check( equalField( row, 0, "name", 4 ) && equalField( row, 1, "value", 5 ), "TSV header fields" );
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OK && row.nFields == 2, "TSV value row" );
	check( equalField( row, 1, "tab\tinside", 10 ), "TSV escaped delimiter" );
	reader.close();
}

static void testOutOfMemory( const sirContainerAllocator_t *pBacking, sirConfigParseContext_t context ) noexcept
{
	SIRConfigBudget tiny;
	check( tiny.init( pBacking, 16 ), "small budget init" );
	context.pScratchAllocator = tiny.getAllocator();
	const char szText[] = "a,b,c\n";
	sirConfigInput_t input = { ( const uint8_t * )szText, sizeof( szText ) - 1, {} };
	SIRDelimitedReader reader;
	check( reader.openBuffer( input, context, SIRCsv::defaultOptions() ) == SIR_CONFIG_OK, "small budget buffer open" );
	sirDelimitedRowView_t row = {};
	check( reader.nextRow( &row, nullptr ) == SIR_CONFIG_OUT_OF_MEMORY, "small budget denied row allocation" );
	check( tiny.getRejectedAllocations() >= 1, "allocation denial tracked" );
	reader.close();
	check( tiny.getLiveBytes() == 0 && tiny.shutdown(), "denied parse releases scratch" );
}

static void testOutputLimit( sirConfigParseContext_t context, const sirDelimitedOptions_t& options ) noexcept
{
	context.limits.nMaxOutputBytes = 4;
	sirTestSink_t output = { {}, 0, 1024 };
	const sirConfigSink_t sink = { &output, sinkWrite };
	SIRDelimitedWriter writer;
	check( writer.open( sink, context, options ) == SIR_CONFIG_OK, "bounded writer open" );
	const sirStringView_t fields[] = { { "abc", 3 }, { "def", 3 } };
	check( writer.writeRow( fields, 2 ) == SIR_CONFIG_LIMIT_EXCEEDED, "output cap rejected" );
	check( output.nLength == 0, "output cap preflight is transactional" );
	writer.close();
}

int main( void )
{
	sirContainerAllocator_t backing = { nullptr, backingAllocate, backingFree, 0, 19 };
	testBudgets( &backing );

	SIRConfigBudget scratch;
	check( scratch.init( &backing, 32768 ), "scratch init" );
	sirConfigParseContext_t context = {};
	context.pScratchAllocator = scratch.getAllocator();
	context.limits.nMaxInputBytes = 100000;
	context.limits.nMaxScratchLiveBytes = 32768;
	context.limits.nMaxOutputBytes = 100000;
	context.limits.nMaxRowBytes = 4096;
	context.limits.nMaxScalarBytes = 4096;
	context.limits.nMaxColumns = 32;
	context.limits.nMaxRows = 128;
	sirDelimitedOptions_t options = SIRCsv::defaultOptions();

	testDelimited( context, options );
	testStream( context, options );
	testWriter( context, options );
	testFailures( context, options );
	testTSV( context );
	testOutOfMemory( &backing, context );
	testOutputLimit( context, options );
	check( scratch.getLiveBytes() == 0, "all parser scratch freed" );
	check( scratch.shutdown(), "scratch shutdown" );

	fprintf( stdout, "%u failures\n", s_nFailures );
	return s_nFailures == 0 ? 0 : 1;
}
