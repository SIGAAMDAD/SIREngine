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

SIREditorApplication::SIREditorApplication( void ) noexcept
    : m_pfnFrame( nullptr ), m_pContext( nullptr ), m_nFrameCount( 0 ), m_bInitialized( false ), m_bRunning( false )
{
}

uint64_t SIREditorApplication::getFrameCount( void ) const noexcept
{
	return m_nFrameCount;
}
bool SIREditorApplication::isRunning( void ) const noexcept
{
	return m_bRunning;
}

sirEditorApplicationResult_t SIREditorApplication::initialize( sirEditorFrameCallback_t pfnFrame, void *pContext ) noexcept
{
	if ( m_bInitialized ) {
		return SIR_EDITOR_APP_ALREADY_INITIALIZED;
	}
	if ( pfnFrame == nullptr ) {
		return SIR_EDITOR_APP_INVALID_ARGUMENT;
	}
	m_pfnFrame = pfnFrame;
	m_pContext = pContext;
	m_nFrameCount = 0;
	m_bInitialized = true;
	m_bRunning = true;
	return SIR_EDITOR_APP_OK;
}

sirEditorApplicationResult_t SIREditorApplication::tick( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_APP_NOT_INITIALIZED;
	}
	if ( !m_bRunning ) {
		return SIR_EDITOR_APP_OK;
	}
	if ( m_nFrameCount == UINT64_MAX ) {
		m_bRunning = false;
		return SIR_EDITOR_APP_FRAME_EXHAUSTED;
	}
	const uint64_t nFrame = m_nFrameCount;
	// Increment before invoking the host: callbacks may request a synchronous quit.
	++m_nFrameCount;
	if ( !m_pfnFrame( nFrame, m_pContext ) ) {
		m_bRunning = false;
		return SIR_EDITOR_APP_CALLBACK_FAILED;
	}
	return SIR_EDITOR_APP_OK;
}

sirEditorApplicationResult_t SIREditorApplication::run( uint64_t nMaxFrames ) noexcept
{
	if ( !m_bInitialized ) {
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
	m_bInitialized = false;
	m_pfnFrame = nullptr;
	m_pContext = nullptr;
	m_nFrameCount = 0;
}
