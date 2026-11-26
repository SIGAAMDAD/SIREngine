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

#ifndef __SIR_WEAKRESOURCEPTR_H__
#define __SIR_WEAKRESOURCEPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRResourcePtr.h"

// Generation-checked registry reference; does not increase the strong count.
// The referenced graph itself must outlive this observer.
template <typename T>
class SIRWeakResourcePtr
{
public:
	SIRWeakResourcePtr( void ) noexcept;
	explicit SIRWeakResourcePtr( const SIRResourcePtr<T>& rStrong ) noexcept;
	~SIRWeakResourcePtr( void ) noexcept = default;

	bool isExpired( void ) const noexcept;
	SIRResourceHandle getHandle( void ) const noexcept;
	SIRResourceResult lock( SIRResourcePtr<T> *pOut ) const noexcept;
	void reset( void ) noexcept;

private:
	SIRResourceGraph *m_pGraph;
	SIRResourceHandle m_hResource;
};

template <typename T>
SIRWeakResourcePtr<T>::SIRWeakResourcePtr( void ) noexcept : m_pGraph( nullptr ), m_hResource{}
{
}

template <typename T>
SIRWeakResourcePtr<T>::SIRWeakResourcePtr( const SIRResourcePtr<T>& rStrong ) noexcept
    : m_pGraph( rStrong.getGraph() ), m_hResource( rStrong.getHandle() )
{
}

template <typename T>
bool SIRWeakResourcePtr<T>::isExpired( void ) const noexcept
{
	return m_pGraph == nullptr || !SIRResourceIsReferenceAlive( *m_pGraph, m_hResource );
}

template <typename T>
SIRResourceHandle SIRWeakResourcePtr<T>::getHandle( void ) const noexcept
{
	return m_hResource;
}

template <typename T>
SIRResourceResult SIRWeakResourcePtr<T>::lock( SIRResourcePtr<T> *pOut ) const noexcept
{
	if ( m_pGraph == nullptr ) {
		return SIRResourceResult::SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	return SIRResourcePtr<T>::acquire( *m_pGraph, m_hResource, pOut );
}

template <typename T>
void SIRWeakResourcePtr<T>::reset( void ) noexcept
{
	m_pGraph = nullptr;
	m_hResource = {};
}

#endif // __SIR_WEAKRESOURCEPTR_H__
