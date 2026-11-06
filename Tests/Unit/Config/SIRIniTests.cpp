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

#include <Core/Config/Formats/INI/SIRIni.h>
#include <Core/Config/SIRConfigAtomicFile.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

typedef struct sirIniTestSink_s
{
	char szText[2048];
	uint32_t nWritten;
	uint32_t nFailAfter;
} sirIniTestSink_t;

static sirConfigResult_t sirIniTestWrite( void *pContext, const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	sirIniTestSink_t *pSink = ( sirIniTestSink_t * )pContext;
	if ( pSink->nWritten + nBytes >= sizeof( pSink->szText ) ||
	     ( pSink->nFailAfter != 0 && pSink->nWritten + nBytes > pSink->nFailAfter ) ) {
		return SIR_CONFIG_IO_ERROR;
	}
	memcpy( pSink->szText + pSink->nWritten, pBytes, ( size_t )nBytes );
	pSink->nWritten += ( uint32_t )nBytes;
	pSink->szText[pSink->nWritten] = 0;
	return SIR_CONFIG_OK;
}

static sirConfigResult_t sirIniTestParse( const char *pszText, const sirConfigLimits_t& limits, const sirIniStorage_t& storage,
                                          sirIniParseResult_t *pResult ) noexcept
{
	const sirConfigInput_t input = { ( const uint8_t * )pszText, strlen( pszText ), {} };
	return SIRIniCodec::parse( input, limits, storage, pResult );
}

int main( void )
{
	sirIniEntry_t szEntries[6] = {};
	char szValues[6 * 128] = {};
	sirIniStorage_t storage = { szEntries, 6, szValues, 128, sizeof( szValues ) };
	sirConfigLimits_t limits = {};
	limits.nMaxInputBytes = 1024;
	limits.nMaxScalarBytes = 127;
	limits.nMaxRowBytes = 256;
	limits.nMaxNodes = 6;
	sirIniParseResult_t result = {};
	const char *pszInput = "; Comment\r\n[CVars]\r\nr.alpha = \"a\\n\\t\\\"b\\\\c\" ; comment\r\nr.gamma = 12.5\n";
	assert( sirIniTestParse( pszInput, limits, storage, &result ) == SIR_CONFIG_OK );
	assert( result.nCount == 2 );
	assert( strcmp( szEntries[0].szSection, "CVars" ) == 0 && strcmp( szEntries[0].szKey, "r.alpha" ) == 0 );
	assert( szEntries[0].nValueBytes == 7 && memcmp( szEntries[0].pszValue, "a\n\t\"b\\c", 7 ) == 0 );
	assert( strcmp( szEntries[1].pszValue, "12.5" ) == 0 );
	sirIniTestSink_t sink = {};
	const sirConfigSink_t writer = { &sink, sirIniTestWrite };
	assert( SIRIniCodec::write( szEntries, result.nCount, writer, sizeof( sink.szText ) - 1 ) == SIR_CONFIG_OK );
	assert( strcmp( sink.szText, "[CVars]\nr.alpha = \"a\\n\\t\\\"b\\\\c\"\nr.gamma = \"12.5\"\n" ) == 0 );
	assert( sirIniTestParse( sink.szText, limits, storage, &result ) == SIR_CONFIG_OK );
	assert( result.nCount == 2 && szEntries[0].nValueBytes == 7 );

	assert( sirIniTestParse( "[CVars]\nr.foo=1\nR.FOO=2\n", limits, storage, &result ) == SIR_CONFIG_DUPLICATE_KEY );
	assert( result.diagnostic.nLine == 3 );
	assert( sirIniTestParse( "[CVars]\nr.foo=\"unclosed\n", limits, storage, &result ) == SIR_CONFIG_SYNTAX_ERROR );
	assert( sirIniTestParse( "[CVars]\nr.foo=\"bad\\q\"\n", limits, storage, &result ) == SIR_CONFIG_SYNTAX_ERROR );
	assert( sirIniTestParse( "[CVars]\nr.foo=\"x\"extra\n", limits, storage, &result ) == SIR_CONFIG_SYNTAX_ERROR );
	assert( sirIniTestParse( "[CVars]\nr.foo=2\r\n[]\n", limits, storage, &result ) == SIR_CONFIG_SYNTAX_ERROR );
	assert( sirIniTestParse( "broken", limits, storage, &result ) == SIR_CONFIG_SYNTAX_ERROR );
	const uint8_t szInvalidUtf8[] = { '[', 'C', 'V', 'a', 'r', 's', ']', '\n', 'x', '=', 0xc0, 0xaf, '\n' };
	assert( SIRIniCodec::parse( { szInvalidUtf8, sizeof( szInvalidUtf8 ), {} }, limits, storage, &result ) == SIR_CONFIG_INVALID_ENCODING );
	const uint8_t szNul[] = { 'x', '=', '1', 0, '\n' };
	assert( SIRIniCodec::parse( { szNul, sizeof( szNul ), {} }, limits, storage, &result ) == SIR_CONFIG_INVALID_ENCODING );
	limits.nMaxInputBytes = 4;
	assert( sirIniTestParse( "[CVars]", limits, storage, &result ) == SIR_CONFIG_LIMIT_EXCEEDED );
	limits.nMaxInputBytes = 1024;
	assert( SIRIniCodec::write( szEntries, 2, writer, 3 ) == SIR_CONFIG_LIMIT_EXCEEDED );

	const char *pszTarget = "sir-p5-archive-unit.ini";
	const char *pszTemporary = "sir-p5-archive-unit.tmp";
	SIRPlatformFile::remove( pszTemporary );
	SIRPlatformFile::remove( pszTarget );
	SIRConfigAtomicFile atomic;
	assert( atomic.open( pszTarget, pszTemporary, 1024 ) == SIR_CONFIG_OK );
	const char *pszOld = "[CVars]\nr.alpha = \"old\"\n";
	const sirConfigSink_t fileSink = atomic.getSink();
	assert( fileSink.pfnWrite( fileSink.pContext, ( const uint8_t * )pszOld, strlen( pszOld ) ) == SIR_CONFIG_OK );
	assert( atomic.commit() == SIR_CONFIG_OK );
	uint8_t szFile[256] = {};
	uint32_t nRead = 0;
	assert( SIRConfigAtomicFile::read( pszTarget, szFile, sizeof( szFile ), &nRead ) == SIR_CONFIG_OK );
	assert( nRead == strlen( pszOld ) && memcmp( szFile, pszOld, nRead ) == 0 );
	assert( atomic.open( pszTarget, pszTemporary, 3 ) == SIR_CONFIG_OK );
	assert( fileSink.pfnWrite( fileSink.pContext, ( const uint8_t * )pszOld, strlen( pszOld ) ) == SIR_CONFIG_LIMIT_EXCEEDED );
	assert( atomic.commit() == SIR_CONFIG_IO_ERROR );
	memset( szFile, 0, sizeof( szFile ) );
	assert( SIRConfigAtomicFile::read( pszTarget, szFile, sizeof( szFile ), &nRead ) == SIR_CONFIG_OK );
	assert( nRead == strlen( pszOld ) && memcmp( szFile, pszOld, nRead ) == 0 );
	assert( atomic.open( pszTarget, pszTemporary, 1024 ) == SIR_CONFIG_OK );
	SIRConfigAtomicFile denied;
	assert( denied.open( pszTarget, pszTemporary, 1024 ) == SIR_CONFIG_IO_ERROR );
	atomic.cancel();
	assert( SIRPlatformFile::remove( pszTarget ) == SIR_FILE_OK );
	return 0;
}
