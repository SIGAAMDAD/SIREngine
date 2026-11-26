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

#ifndef __SIR_STRINGBUILDER_H__
#define __SIR_STRINGBUILDER_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Caller-owned scratch buffer. Single writer. No allocations or implicit truncation.
class SIRStringBuilder final
{
public:
	explicit SIRStringBuilder( char *pszStorage, uint32_t nCapacity ) noexcept;
	explicit SIRStringBuilder( const SIRStringBuilder& ) = delete;
	explicit SIRStringBuilder( SIRStringBuilder&& ) = delete;
	~SIRStringBuilder() noexcept = default;
	SIRStringBuilder& operator=( const SIRStringBuilder& ) = delete;
	SIRStringBuilder& operator=( SIRStringBuilder&& ) = delete;

	[[nodiscard]] sirStringView_t getView( void ) const noexcept;
	[[nodiscard]] uint32_t getRemainingCapacity( void ) const noexcept;
	[[nodiscard]] SIRContainerResult tryAppend( sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppendUnsigned( uint64_t nValue ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppendBatch( const sirStringView_t *pViews, uint32_t nCount ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppendGenerated( uint32_t nBytes, void ( *pfnWrite )( char *, uint32_t, void * ) noexcept, void *pContext ) noexcept;
	void clear( void ) noexcept;

private:
	char *m_pszStorage;
	uint32_t m_nCapacity;
	uint32_t m_nLength{};
};

#endif // __SIR_STRINGBUILDER_H__
