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

#ifndef __SIR_STRINGFORMATPROGRAM_H__
#define __SIR_STRINGFORMATPROGRAM_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>
#include <Core/Strings/SIRStringBuilder.h>
#include <charconv>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class sirFormatOperandKind : uint8_t
{
	SIR_FORMAT_VIEW,
	SIR_FORMAT_UNSIGNED,
	SIR_FORMAT_SIGNED
};

typedef struct sirFormatOperand_s
{
	sirFormatOperandKind eKind;
	sirStringView_t view;
	uint64_t nUnsigned;
	int64_t nSigned;
} sirFormatOperand_t;

// A constexpr constructor pre-parses literal/argument boundaries. The program
// owns the format bytes. Runtime rendering emits one batch with one terminator.
template <uint32_t nFormatBytes>
class SIRStringFormatProgram final
{
public:
	explicit constexpr SIRStringFormatProgram( const char ( &szFormat )[nFormatBytes] ) noexcept
	    : m_bValid( true )
	{
		for ( uint32_t i = 0; i < nFormatBytes; ++i ) {
			m_szFormat[i] = szFormat[i];
		}
		for ( uint32_t i = 0; i + 1 < nFormatBytes; ++i ) {
			if ( m_szFormat[i] == '{' && m_szFormat[i + 1] == '}' ) {
				m_iFields[m_nFields++] = i;
				++i;
			} else if ( m_szFormat[i] == '{' || m_szFormat[i] == '}' ) {
				m_bValid = false;
			}
		}
		if ( m_szFormat[nFormatBytes - 1] != '\0' ) {
			m_bValid = false;
		}
	}

	explicit SIRStringFormatProgram( const SIRStringFormatProgram& ) noexcept = default;
	explicit SIRStringFormatProgram( SIRStringFormatProgram&& ) noexcept = default;
	~SIRStringFormatProgram() noexcept = default;
	SIRStringFormatProgram& operator=( const SIRStringFormatProgram& ) noexcept = default;
	SIRStringFormatProgram& operator=( SIRStringFormatProgram&& ) noexcept = default;

	[[nodiscard]] constexpr uint32_t getFieldCount( void ) const noexcept
	{
		return m_nFields;
	}

	[[nodiscard]] constexpr bool isValid( void ) const noexcept
	{
		return m_bValid;
	}

	[[nodiscard]] SIRContainerResult tryRender( SIRStringBuilder& builder, const sirStringView_t *pFields, uint32_t nFields ) const noexcept;

	[[nodiscard]] SIRContainerResult tryRenderValues( SIRStringBuilder& builder, const sirFormatOperand_t *pFields, uint32_t nFields ) const noexcept;

private:
	static_assert( nFormatBytes > 0, "format requires terminator" );
	char m_szFormat[nFormatBytes]{};
	uint32_t m_iFields[nFormatBytes / 2 + 1]{};
	uint32_t m_nFields{};
	bool m_bValid : 1;
};

template <uint32_t nFormatBytes>
SIRContainerResult SIRStringFormatProgram<nFormatBytes>::tryRender( SIRStringBuilder& builder, const sirStringView_t *pFields, uint32_t nFields ) const noexcept
{
	if ( !isValid() || nFields != m_nFields || ( nFields != 0 && pFields == nullptr ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	sirStringView_t fragments[nFormatBytes + 1]{};
	uint32_t iStart = 0;
	for ( uint32_t i = 0; i < m_nFields; ++i ) {
		fragments[i * 2] = { m_szFormat + iStart, m_iFields[i] - iStart };
		fragments[i * 2 + 1] = pFields[i];
		iStart = m_iFields[i] + 2;
	}
	fragments[m_nFields * 2] = { m_szFormat + iStart, nFormatBytes - 1 - iStart };
	return builder.tryAppendBatch( fragments, m_nFields * 2 + 1 );
}

template <uint32_t nFormatBytes>
SIRContainerResult
SIRStringFormatProgram<nFormatBytes>::tryRenderValues( SIRStringBuilder& builder, const sirFormatOperand_t *pFields, uint32_t nFields ) const noexcept
{
	if ( !isValid() || nFields != m_nFields || ( nFields != 0 && pFields == nullptr ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	char szNumeric[nFormatBytes / 2 + 1][21];
	sirStringView_t views[nFormatBytes / 2 + 1];
	for ( uint32_t i = 0; i < nFields; ++i ) {
		const sirFormatOperand_t& field = pFields[i];
		if ( field.eKind == sirFormatOperandKind::SIR_FORMAT_VIEW ) {
			views[i] = field.view;
			continue;
		}
		std::to_chars_result result{};
		if ( field.eKind == sirFormatOperandKind::SIR_FORMAT_UNSIGNED ) {
			result = std::to_chars( szNumeric[i], szNumeric[i] + 21, field.nUnsigned );
		} else if ( field.eKind == sirFormatOperandKind::SIR_FORMAT_SIGNED ) {
			result = std::to_chars( szNumeric[i], szNumeric[i] + 21, field.nSigned );
		} else {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( result.ec != std::errc{} ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		views[i] = { szNumeric[i], static_cast<uint32_t>( result.ptr - szNumeric[i] ) };
	}
	return tryRender( builder, views, nFields );
}

#endif // __SIR_STRINGFORMATPROGRAM_H__
