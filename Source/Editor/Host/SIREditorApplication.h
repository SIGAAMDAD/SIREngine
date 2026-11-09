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
#include <Core/SIRApplication.h>

typedef enum sirEditorApplicationResult_e : uint8_t
{
	SIR_EDITOR_APP_OK = 0,
	SIR_EDITOR_APP_INVALID_ARGUMENT = 1,
	SIR_EDITOR_APP_NOT_INITIALIZED = 2,
	SIR_EDITOR_APP_ALREADY_INITIALIZED = 3,
	SIR_EDITOR_APP_CALLBACK_FAILED = 4,
	SIR_EDITOR_APP_FRAME_EXHAUSTED = 5,
	SIR_EDITOR_APP_HOST_FAILED = 6
} sirEditorApplicationResult_t;

typedef bool ( *sirEditorFrameCallback_t )( uint64_t nFrame, void *pContext ) noexcept;

// The editor is a system registered with the canonical SIRApplication.
// Window/input dispatch and core system ordering remain engine-owned.
class SIREditorApplication
{
public:
	SIREditorApplication( void ) noexcept;
	SIREditorApplication( const SIREditorApplication& ) = delete;
	~SIREditorApplication( void ) noexcept;

	SIREditorApplication& operator=( const SIREditorApplication& ) = delete;

	uint64_t getFrameCount( void ) const noexcept;
	bool isRunning( void ) const noexcept;
	sirAppState_t getCoreState( void ) const noexcept;

	sirEditorApplicationResult_t initialize( sirEditorFrameCallback_t pfnFrame, void *pContext ) noexcept;
	sirEditorApplicationResult_t tick( void ) noexcept;
	sirEditorApplicationResult_t run( uint64_t nMaxFrames ) noexcept;
	void requestQuit( void ) noexcept;
	void shutdown( void ) noexcept;

protected:
private:
	class FrameSystem final : public SIREngineSystem
	{
	public:
		explicit FrameSystem( SIREditorApplication& owner ) noexcept;

		sirSystemResult_t initialize( void ) noexcept override;
		void update( float fDeltaSeconds ) noexcept override;
		sirSystemResult_t shutdown( void ) noexcept override;

	protected:
	private:
		SIREditorApplication& m_Owner;
	};

	void updateFrame( void ) noexcept;

	SIRApplication m_Application;
	FrameSystem m_FrameSystem;
	sirEditorFrameCallback_t m_pfnFrame;
	void *m_pContext;
	bool m_bRunning;
	bool m_bFrameFailed;
};
#endif // __SIRENGINE_SIREDITORAPPLICATION_H__
