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

#include <Streaming/SIRStreamingRuntime.h>
SIRStreamingRuntime::SIRStreamingRuntime( void ) noexcept : m_pResources( nullptr ), m_pCoordinator( nullptr ), m_bInitialized( false ) {}
sirStreamingResult_t SIRStreamingRuntime::initialize( const sirStreamingRuntimeConfig_t& c ) noexcept { if ( m_bInitialized || c.pResources == nullptr || c.pCoordinator == nullptr ) return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT; m_pResources = c.pResources; m_pCoordinator = c.pCoordinator; m_bInitialized = true; return SIRStreamingResult::SIR_STREAMING_OK; }
sirStreamingResult_t SIRStreamingRuntime::tick( const sirStreamingBudget_t& b ) noexcept { return !m_bInitialized ? SIRStreamingResult::SIR_STREAMING_NOT_READY : m_pResources->update( b ); }
sirStreamingResult_t SIRStreamingRuntime::shutdown( void ) noexcept { if ( !m_bInitialized ) return SIRStreamingResult::SIR_STREAMING_OK; const sirStreamingResult_t result = m_pResources->shutdown(); if ( result != SIRStreamingResult::SIR_STREAMING_OK ) return result; m_pResources = nullptr; m_pCoordinator = nullptr; m_bInitialized = false; return SIRStreamingResult::SIR_STREAMING_OK; }
