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
#ifndef __SIRENGINE_SIRWINDOWABISURFACE_H__
#define __SIRENGINE_SIRWINDOWABISURFACE_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Renderer/SIRGraphicsModule.h>
#include <Renderer/SIRWindowSurfaceAdapter.h>

// Renderer owns the GPU surface; Core owns the native window. A successful
// create holds a Core lease until the backend has actually retired the GPU
// surface. Never destroy the window or graphics device before retire succeeds.
class SIRWindowABISurface final
{
public:
	SIRWindowABISurface( void ) noexcept = default;
	~SIRWindowABISurface( void ) noexcept = default;

	SIRWindowABISurface( const SIRWindowABISurface& ) = delete;
	SIRWindowABISurface& operator=( const SIRWindowABISurface& ) = delete;

	bool isActive( void ) const noexcept;
	sirGraphicsHandle_t getSurface( void ) const noexcept;
	uint64_t getMetricsRevision( void ) const noexcept;

	sirGraphicsABIResult_t create( SIRWindowSystem& windows, sirWindowHandle_t hWindow, const sirGraphicsABIDevice_t *pDevice,
	                               const sirGraphicsABIPresentDesc_t *pPresent ) noexcept;
	sirGraphicsABIResult_t resize( const sirGraphicsABIPresentDesc_t *pPresent ) noexcept;
	sirGraphicsABIResult_t acquireImage( sirGraphicsSurfaceImage_t *pImage ) noexcept;
	sirGraphicsABIResult_t presentImage( const sirGraphicsCompletion_t *pCompletion ) noexcept;
	sirGraphicsABIResult_t retire( const sirGraphicsCompletionSet_t *pComplete ) noexcept;

protected:
private:
	sirGraphicsABINativeSurface_t nativeSurface( void ) const noexcept;
	SIRWindowSystem *m_pWindows = nullptr;
	const sirGraphicsABIDevice_t *m_pDevice = nullptr;
	sirWindowHandle_t m_hWindow = {};
	sirWindowNativeLease_t m_hLease = {};
	sirGraphicsNativeSurface_t m_Native = {};
	sirGraphicsHandle_t m_hSurface = {};
	uint64_t m_nMetricsRevision = 0;
	sirGraphicsABIPresentDesc_t m_Present = {};
	sirGraphicsSurfaceImage_t m_Acquired = {};
	bool m_bImageAcquired = false;
};
#endif // __SIRENGINE_SIRWINDOWABISURFACE_H__
