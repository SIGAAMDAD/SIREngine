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

#ifndef __SIR_STRING_VIEW_H__
#define __SIR_STRING_VIEW_H__

#include "Core/Platform/SIRCompiler.h"
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>
#include <string.h>

class SIRStringView
{
public:
	constexpr explicit SIRStringView( void ) noexcept = default;
	constexpr explicit SIRStringView( const char *pszString, size_t nChars ) noexcept;
	constexpr explicit SIRStringView( const SIRStringView& other ) noexcept = default;
	~SIRStringView() noexcept = default;

	constexpr char at( size_t nOffset ) const noexcept;
	bool equals( const char *pszString ) const noexcept;
	bool equals( const SIRStringView& other ) const noexcept;
	bool contains( const char *pszString ) const noexcept;
	bool contains( const SIRStringView& other ) const noexcept;
	bool contains( char c ) const noexcept;

	constexpr size_t getSize( void ) const noexcept;
	constexpr bool isEmpty( void ) const noexcept;

	constexpr const char *getData( void ) const noexcept;
	constexpr const SIRSpan<const char>& getSpan( void ) const noexcept;

private:
	SIRSpan<const char> m_View;
};

SIR_FORCEINLINE constexpr SIRStringView::SIRStringView( const char *pszString, size_t nChars ) noexcept
	: m_View( pszString, nChars )
{
}

SIR_FORCEINLINE constexpr char SIRStringView::at( size_t nOffset ) const noexcept
{
	return m_View[nOffset];
}

SIR_FORCEINLINE bool SIRStringView::equals( const char *pszString ) const noexcept
{
	return strncmp( m_View.getData(), pszString, m_View.getSize() ) == 0;
}

SIR_FORCEINLINE bool SIRStringView::equals( const SIRStringView& other ) const noexcept
{
	return strncmp( m_View.getData(), other.getData(), m_View.getSize() < other.getSize() ? m_View.getSize() : other.getSize() );
}

#endif // __SIR_STRING_VIEW_H__
