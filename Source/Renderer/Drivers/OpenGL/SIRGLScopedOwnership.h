/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRGLSCOPEDOWNERSHIP_H__
#define __SIRENGINE_SIRGLSCOPEDOWNERSHIP_H__
#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/Drivers/OpenGL/SIRGLContext.h>

// Non-owning until adopt(). Failure paths return native names to their original GL owner.
// The wrapper is stack-only; its size and allocation cost are independent of resource count.
template <typename TDispatch, sirGraphicsResult_t ( *DESTROY )( const TDispatch *, uint32_t ) noexcept>
class SIRGLScopedName final
{
public:
	explicit SIRGLScopedName( const TDispatch *pDispatch ) noexcept : m_pDispatch( pDispatch )
	{
	}
	~SIRGLScopedName( void ) noexcept
	{
		if ( m_nName != 0 ) {
			( void )DESTROY( m_pDispatch, m_nName );
		}
	}
	SIRGLScopedName( const SIRGLScopedName& ) = delete;
	SIRGLScopedName& operator=( const SIRGLScopedName& ) = delete;
	SIRGLScopedName( SIRGLScopedName&& ) = delete;
	SIRGLScopedName& operator=( SIRGLScopedName&& ) = delete;
	void adopt( uint32_t nName ) noexcept
	{
		m_nName = nName;
	}
	uint32_t release( void ) noexcept
	{
		const uint32_t nName = m_nName;
		m_nName = 0;
		return nName;
	}

private:
	const TDispatch *m_pDispatch;
	uint32_t m_nName = 0;
};

class SIRGLScopedContext final
{
public:
	explicit SIRGLScopedContext( sirGLNativeContext_t *pContext ) noexcept : m_pContext( pContext )
	{
	}
	~SIRGLScopedContext( void ) noexcept
	{
		sirGLDestroyNativeContext( m_pContext );
	}
	SIRGLScopedContext( const SIRGLScopedContext& ) = delete;
	SIRGLScopedContext& operator=( const SIRGLScopedContext& ) = delete;
	sirGLNativeContext_t *release( void ) noexcept
	{
		sirGLNativeContext_t *pContext = m_pContext;
		m_pContext = nullptr;
		return pContext;
	}

private:
	sirGLNativeContext_t *m_pContext;
};

template <typename TObject>
class SIRGLScopedHostObject final
{
public:
	SIRGLScopedHostObject( const sirGraphicsABIHost_t *pHost, TObject *pObject ) noexcept : m_pHost( pHost ), m_pObject( pObject )
	{
	}
	~SIRGLScopedHostObject( void ) noexcept
	{
		if ( m_pObject != nullptr ) {
			m_pObject->~TObject();
			m_pHost->pFree( m_pHost->pContext, m_pObject );
		}
	}
	SIRGLScopedHostObject( const SIRGLScopedHostObject& ) = delete;
	SIRGLScopedHostObject& operator=( const SIRGLScopedHostObject& ) = delete;
	TObject *release( void ) noexcept
	{
		TObject *pObject = m_pObject;
		m_pObject = nullptr;
		return pObject;
	}

private:
	const sirGraphicsABIHost_t *m_pHost;
	TObject *m_pObject;
};
#endif
