/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRWINDOWSERVICE_H__
#define __SIRENGINE_SIRWINDOWSERVICE_H__

#include <Core/Platform/Window/SIRWindowSystem.h>
#include <Core/Systems/SIREngineSystem.h>

// A Core lifecycle provider: render/input/editor consumers depend on this
// system instead of creating or destroying native windows independently.
class SIRWindowService final : public SIREngineSystem
{
public:
	SIRWindowService( SIRWindowSystem& windows, SIRWindowDriver& driver, const sirWindowDesc_t& desc,
	                  SIRInputService *pInput = nullptr ) noexcept;
	bool setDesktopBackend( SIRInputBackend& backend ) noexcept;
	sirSystemResult_t initialize( void ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;
	sirWindowHandle_t primaryWindow( void ) const noexcept;
	sirWindowResult_t pump( void ) noexcept;
	bool hasLiveWindows( void ) const noexcept;

private:
	SIRWindowSystem& m_Windows;
	SIRWindowDriver& m_Driver;
	sirWindowDesc_t m_Desc;
	SIRInputService *m_pInput;
	SIRInputBackend *m_pDesktop;
	sirWindowHandle_t m_hPrimary;
	bool m_bStarted;
};

#endif // __SIRENGINE_SIRWINDOWSERVICE_H__
