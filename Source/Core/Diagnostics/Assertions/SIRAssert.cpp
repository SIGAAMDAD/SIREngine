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

#include <Core/Diagnostics/Assertions/SIRAssert.h>
#include <Core/Diagnostics/SIRDiagnosticBuffer.h>
#include <Core/Diagnostics/SIREmergencyDiagnostics.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformContext.h>

#if defined( _MSC_VER )
#include <intrin.h>
#define SIR_ASSERT_RETURN_ADDRESS() _ReturnAddress()
#else
#define SIR_ASSERT_RETURN_ADDRESS() __builtin_extract_return_addr( __builtin_return_address( 0 ) )
#endif

class SIRAssertReport final
{
public:
	explicit SIRAssertReport( void ) = delete;
	explicit SIRAssertReport( const SIRAssertReport& ) = delete;
	explicit SIRAssertReport( SIRAssertReport&& ) = delete;

	SIRAssertReport& operator=( const SIRAssertReport& ) = delete;
	SIRAssertReport& operator=( SIRAssertReport&& ) = delete;

	[[nodiscard]] static sirEmergencyRecord_t
	makeRecord( const sirAssertContext_t& context, sirEmergencySeverity eSeverity, const void *pReturnAddress ) noexcept;
	static void setMessage( sirEmergencyRecord_t& data, const char *pszMessage ) noexcept;
	static void formatMessage( sirEmergencyRecord_t& data, const char *pszFormat, va_list& args ) noexcept;

private:
	enum class sirLength : uint8_t
	{
		SIR_LENGTH_DEFAULT,
		SIR_LENGTH_HH,
		SIR_LENGTH_H,
		SIR_LENGTH_L,
		SIR_LENGTH_LL,
		SIR_LENGTH_J,
		SIR_LENGTH_Z,
		SIR_LENGTH_T
	};

	enum class sirFormatFlag : uint8_t
	{
		SIR_FORMAT_LEFT = 1,
		SIR_FORMAT_PLUS = 2,
		SIR_FORMAT_SPACE = 4,
		SIR_FORMAT_ZERO = 8,
		SIR_FORMAT_ALTERNATE = 16
	};

	typedef struct sirFormat_s
	{
		sirLength eLength;
		uint8_t nFlags;
		uint32_t nWidth;
		int32_t nPrecision;
		char nConversion;
	} sirFormat_t;

	typedef struct sirField_s
	{
		const char *pText;
		uint32_t nSize;
		uint32_t nZeros;
		char szPrefix[4];
	} sirField_t;

	static constexpr int32_t s_nMaxWidth = 384;
	static constexpr int32_t s_nMaxFloatPrecision = 64;

	[[nodiscard]] static bool hasFlag( const sirFormat_t& format, sirFormatFlag eFlag ) noexcept;
	[[nodiscard]] static sirFormat_t parseFormat( const char *& pCursor, va_list& args ) noexcept;
	[[nodiscard]] static int32_t parseNumber( const char *& pCursor ) noexcept;
	[[nodiscard]] static int64_t readSigned( sirLength eLength, va_list& args ) noexcept;
	[[nodiscard]] static uint64_t readUnsigned( sirLength eLength, va_list& args ) noexcept;
	static void writeField( sirDiagnosticBuffer_t& output, const sirFormat_t& format, const sirField_t& field ) noexcept;
	static void formatInteger( sirDiagnosticBuffer_t& output, sirFormat_t format, va_list& args ) noexcept;
	[[nodiscard]] static bool formatFloat( sirDiagnosticBuffer_t& output, const sirFormat_t& format, va_list& args ) noexcept;
	[[nodiscard]] static bool formatField( sirDiagnosticBuffer_t& output, const sirFormat_t& format, va_list& args ) noexcept;
};

sirEmergencyRecord_t SIRAssertReport::makeRecord( const sirAssertContext_t& context, const sirEmergencySeverity eSeverity, const void *pReturnAddress ) noexcept
{
	sirEmergencyRecord_t data{};
	data.eSeverity = eSeverity;
	data.nLine = context.source.nLine;
	data.nReturnAddress = reinterpret_cast<uintptr_t>( pReturnAddress ); // Preserve the raw native return address.
	sirPlatformContext_t native{};
	if ( SIRPlatformContext::capture( &native ) ) {
		data.nProcessId = native.nProcessId;
		data.nThreadId = native.nThreadId;
	}
	sirDiagnosticBuffer_t{ data.szFile, sizeof( data.szFile ), 0 }.append( context.source.pszFile );
	sirDiagnosticBuffer_t{ data.szFunction, sizeof( data.szFunction ), 0 }.append( context.source.pszFunction );
	sirDiagnosticBuffer_t{ data.szExpression, sizeof( data.szExpression ), 0 }.append( context.pszExpression );
	return data;
}

void SIRAssertReport::setMessage( sirEmergencyRecord_t& data, const char *pszMessage ) noexcept
{
	sirDiagnosticBuffer_t{ data.szMessage, sizeof( data.szMessage ), 0 }.append( pszMessage );
}

bool SIRAssertReport::hasFlag( const sirFormat_t& format, const sirFormatFlag eFlag ) noexcept
{
	return ( format.nFlags & static_cast<uint8_t>( eFlag ) ) != 0;
}

int32_t SIRAssertReport::parseNumber( const char *& pCursor ) noexcept
{
	int32_t nValue = 0;
	while ( *pCursor >= '0' && *pCursor <= '9' ) {
		const int32_t nNext = nValue * 10 + ( *pCursor++ - '0' );
		nValue = nNext > s_nMaxWidth ? s_nMaxWidth : nNext;
	}
	return nValue;
}

SIRAssertReport::sirFormat_t SIRAssertReport::parseFormat( const char *& pCursor, va_list& args ) noexcept
{
	sirFormat_t format{};
	format.nPrecision = -1;
	for ( bool bFlags = true; bFlags; ) {
		switch ( *pCursor ) {
		case '-':
			format.nFlags |= static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_LEFT );
			break;
		case '+':
			format.nFlags |= static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_PLUS );
			break;
		case ' ':
			format.nFlags |= static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_SPACE );
			break;
		case '0':
			format.nFlags |= static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_ZERO );
			break;
		case '#':
			format.nFlags |= static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_ALTERNATE );
			break;
		default:
			bFlags = false;
			continue;
		}
		++pCursor;
	}
	if ( *pCursor == '*' ) {
		++pCursor;
		const int64_t nWidth = va_arg( args, int32_t );
		if ( nWidth < 0 ) {
			format.nFlags |= static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_LEFT );
		}
		const int64_t nMagnitude = nWidth < 0 ? -nWidth : nWidth;
		format.nWidth = static_cast<uint32_t>( nMagnitude > s_nMaxWidth ? s_nMaxWidth : nMagnitude );
	} else {
		format.nWidth = static_cast<uint32_t>( parseNumber( pCursor ) );
	}
	if ( *pCursor == '.' ) {
		++pCursor;
		if ( *pCursor == '*' ) {
			++pCursor;
			const int32_t nPrecision = va_arg( args, int32_t );
			format.nPrecision = nPrecision > s_nMaxWidth ? s_nMaxWidth : nPrecision;
		} else {
			format.nPrecision = parseNumber( pCursor );
		}
	}
	switch ( *pCursor ) {
	case 'h':
		++pCursor;
		format.eLength = sirLength::SIR_LENGTH_H;
		if ( *pCursor == 'h' ) {
			++pCursor;
			format.eLength = sirLength::SIR_LENGTH_HH;
		}
		break;
	case 'l':
		++pCursor;
		format.eLength = sirLength::SIR_LENGTH_L;
		if ( *pCursor == 'l' ) {
			++pCursor;
			format.eLength = sirLength::SIR_LENGTH_LL;
		}
		break;
	case 'j':
		++pCursor;
		format.eLength = sirLength::SIR_LENGTH_J;
		break;
	case 'z':
		++pCursor;
		format.eLength = sirLength::SIR_LENGTH_Z;
		break;
	case 't':
		++pCursor;
		format.eLength = sirLength::SIR_LENGTH_T;
		break;
	default:
		break;
	}
	format.nConversion = *pCursor;
	if ( *pCursor != '\0' ) {
		++pCursor;
	}
	return format;
}

int64_t SIRAssertReport::readSigned( const sirLength eLength, va_list& args ) noexcept
{
	// The ellipsis requires the native printf ABI types even when, for
	// example, native long and the fixed-width type have the same size.
	switch ( eLength ) {
	case sirLength::SIR_LENGTH_HH:
		return static_cast<int8_t>( va_arg( args, int32_t ) );
	case sirLength::SIR_LENGTH_H:
		return static_cast<int16_t>( va_arg( args, int32_t ) );
	case sirLength::SIR_LENGTH_L:
		return va_arg( args, decltype( 0L ) );
	case sirLength::SIR_LENGTH_LL:
		return va_arg( args, decltype( 0LL ) );
	case sirLength::SIR_LENGTH_J:
		return va_arg( args, intmax_t );
	case sirLength::SIR_LENGTH_Z:
		return va_arg( args, std::make_signed_t<size_t> );
	case sirLength::SIR_LENGTH_T:
		return va_arg( args, ptrdiff_t );
	default:
		return va_arg( args, int32_t );
	}
}

uint64_t SIRAssertReport::readUnsigned( const sirLength eLength, va_list& args ) noexcept
{
	switch ( eLength ) {
	case sirLength::SIR_LENGTH_HH:
		return static_cast<uint8_t>( va_arg( args, int32_t ) );
	case sirLength::SIR_LENGTH_H:
		return static_cast<uint16_t>( va_arg( args, int32_t ) );
	case sirLength::SIR_LENGTH_L:
		return va_arg( args, decltype( 0UL ) );
	case sirLength::SIR_LENGTH_LL:
		return va_arg( args, decltype( 0ULL ) );
	case sirLength::SIR_LENGTH_J:
		return va_arg( args, uintmax_t );
	case sirLength::SIR_LENGTH_Z:
		return va_arg( args, size_t );
	case sirLength::SIR_LENGTH_T:
		return va_arg( args, std::make_unsigned_t<ptrdiff_t> );
	default:
		return va_arg( args, uint32_t );
	}
}

void SIRAssertReport::writeField( sirDiagnosticBuffer_t& output, const sirFormat_t& format, const sirField_t& field ) noexcept
{
	const uint32_t nPrefix = static_cast<uint32_t>( strlen( field.szPrefix ) );
	const uint32_t nTotal = nPrefix + field.nZeros + field.nSize;
	const uint32_t nPadding = format.nWidth > nTotal ? format.nWidth - nTotal : 0;
	const bool bLeft = hasFlag( format, sirFormatFlag::SIR_FORMAT_LEFT );
	const bool bZero = hasFlag( format, sirFormatFlag::SIR_FORMAT_ZERO );
	if ( !bLeft && !bZero ) {
		output.repeat( ' ', nPadding );
	}
	output.append( field.szPrefix, nPrefix );
	if ( !bLeft && bZero ) {
		output.repeat( '0', nPadding );
	}
	output.repeat( '0', field.nZeros );
	for ( uint32_t i = 0; i < field.nSize && output.nSize < output.nCapacity - 1; ++i ) {
		output.put( field.pText[i] );
	}
	if ( bLeft ) {
		output.repeat( ' ', nPadding );
	}
}

void SIRAssertReport::formatInteger( sirDiagnosticBuffer_t& output, sirFormat_t format, va_list& args ) noexcept
{
	const char nConversion = format.nConversion;
	const bool bSigned = nConversion == 'd' || nConversion == 'i';
	const bool bPointer = nConversion == 'p';
	const bool bHex = bPointer || nConversion == 'x' || nConversion == 'X';
	const uint8_t nBase = bHex ? 16 : ( nConversion == 'o' ? 8 : 10 );
	uint64_t nValue = 0;
	sirField_t field{};
	if ( bSigned ) {
		const int64_t nSigned = readSigned( format.eLength, args );
		nValue = static_cast<uint64_t>( nSigned );
		if ( nSigned < 0 ) {
			nValue = UINT64_C( 0 ) - nValue;
			field.szPrefix[0] = '-';
		} else if ( hasFlag( format, sirFormatFlag::SIR_FORMAT_PLUS ) ) {
			field.szPrefix[0] = '+';
		} else if ( hasFlag( format, sirFormatFlag::SIR_FORMAT_SPACE ) ) {
			field.szPrefix[0] = ' ';
		}
	} else if ( bPointer ) {
		nValue = reinterpret_cast<uintptr_t>( va_arg( args, void * ) ); // Native printf pointer representation.
	} else {
		nValue = readUnsigned( format.eLength, args );
	}
	char szDigits[32]{};
	const std::to_chars_result result = std::to_chars( szDigits, szDigits + sizeof( szDigits ), nValue, nBase );
	field.pText = szDigits;
	if ( nValue != 0 || format.nPrecision != 0 || bPointer ) {
		field.nSize = static_cast<uint32_t>( result.ptr - szDigits );
	}
	if ( nConversion == 'X' ) {
		for ( uint32_t i = 0; i < field.nSize; ++i ) {
			if ( szDigits[i] >= 'a' && szDigits[i] <= 'f' ) {
				szDigits[i] = static_cast<char>( szDigits[i] - 'a' + 'A' );
			}
		}
	}
	if ( bPointer || ( bHex && nValue != 0 && hasFlag( format, sirFormatFlag::SIR_FORMAT_ALTERNATE ) ) ) {
		field.szPrefix[0] = '0';
		field.szPrefix[1] = nConversion == 'X' ? 'X' : 'x';
	}
	if ( format.nPrecision >= 0 ) {
		format.nFlags &= static_cast<uint8_t>( ~static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_ZERO ) );
		const uint32_t nPrecision = static_cast<uint32_t>( format.nPrecision );
		field.nZeros = nPrecision > field.nSize ? nPrecision - field.nSize : 0;
	}
	if ( nBase == 8 && field.nZeros == 0 && hasFlag( format, sirFormatFlag::SIR_FORMAT_ALTERNATE ) ) {
		if ( nValue != 0 || field.nSize == 0 ) {
			field.nZeros = 1;
		}
	}
	writeField( output, format, field );
}

bool SIRAssertReport::formatFloat( sirDiagnosticBuffer_t& output, const sirFormat_t& format, va_list& args ) noexcept
{
	if ( format.eLength != sirLength::SIR_LENGTH_DEFAULT && format.eLength != sirLength::SIR_LENGTH_L ) {
		return false;
	}
	const char nConversion = format.nConversion;
	const bool bUpper = nConversion >= 'A' && nConversion <= 'Z';
	const char nLower = bUpper ? static_cast<char>( nConversion - 'A' + 'a' ) : nConversion;
	// C varargs promote printf floating-point arguments to double.
	const double fValue = va_arg( args, double );
	const bool bNegative = std::signbit( fValue );
	const double fMagnitude = bNegative ? -fValue : fValue;
	std::chars_format eStyle = std::chars_format::general;
	if ( nLower == 'a' ) {
		eStyle = std::chars_format::hex;
	} else if ( nLower == 'e' ) {
		eStyle = std::chars_format::scientific;
	} else if ( nLower == 'f' ) {
		eStyle = std::chars_format::fixed;
	}
	int32_t nPrecision = format.nPrecision < 0 ? 6 : format.nPrecision;
	if ( nLower == 'g' && nPrecision == 0 ) {
		nPrecision = 1;
	}
	if ( nPrecision > s_nMaxFloatPrecision ) {
		nPrecision = s_nMaxFloatPrecision;
	}
	char szDigits[384]{};
	const std::to_chars_result result = nLower == 'a' && format.nPrecision < 0
	                                        ? std::to_chars( szDigits, szDigits + sizeof( szDigits ), fMagnitude, eStyle )
	                                        : std::to_chars( szDigits, szDigits + sizeof( szDigits ), fMagnitude, eStyle, nPrecision );
	if ( result.ec != std::errc{} ) {
		return false;
	}
	sirField_t field{};
	field.pText = szDigits;
	field.nSize = static_cast<uint32_t>( result.ptr - szDigits );
	uint32_t nPrefix = 0;
	if ( bNegative ) {
		field.szPrefix[nPrefix++] = '-';
	} else if ( hasFlag( format, sirFormatFlag::SIR_FORMAT_PLUS ) ) {
		field.szPrefix[nPrefix++] = '+';
	} else if ( hasFlag( format, sirFormatFlag::SIR_FORMAT_SPACE ) ) {
		field.szPrefix[nPrefix++] = ' ';
	}
	if ( nLower == 'a' && std::isfinite( fMagnitude ) ) {
		field.szPrefix[nPrefix++] = '0';
		field.szPrefix[nPrefix] = bUpper ? 'X' : 'x';
	}
	if ( bUpper ) {
		for ( uint32_t i = 0; i < field.nSize; ++i ) {
			if ( szDigits[i] >= 'a' && szDigits[i] <= 'z' ) {
				szDigits[i] = static_cast<char>( szDigits[i] - 'a' + 'A' );
			}
		}
	}
	writeField( output, format, field );
	return true;
}

bool SIRAssertReport::formatField( sirDiagnosticBuffer_t& output, const sirFormat_t& format, va_list& args ) noexcept
{
	switch ( format.nConversion ) {
	case 'd':
	case 'i':
	case 'u':
	case 'o':
	case 'x':
	case 'X':
	case 'p':
		formatInteger( output, format, args );
		return true;
	case 'a':
	case 'A':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
		if ( hasFlag( format, sirFormatFlag::SIR_FORMAT_ALTERNATE ) ) {
			return false;
		}
		return formatFloat( output, format, args );
	case 'c':
	case 's':
		if ( format.eLength != sirLength::SIR_LENGTH_DEFAULT ) {
			return false;
		}
		break;
	default:
		return false;
	}
	sirField_t field{};
	sirFormat_t textFormat = format;
	textFormat.nFlags &= static_cast<uint8_t>( ~static_cast<uint8_t>( sirFormatFlag::SIR_FORMAT_ZERO ) );
	if ( format.nConversion == 'c' ) {
		const char nByte = static_cast<char>( va_arg( args, int32_t ) );
		field.pText = &nByte;
		field.nSize = 1;
		writeField( output, textFormat, field );
		return true;
	}
	field.pText = va_arg( args, const char * );
	if ( field.pText == nullptr ) {
		field.pText = "(null)";
	}
	const uint32_t nLimit = format.nPrecision < 0 ? output.nCapacity : static_cast<uint32_t>( format.nPrecision );
	while ( field.nSize < nLimit && field.pText[field.nSize] != '\0' ) {
		++field.nSize;
	}
	writeField( output, textFormat, field );
	return true;
}

void SIRAssertReport::formatMessage( sirEmergencyRecord_t& data, const char *pszFormat, va_list& args ) noexcept
{
	if ( pszFormat == nullptr ) {
		return;
	}
	// Use bounded conversions in place of CRT printf, which can allocate or
	// consult locale state. Unsupported directives end formatting before
	// reading their value, including positional, wide, long double and %n.
	sirDiagnosticBuffer_t output{ data.szMessage, sizeof( data.szMessage ), 0 };
	const char *pCursor = pszFormat;
	while ( *pCursor != '\0' && output.nSize < output.nCapacity - 1 ) {
		const char nByte = *pCursor++;
		if ( nByte != '%' ) {
			output.put( nByte );
		} else if ( *pCursor == '%' ) {
			output.put( *pCursor++ );
		} else if ( !formatField( output, parseFormat( pCursor, args ), args ) ) {
			output.append( "<unsupported format>" );
			break;
		}
	}
}

void SIRAssert::beginFailure( void ) noexcept
{
	SIREmergencyDiagnostics::beginTermination();
}

void SIRAssert::fail( const sirAssertContext_t& context, const char *pszMessage ) noexcept
{
	beginFailure();
	sirEmergencyRecord_t data = SIRAssertReport::makeRecord( context, sirEmergencySeverity::SIR_EMERGENCY_FATAL, SIR_ASSERT_RETURN_ADDRESS() );
	SIRAssertReport::setMessage( data, pszMessage );
	SIREmergencyDiagnostics::finishTermination( data );
}

void SIRAssert::failFormatted( const sirAssertContext_t& context, const char *pszFormat, ... ) noexcept
{
	sirEmergencyRecord_t data = SIRAssertReport::makeRecord( context, sirEmergencySeverity::SIR_EMERGENCY_FATAL, SIR_ASSERT_RETURN_ADDRESS() );
	va_list args;
	va_start( args, pszFormat );
	SIRAssertReport::formatMessage( data, pszFormat, args );
	va_end( args );
	SIREmergencyDiagnostics::finishTermination( data );
}

bool SIRAssert::reportEnsure( const sirAssertContext_t& context, const char *pszMessage ) noexcept
{
	sirEmergencyRecord_t data = SIRAssertReport::makeRecord( context, sirEmergencySeverity::SIR_EMERGENCY_ENSURE, SIR_ASSERT_RETURN_ADDRESS() );
	SIRAssertReport::setMessage( data, pszMessage );
	static_cast<void>( SIREmergencyDiagnostics::record( data ) );
	return false;
}

bool SIRAssert::reportEnsureFormatted( const sirAssertContext_t& context, const char *pszFormat, ... ) noexcept
{
	sirEmergencyRecord_t data = SIRAssertReport::makeRecord( context, sirEmergencySeverity::SIR_EMERGENCY_ENSURE, SIR_ASSERT_RETURN_ADDRESS() );
	va_list args;
	va_start( args, pszFormat );
	SIRAssertReport::formatMessage( data, pszFormat, args );
	va_end( args );
	static_cast<void>( SIREmergencyDiagnostics::record( data ) );
	return false;
}

bool SIRAssert::claimEnsure( std::atomic<uint32_t>& nClaim ) noexcept
{
	// The flag arbitrates this site and does not publish any data.
	uint32_t nExpected = 0;
	return nClaim.compare_exchange_strong( nExpected, 1, std::memory_order_relaxed );
}

#undef SIR_ASSERT_RETURN_ADDRESS
