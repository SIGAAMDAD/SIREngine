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

#ifndef __SIR_STREAMINGRUNTIME_H__
#define __SIR_STREAMINGRUNTIME_H__
#include <Streaming/SIRResourceStreamingCoordinator.h>
typedef struct sirStreamingRuntimeConfig_s
{
	SIRResourceStreamingCoordinator *pResources;
	SIRStreamingCoordinator *pCoordinator;
} sirStreamingRuntimeConfig_t;
class SIRStreamingRuntime final
{
public:
	SIRStreamingRuntime( void ) noexcept;
	SIRStreamingRuntime( const SIRStreamingRuntime& ) = delete;
	SIRStreamingRuntime& operator=( const SIRStreamingRuntime& ) = delete;
	sirStreamingResult_t initialize( const sirStreamingRuntimeConfig_t& ) noexcept;
	sirStreamingResult_t tick( const sirStreamingBudget_t& ) noexcept;
	sirStreamingResult_t shutdown( void ) noexcept;
private: SIRResourceStreamingCoordinator *m_pResources; SIRStreamingCoordinator *m_pCoordinator; bool m_bInitialized;
};
#endif
