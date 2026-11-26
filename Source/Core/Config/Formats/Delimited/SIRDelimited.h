/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_DELIMITED_H__
#define __SIR_DELIMITED_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Config/SIRConfigTypes.h>

typedef struct sirDelimitedOptions_s
{
	uint8_t nDelimiter;
	uint8_t nQuote;
	bool bStrictQuotes;
	bool bWriteCRLF;
} sirDelimitedOptions_t;

typedef struct sirDelimitedField_s
{
	sirStringView_t value;
	uint32_t nOffset;
} sirDelimitedField_t;

typedef struct sirDelimitedRowView_s
{
	const sirDelimitedField_t *pFields;
	uint32_t nFields;
} sirDelimitedRowView_t;

enum class SIRDelimitedState : uint8_t {
	SIR_DELIMITED_START = 0,
	SIR_DELIMITED_UNQUOTED,
	SIR_DELIMITED_QUOTED,
	SIR_DELIMITED_AFTER_QUOTE
};

class SIRDelimitedReader
{
public:
	SIRDelimitedReader( void ) noexcept;
	SIRDelimitedReader( const SIRDelimitedReader& ) = delete;
	SIRDelimitedReader& operator=( const SIRDelimitedReader& ) = delete;
	~SIRDelimitedReader( void ) noexcept;

	SIRConfigResult openBuffer( const sirConfigInput_t& input, const sirConfigParseContext_t& context,
	                              const sirDelimitedOptions_t& options ) noexcept;
	SIRConfigResult openStream( const sirConfigSource_t& source, const sirConfigParseContext_t& context,
	                              const sirDelimitedOptions_t& options ) noexcept;
	SIRConfigResult nextRow( sirDelimitedRowView_t *pRow, sirConfigDiagnostic_t *pDiagnostic ) noexcept;
	void close( void ) noexcept;

private:
	SIRConfigResult openCommon( const sirConfigParseContext_t& context, const sirDelimitedOptions_t& options ) noexcept;
	SIRConfigResult readByte( uint8_t *pByte, bool *pEnd ) noexcept;
	SIRConfigResult appendByte( uint8_t nByte ) noexcept;
	SIRConfigResult completeField( void ) noexcept;
	SIRConfigResult completeRow( sirDelimitedRowView_t *pRow ) noexcept;
	SIRConfigResult fail( SIRConfigResult eResult, sirConfigDiagnostic_t *pDiagnostic ) noexcept;
	bool validOptions( const sirDelimitedOptions_t& options ) const noexcept;

	const sirContainerAllocator_t *m_pAllocator;
	sirConfigLimits_t m_Limits;
	sirDelimitedOptions_t m_Options;
	sirConfigInput_t m_Input;
	sirConfigSource_t m_Source;
	uint8_t *m_pRowBytes;
	sirDelimitedField_t *m_pFields;
	uint32_t m_nRowCapacity;
	uint32_t m_nFieldCapacity;
	uint32_t m_nRowBytes;
	uint32_t m_nFields;
	uint32_t m_nFieldStart;
	uint32_t m_nRows;
	uint64_t m_nOffset;
	uint64_t m_nFetched;
	uint32_t m_nLine;
	uint32_t m_nColumn;
	uint32_t m_nChunkStart;
	uint32_t m_nChunkLength;
	SIRDelimitedState m_eState;
	SIRConfigResult m_eFailure;
	bool m_bOpened;
	bool m_bStreaming;
	bool m_bEnd;
	bool m_bRowStarted;
	bool m_bSkipLF;
	bool m_bPrevCR;
	uint8_t m_szChunk[1024];
};

class SIRDelimitedWriter
{
public:
	SIRDelimitedWriter( void ) noexcept;
	SIRConfigResult open( const sirConfigSink_t& sink, const sirConfigParseContext_t& context,
	                        const sirDelimitedOptions_t& options ) noexcept;
	SIRConfigResult writeRow( const sirStringView_t *pFields, uint32_t nFieldCount ) noexcept;
	SIRConfigResult finish( void ) noexcept;
	void close( void ) noexcept;

private:
	bool needsQuote( const sirStringView_t& field ) const noexcept;
	SIRConfigResult writeSpan( const uint8_t *pBytes, uint64_t nBytes ) noexcept;
	SIRConfigResult writeByte( uint8_t nByte ) noexcept;
	SIRConfigResult computeRowBytes( const sirStringView_t *pFields, uint32_t nFieldCount, uint64_t *pBytes ) const noexcept;

	sirConfigSink_t m_Sink;
	sirConfigLimits_t m_Limits;
	sirDelimitedOptions_t m_Options;
	uint64_t m_nOutputBytes;
	uint32_t m_nRows;
	SIRConfigResult m_eFailure;
	bool m_bOpened;
};

class SIRCsv
{
public:
	static sirDelimitedOptions_t defaultOptions( void ) noexcept;
};

class SIRTsv
{
public:
	static sirDelimitedOptions_t defaultOptions( void ) noexcept;
};

#endif // __SIR_DELIMITED_H__
