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

#include <Scripting/Core/SIRScriptServer.h>

SIRScriptServer::SIRScriptServer( void ) noexcept : m_Bindings(), m_CodeManager( &m_Bindings ), m_Runtime( &m_Bindings )
{
}

SIRScriptServer::~SIRScriptServer( void ) noexcept
{
	// Finish backend unload callbacks before code-manager resource release.
	( void )m_Runtime.shutdownAll();
	( void )m_CodeManager.shutdown();
}

SIRScriptBindingRegistry& SIRScriptServer::bindings( void ) noexcept
{
	return m_Bindings;
}

SIRScriptRuntime& SIRScriptServer::runtime( void ) noexcept
{
	return m_Runtime;
}

SIRScriptCodeManager& SIRScriptServer::codeManager( void ) noexcept
{
	return m_CodeManager;
}

sirScriptResult_t SIRScriptServer::useCodeManagerSource( void ) noexcept
{
	return m_Runtime.setProgramSource( m_CodeManager.makeProgramSourceAPI() );
}
