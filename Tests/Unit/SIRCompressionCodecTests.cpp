/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://www.mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/Compression/SIRBZip2Codec.h>
#include <Core/Compression/SIRLZ4Codec.h>
#include <Core/Compression/SIRZlibCodec.h>
#include <Core/Compression/SIRZstdCodec.h>
#include <stdio.h>
#include <string.h>

static bool sirCompressionCheck( bool bCondition, const char *pszExpression, int nLine )
{
	if ( !bCondition ) {
		fprintf( stderr, "SIR compression codec test failed at line %d: %s\n", nLine, pszExpression );
	}
	return bCondition;
}

#define SIR_COMPRESSION_CHECK( expression ) sirCompressionCheck( ( expression ), #expression, __LINE__ )

static bool testCodec( SIRCompressionCodec& rCodec, SIRCompressionType eExpectedType )
{
	uint8_t szInput[4096] = {};
	for ( uint32_t iIndex = 0; iIndex < sizeof( szInput ); ++iIndex ) {
		szInput[iIndex] = static_cast<uint8_t>( iIndex % 29 );
	}
	uint8_t szEncoded[8192] = {};
	uint8_t szDecoded[4096] = {};
	uint8_t szScratch[1] = {};
	SIRSpan<uint8_t> Input( szInput, sizeof( szInput ) );
	SIRSpan<uint8_t> Encoded( szEncoded, sizeof( szEncoded ) );
	SIRSpan<uint8_t> Decoded( szDecoded, sizeof( szDecoded ) );
	SIRSpan<uint8_t> Scratch( szScratch, sizeof( szScratch ) );
	SIRCompressionResult eResult = SIRCompressionResult::SIR_COMPRESSION_SUCCESS;
	size_t nEncoded = 0;
	size_t nDecoded = 0;

	if ( !SIR_COMPRESSION_CHECK( rCodec.getType() == eExpectedType ) ||
	     !SIR_COMPRESSION_CHECK( rCodec.getMaximumEncodedSize( sizeof( szInput ) ) <= sizeof( szEncoded ) ) ||
	     !SIR_COMPRESSION_CHECK( rCodec.getScratchSize() == 0 ) ) {
		return false;
	}
	eResult = rCodec.encode( Input, Encoded, Scratch, nEncoded );
	if ( !SIR_COMPRESSION_CHECK( eResult == SIRCompressionResult::SIR_COMPRESSION_SUCCESS ) ||
	     !SIR_COMPRESSION_CHECK( nEncoded > 0 && nEncoded <= sizeof( szEncoded ) ) ) {
		return false;
	}
	SIRSpan<uint8_t> EncodedPayload( szEncoded, nEncoded );
	eResult = rCodec.decode( EncodedPayload, Decoded, Scratch, nDecoded );
	return SIR_COMPRESSION_CHECK( eResult == SIRCompressionResult::SIR_COMPRESSION_SUCCESS ) &&
	       SIR_COMPRESSION_CHECK( nDecoded == sizeof( szInput ) ) &&
	       SIR_COMPRESSION_CHECK( memcmp( szInput, szDecoded, sizeof( szInput ) ) == 0 );
}

static bool testValidation( SIRCompressionCodec& rCodec )
{
	uint8_t szInput[1024] = {};
	uint8_t szEncoded[4096] = {};
	uint8_t szOutput[1024] = {};
	uint8_t nOneByte = 0;
	SIRSpan<uint8_t> Input( szInput, sizeof( szInput ) );
	SIRSpan<uint8_t> Encoded( szEncoded, sizeof( szEncoded ) );
	SIRSpan<uint8_t> Output( szOutput, sizeof( szOutput ) );
	SIRSpan<uint8_t> SmallOutput( &nOneByte, 1 );
	SIRSpan<uint8_t> InvalidInput( nullptr, 1 );
	size_t nBytesWritten = 17;

	if ( !SIR_COMPRESSION_CHECK( rCodec.encode( InvalidInput, Encoded, nBytesWritten ) ==
	                             SIRCompressionResult::SIR_COMPRESSION_INVALID_ARGUMENT ) ||
	     !SIR_COMPRESSION_CHECK( nBytesWritten == 0 ) ) {
		return false;
	}
	nBytesWritten = 17;
	if ( !SIR_COMPRESSION_CHECK( rCodec.encode( Input, SmallOutput, nBytesWritten ) ==
	                             SIRCompressionResult::SIR_COMPRESSION_INSUFFICIENT_CAPACITY ) ||
	     !SIR_COMPRESSION_CHECK( nBytesWritten == 0 ) ) {
		return false;
	}
	SIRSpan<uint8_t> EmptyInput( nullptr, 0 );
	nBytesWritten = 17;
	if ( !SIR_COMPRESSION_CHECK( rCodec.encode( EmptyInput, Encoded, nBytesWritten ) == SIRCompressionResult::SIR_COMPRESSION_SUCCESS ) ||
	     !SIR_COMPRESSION_CHECK( nBytesWritten <= sizeof( szEncoded ) ) ) {
		return false;
	}
	SIRSpan<uint8_t> EmptyEncoded( szEncoded, nBytesWritten );
	nBytesWritten = 17;
	return SIR_COMPRESSION_CHECK( rCodec.decode( EmptyEncoded, Output, nBytesWritten ) == SIRCompressionResult::SIR_COMPRESSION_SUCCESS ) &&
	       SIR_COMPRESSION_CHECK( nBytesWritten == 0 );
}

int main( void )
{
	SIRLZ4Codec lz4;
	SIRBZip2Codec bzip2;
	SIRZstdCodec zstd;
	SIRZlibCodec zlib;
	return testCodec( lz4, SIRCompressionType::SIR_COMPRESSION_TYPE_LZ4 ) && testValidation( lz4 ) &&
	               testCodec( bzip2, SIRCompressionType::SIR_COMPRESSION_TYPE_BZIP2 ) && testValidation( bzip2 ) &&
	               testCodec( zstd, SIRCompressionType::SIR_COMPRESSION_TYPE_ZSTD ) && testValidation( zstd ) &&
	               testCodec( zlib, SIRCompressionType::SIR_COMPRESSION_TYPE_ZLIB ) && testValidation( zlib )
	           ? 0
	           : 1;
}
