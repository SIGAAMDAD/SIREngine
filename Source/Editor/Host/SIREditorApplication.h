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
#ifndef __SIRENGINE_SIREDITORAPPLICATION_H__
#define __SIRENGINE_SIREDITORAPPLICATION_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

typedef enum sirEditorApplicationResult_e : uint8_t
{
	SIR_EDITOR_APP_OK = 0,
	SIR_EDITOR_APP_INVALID_ARGUMENT = 1,
	SIR_EDITOR_APP_NOT_INITIALIZED = 2,
	SIR_EDITOR_APP_ALREADY_INITIALIZED = 3,
	SIR_EDITOR_APP_CALLBACK_FAILED = 4,
	SIR_EDITOR_APP_FRAME_EXHAUSTED = 5
} sirEditorApplicationResult_t;

typedef bool ( *sirEditorFrameCallback_t )( uint64_t nFrame, void *pContext ) noexcept;

// Main-thread, frame-bounded editor application pump. Window/input/render backends
// can supply one callback later without changing lifecycle or frame ownership.
class SIREditorApplication
{
public:
	SIREditorApplication( void ) noexcept;

	uint64_t getFrameCount( void ) const noexcept;
	bool isRunning( void ) const noexcept;

	sirEditorApplicationResult_t initialize( sirEditorFrameCallback_t pfnFrame, void *pContext ) noexcept;
	sirEditorApplicationResult_t tick( void ) noexcept;
	sirEditorApplicationResult_t run( uint64_t nMaxFrames ) noexcept;
	void requestQuit( void ) noexcept;
	void shutdown( void ) noexcept;

protected:
private:
	sirEditorFrameCallback_t m_pfnFrame;
	void *m_pContext;
	uint64_t m_nFrameCount;
	bool m_bInitialized;
	bool m_bRunning;
};
#endif // __SIRENGINE_SIREDITORAPPLICATION_H__
