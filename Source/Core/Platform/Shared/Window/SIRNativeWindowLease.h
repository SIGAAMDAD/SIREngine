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

#ifndef __SIR_NATIVEWINDOWLEASE_H__
#define __SIR_NATIVEWINDOWLEASE_H__
#include <Core/SIRPCH.h>
#include <Core/Platform/Shared/Window/SIRWindowTypes.h>
class SIRWindowSystem;

// The system must outlive the lease. Retire GPU work before resetting it.
class SIRNativeWindowLease final
{
public:
	explicit SIRNativeWindowLease( void ) noexcept;
	explicit SIRNativeWindowLease( const SIRNativeWindowLease& ) = delete;
	explicit SIRNativeWindowLease( SIRNativeWindowLease&& other ) noexcept;
	~SIRNativeWindowLease() noexcept;

	SIRNativeWindowLease& operator=( const SIRNativeWindowLease& ) = delete;
	SIRNativeWindowLease& operator=( SIRNativeWindowLease&& ) = delete;
	[[nodiscard]] bool isValid( void ) const noexcept;
	[[nodiscard]] const sirWindowNativeView_t& getView( void ) const noexcept;
	[[nodiscard]] sirWindowResult reset( void ) noexcept;

private:
	friend class SIRWindowSystem;
	SIRWindowSystem *m_pOwner;
	sirWindowNativeLease_t m_Lease;
	sirWindowNativeView_t m_View;
};
#endif // __SIR_NATIVEWINDOWLEASE_H__
