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

#ifndef __SIR_D3_D11_COMOWNER_H__
#define __SIR_D3_D11_COMOWNER_H__

// Stack-only ownership for unpublished COM objects and temporary DXGI refs.
// Publication transfers the original reference into the fixed device slots.
template <typename T>
class SIRD3D11ComOwner final
{
public:
	explicit SIRD3D11ComOwner( T *pObject = nullptr ) noexcept : m_pObject( pObject )
	{
	}
	~SIRD3D11ComOwner( void ) noexcept
	{
		reset();
	}

	SIRD3D11ComOwner( const SIRD3D11ComOwner& ) = delete;
	SIRD3D11ComOwner& operator=( const SIRD3D11ComOwner& ) = delete;
	SIRD3D11ComOwner( SIRD3D11ComOwner&& ) = delete;
	SIRD3D11ComOwner& operator=( SIRD3D11ComOwner&& ) = delete;

	T *get( void ) const noexcept
	{
		return m_pObject;
	}
	T *operator->( void ) const noexcept
	{
		return m_pObject;
	}
	T **put( void ) noexcept
	{
		reset();
		return &m_pObject;
	}
	T *release( void ) noexcept
	{
		T *pResult = m_pObject;
		m_pObject = nullptr;
		return pResult;
	}
	void reset( T *pObject = nullptr ) noexcept
	{
		if ( pObject == m_pObject ) {
			return;
		}
		if ( m_pObject != nullptr ) {
			m_pObject->Release();
		}
		m_pObject = pObject;
	}

protected:
private:
	T *m_pObject = nullptr;
};

#endif // __SIR_D3_D11_COMOWNER_H__
