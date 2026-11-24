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
#ifndef __SIR_D3_D9_SCOPEDOWNERSHIP_H__
#define __SIR_D3_D9_SCOPEDOWNERSHIP_H__
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>

// Owns only COM objects not yet published into a device-owned resource table.
// A registered resource is transferred with detach(); no heap allocation occurs.
template <typename T>
class SIRD3D9ScopedComRef
{
public:
	explicit SIRD3D9ScopedComRef( T *pObject = nullptr ) noexcept : m_pObject( pObject )
	{
	}
	~SIRD3D9ScopedComRef( void ) noexcept
	{
		reset();
	}
	SIRD3D9ScopedComRef( const SIRD3D9ScopedComRef& ) = delete;
	SIRD3D9ScopedComRef& operator=( const SIRD3D9ScopedComRef& ) = delete;

	T *get( void ) const noexcept
	{
		return m_pObject;
	}
	T **put( void ) noexcept
	{
		reset();
		return &m_pObject;
	}
	T *detach( void ) noexcept
	{
		T *pObject = m_pObject;
		m_pObject = nullptr;
		return pObject;
	}
	void reset( T *pObject = nullptr ) noexcept
	{
		if ( m_pObject != nullptr ) {
			m_pObject->Release();
		}
		m_pObject = pObject;
	}

private:
	T *m_pObject = nullptr;
};

class SIRD3D9ScopedModule
{
public:
	explicit SIRD3D9ScopedModule( HMODULE hModule = nullptr ) noexcept : m_hModule( hModule )
	{
	}
	~SIRD3D9ScopedModule( void ) noexcept
	{
		reset();
	}
	SIRD3D9ScopedModule( const SIRD3D9ScopedModule& ) = delete;
	SIRD3D9ScopedModule& operator=( const SIRD3D9ScopedModule& ) = delete;

	HMODULE get( void ) const noexcept
	{
		return m_hModule;
	}
	HMODULE detach( void ) noexcept
	{
		HMODULE hModule = m_hModule;
		m_hModule = nullptr;
		return hModule;
	}
	void reset( HMODULE hModule = nullptr ) noexcept
	{
		if ( m_hModule != nullptr ) {
			FreeLibrary( m_hModule );
		}
		m_hModule = hModule;
	}

private:
	HMODULE m_hModule = nullptr;
};
class SIRD3D9ScopedTemporaryAllocation
{
public:
	explicit SIRD3D9ScopedTemporaryAllocation( void *pData ) noexcept : m_pData( pData )
	{
	}
	~SIRD3D9ScopedTemporaryAllocation( void ) noexcept
	{
		free( m_pData );
	}
	SIRD3D9ScopedTemporaryAllocation( const SIRD3D9ScopedTemporaryAllocation& ) = delete;
	SIRD3D9ScopedTemporaryAllocation& operator=( const SIRD3D9ScopedTemporaryAllocation& ) = delete;
	void *get( void ) const noexcept
	{
		return m_pData;
	}

private:
	void *m_pData = nullptr;
};
#endif
#endif // __SIR_D3_D9_SCOPEDOWNERSHIP_H__
