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

#ifndef SIRENGINE_SIRRESOURCEPTR_H
#define SIRENGINE_SIRRESOURCEPTR_H

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRResourceReference.h"
#include <stdlib.h>

// Strong lifetime claim on a canonical GRDG slot, not on a payload allocation.
// T is an existing typed SIRResourceView subclass with T::RESOURCE_KIND.
// The graph must outlive every pointer and weak reference that names it.
template <typename T>
class SIRResourcePtr
{
public:
	SIRResourcePtr( void ) noexcept;
	~SIRResourcePtr( void ) noexcept;
	SIRResourcePtr( const SIRResourcePtr& rOther ) noexcept;
	SIRResourcePtr( SIRResourcePtr&& rOther ) noexcept;
	SIRResourcePtr& operator=( const SIRResourcePtr& rOther ) noexcept;
	SIRResourcePtr& operator=( SIRResourcePtr&& rOther ) noexcept;

	bool isValid( void ) const noexcept;
	SIRResourceHandle getHandle( void ) const noexcept;
	SIRResourceGraph *getGraph( void ) const noexcept;
	SIRResourceResult resolve( const SIRResourceReadScope& rScope, SIRResourceView *pView ) const noexcept;
	void reset( void ) noexcept;
	void swap( SIRResourcePtr& rOther ) noexcept;

	static SIRResourceResult acquire( SIRResourceGraph& rGraph, SIRResourceHandle hResource, SIRResourcePtr *pOut ) noexcept;

private:
	SIRResourceGraph *m_pGraph;
	SIRResourceHandle m_hResource;
};

template <typename T>
SIRResourcePtr<T>::SIRResourcePtr( void ) noexcept : m_pGraph( nullptr ), m_hResource{}
{
}

template <typename T>
SIRResourcePtr<T>::~SIRResourcePtr( void ) noexcept
{
	reset();
}

template <typename T>
SIRResourcePtr<T>::SIRResourcePtr( const SIRResourcePtr& rOther ) noexcept : m_pGraph( rOther.m_pGraph ), m_hResource( rOther.m_hResource )
{
	if ( m_pGraph != nullptr && SIRResourceAcquireReference( *m_pGraph, m_hResource, T::RESOURCE_KIND ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		// Copying an existing strong claim must never fail while graph lifetime is valid.
		abort();
	}
}

template <typename T>
SIRResourcePtr<T>::SIRResourcePtr( SIRResourcePtr&& rOther ) noexcept : m_pGraph( rOther.m_pGraph ), m_hResource( rOther.m_hResource )
{
	rOther.m_pGraph = nullptr;
	rOther.m_hResource = {};
}

template <typename T>
SIRResourcePtr<T>& SIRResourcePtr<T>::operator=( const SIRResourcePtr& rOther ) noexcept
{
	if ( this == &rOther ) {
		return *this;
	}
	SIRResourcePtr rCopy( rOther );
	swap( rCopy );
	return *this;
}

template <typename T>
SIRResourcePtr<T>& SIRResourcePtr<T>::operator=( SIRResourcePtr&& rOther ) noexcept
{
	if ( this == &rOther ) {
		return *this;
	}
	reset();
	m_pGraph = rOther.m_pGraph;
	m_hResource = rOther.m_hResource;
	rOther.m_pGraph = nullptr;
	rOther.m_hResource = {};
	return *this;
}

template <typename T>
bool SIRResourcePtr<T>::isValid( void ) const noexcept
{
	return m_pGraph != nullptr;
}

template <typename T>
SIRResourceHandle SIRResourcePtr<T>::getHandle( void ) const noexcept
{
	return m_hResource;
}

template <typename T>
SIRResourceGraph *SIRResourcePtr<T>::getGraph( void ) const noexcept
{
	return m_pGraph;
}

template <typename T>
SIRResourceResult SIRResourcePtr<T>::resolve( const SIRResourceReadScope& rScope, SIRResourceView *pView ) const noexcept
{
	if ( m_pGraph == nullptr || pView == nullptr ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRResourceView base;
	const SIRResourceResult eResult = m_pGraph->resolve( rScope, m_hResource, &base );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	if ( base.getKind() != T::RESOURCE_KIND ) {
		return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}
	*pView = base;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

template <typename T>
void SIRResourcePtr<T>::reset( void ) noexcept
{
	if ( m_pGraph != nullptr ) {
		SIRResourceReleaseReference( *m_pGraph, m_hResource );
	}
	m_pGraph = nullptr;
	m_hResource = {};
}

template <typename T>
void SIRResourcePtr<T>::swap( SIRResourcePtr& rOther ) noexcept
{
	SIRResourceGraph *pGraph = m_pGraph;
	SIRResourceHandle hResource = m_hResource;
	m_pGraph = rOther.m_pGraph;
	m_hResource = rOther.m_hResource;
	rOther.m_pGraph = pGraph;
	rOther.m_hResource = hResource;
}

template <typename T>
SIRResourceResult SIRResourcePtr<T>::acquire( SIRResourceGraph& rGraph, SIRResourceHandle hResource, SIRResourcePtr *pOut ) noexcept
{
	if ( pOut == nullptr ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const SIRResourceResult eResult = SIRResourceAcquireReference( rGraph, hResource, T::RESOURCE_KIND );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	SIRResourcePtr owned;
	owned.m_pGraph = &rGraph;
	owned.m_hResource = hResource;
	pOut->swap( owned );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

#endif // SIRENGINE_SIRRESOURCEPTR_H
