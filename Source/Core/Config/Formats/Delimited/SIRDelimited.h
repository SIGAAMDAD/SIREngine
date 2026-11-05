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

#ifndef __SIRENGINE_SIRDELIMITED_H__
#define __SIRENGINE_SIRDELIMITED_H__

#include <SIRPCH.hpp>

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

typedef enum sirDelimitedState_e : uint8_t
{
	SIR_DELIMITED_START = 0,
	SIR_DELIMITED_UNQUOTED,
	SIR_DELIMITED_QUOTED,
	SIR_DELIMITED_AFTER_QUOTE
} sirDelimitedState_t;

class SIRDelimitedReader
{
public:
	SIRDelimitedReader( void ) noexcept;
	SIRDelimitedReader( const SIRDelimitedReader& ) = delete;
	SIRDelimitedReader& operator=( const SIRDelimitedReader& ) = delete;
	~SIRDelimitedReader( void ) noexcept;

	sirConfigResult_t openBuffer( const sirConfigInput_t& input, const sirConfigParseContext_t& context,
	                              const sirDelimitedOptions_t& options ) noexcept;
	sirConfigResult_t openStream( const sirConfigSource_t& source, const sirConfigParseContext_t& context,
	                              const sirDelimitedOptions_t& options ) noexcept;
	sirConfigResult_t nextRow( sirDelimitedRowView_t *pRow, sirConfigDiagnostic_t *pDiagnostic ) noexcept;
	void close( void ) noexcept;

private:
	sirConfigResult_t openCommon( const sirConfigParseContext_t& context, const sirDelimitedOptions_t& options ) noexcept;
	sirConfigResult_t readByte( uint8_t *pByte, bool *pEnd ) noexcept;
	sirConfigResult_t appendByte( uint8_t nByte ) noexcept;
	sirConfigResult_t completeField( void ) noexcept;
	sirConfigResult_t completeRow( sirDelimitedRowView_t *pRow ) noexcept;
	sirConfigResult_t fail( sirConfigResult_t eResult, sirConfigDiagnostic_t *pDiagnostic ) noexcept;
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
	sirDelimitedState_t m_eState;
	sirConfigResult_t m_eFailure;
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
	sirConfigResult_t open( const sirConfigSink_t& sink, const sirConfigParseContext_t& context,
	                        const sirDelimitedOptions_t& options ) noexcept;
	sirConfigResult_t writeRow( const sirStringView_t *pFields, uint32_t nFieldCount ) noexcept;
	sirConfigResult_t finish( void ) noexcept;
	void close( void ) noexcept;

private:
	bool needsQuote( const sirStringView_t& field ) const noexcept;
	sirConfigResult_t writeSpan( const uint8_t *pBytes, uint64_t nBytes ) noexcept;
	sirConfigResult_t writeByte( uint8_t nByte ) noexcept;
	sirConfigResult_t computeRowBytes( const sirStringView_t *pFields, uint32_t nFieldCount, uint64_t *pBytes ) const noexcept;

	sirConfigSink_t m_Sink;
	sirConfigLimits_t m_Limits;
	sirDelimitedOptions_t m_Options;
	uint64_t m_nOutputBytes;
	uint32_t m_nRows;
	sirConfigResult_t m_eFailure;
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

#endif // __SIRENGINE_SIRDELIMITED_H__
