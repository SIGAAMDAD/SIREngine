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

#ifndef __SIR_RESOURCEREADPTR_H__
#define __SIR_RESOURCEREADPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceGraph.h>

// Borrowed, typed view of a single GRDG published revision. The read scope
// must outlive this wrapper and every typed view returned from get().
template <typename T>
class SIRResourceReadPtr
{
public:
	SIRResourceReadPtr( void ) noexcept = default;
	bool isValid( void ) const noexcept
	{
		return m_View.isValid() && m_View.getKind() == T::RESOURCE_KIND;
	}
	SIRResourceHandle getHandle( void ) const noexcept
	{
		return m_View.getHandle();
	}
	uint64_t getRevision( void ) const noexcept
	{
		return isValid() ? m_View.getRevision() : 0;
	}
	SIRResourceResult get( T *pOut ) const noexcept
	{
		return tryCastResourceView( m_View, pOut );
	}
	void reset( void ) noexcept
	{
		m_View = SIRResourceView();
	}
	SIRResourceResult acquire( const SIRResourceGraph& rGraph, const SIRResourceReadScope& rScope, SIRResourceHandle hResource ) noexcept
	{
		reset();
		SIRResourceView view;
		const SIRResourceResult eResult = rGraph.resolve( rScope, hResource, &view );
		if ( eResult != SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS ) {
			return eResult;
		}
		if ( view.getKind() != T::RESOURCE_KIND ) {
			return SIRResourceResult::SIR_RESOURCE_RESULT_TYPE_MISMATCH;
		}
		m_View = view;
		return SIRResourceResult::SIR_RESOURCE_RESULT_SUCCESS;
	}

private:
	SIRResourceView m_View;
};

#endif // __SIR_RESOURCEREADPTR_H__
