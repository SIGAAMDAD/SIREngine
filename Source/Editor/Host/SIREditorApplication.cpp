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
#include <Editor/Host/SIREditorApplication.h>
#include <new>

SIREditorApplication::FrameSystem::FrameSystem( SIREditorApplication& owner ) noexcept : m_Owner( owner )
{
}

sirSystemResult_t SIREditorApplication::FrameSystem::initialize( void ) noexcept
{
	return SIR_SYSTEM_OK;
}

void SIREditorApplication::FrameSystem::update( float fDeltaSeconds ) noexcept
{
	( void )fDeltaSeconds;
	m_Owner.updateFrame();
}

sirSystemResult_t SIREditorApplication::FrameSystem::shutdown( void ) noexcept
{
	return SIR_SYSTEM_OK;
}

SIREditorApplication::SIREditorApplication( void ) noexcept
    : m_Application(), m_FrameSystem( *this ), m_pfnFrame( nullptr ), m_pContext( nullptr ), m_bRunning( false ), m_bFrameFailed( false )
{
}

SIREditorApplication::~SIREditorApplication( void ) noexcept
{
	shutdown();
}

uint64_t SIREditorApplication::getFrameCount( void ) const noexcept
{
	return m_Application.getTickCount();
}

bool SIREditorApplication::isRunning( void ) const noexcept
{
	return m_bRunning && m_Application.getState() == SIR_APP_READY;
}

sirAppState_t SIREditorApplication::getCoreState( void ) const noexcept
{
	return m_Application.getState();
}

sirEditorApplicationResult_t SIREditorApplication::initialize( sirEditorFrameCallback_t pfnFrame, void *pContext ) noexcept
{
	if ( m_Application.getState() != SIR_APP_UNINITIALIZED && m_Application.getState() != SIR_APP_STOPPED ) {
		return SIR_EDITOR_APP_ALREADY_INITIALIZED;
	}
	if ( pfnFrame == nullptr ) {
		return SIR_EDITOR_APP_INVALID_ARGUMENT;
	}
	if ( m_Application.getState() == SIR_APP_STOPPED ) {
		// A complete shutdown releases every borrowed system before rebuilding
		// the core lifecycle in the same storage, without an extra heap owner.
		m_Application.~SIRApplication();
		new ( &m_Application ) SIRApplication();
	}

	// The core application owns system registration, initialization and tick count.
	if ( !m_Application.registerSystem( &m_FrameSystem ) || !m_Application.initialize() ) {
		return SIR_EDITOR_APP_HOST_FAILED;
	}
	m_pfnFrame = pfnFrame;
	m_pContext = pContext;
	m_bFrameFailed = false;
	m_bRunning = true;
	return SIR_EDITOR_APP_OK;
}

void SIREditorApplication::updateFrame( void ) noexcept
{
	if ( !m_bRunning || m_pfnFrame == nullptr ) {
		return;
	}
	// The core increments its tick count after systems finish this frame.
	if ( !m_pfnFrame( m_Application.getTickCount(), m_pContext ) ) {
		m_bFrameFailed = true;
		m_bRunning = false;
	}
}

sirEditorApplicationResult_t SIREditorApplication::tick( void ) noexcept
{
	if ( m_Application.getState() != SIR_APP_READY ) {
		return SIR_EDITOR_APP_NOT_INITIALIZED;
	}
	if ( !m_bRunning ) {
		return SIR_EDITOR_APP_OK;
	}
	if ( m_Application.getTickCount() == UINT64_MAX ) {
		m_bRunning = false;
		return SIR_EDITOR_APP_FRAME_EXHAUSTED;
	}
	m_bFrameFailed = false;
	if ( !m_Application.tick() ) {
		m_bRunning = false;
		return SIR_EDITOR_APP_HOST_FAILED;
	}
	return m_bFrameFailed ? SIR_EDITOR_APP_CALLBACK_FAILED : SIR_EDITOR_APP_OK;
}

sirEditorApplicationResult_t SIREditorApplication::run( uint64_t nMaxFrames ) noexcept
{
	if ( m_Application.getState() != SIR_APP_READY ) {
		return SIR_EDITOR_APP_NOT_INITIALIZED;
	}
	if ( nMaxFrames == 0 ) {
		return SIR_EDITOR_APP_INVALID_ARGUMENT;
	}
	for ( uint64_t iFrame = 0; iFrame < nMaxFrames && m_bRunning; ++iFrame ) {
		const sirEditorApplicationResult_t eResult = tick();
		if ( eResult != SIR_EDITOR_APP_OK ) {
			return eResult;
		}
	}
	return SIR_EDITOR_APP_OK;
}

void SIREditorApplication::requestQuit( void ) noexcept
{
	m_bRunning = false;
}

void SIREditorApplication::shutdown( void ) noexcept
{
	m_bRunning = false;
	( void )m_Application.shutdown();
	m_pfnFrame = nullptr;
	m_pContext = nullptr;
	m_bFrameFailed = false;
}
