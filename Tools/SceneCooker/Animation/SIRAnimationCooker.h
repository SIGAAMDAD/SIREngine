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
#ifndef __SIRENGINE_SIRANIMATIONCOOKER_H__
#define __SIRENGINE_SIRANIMATIONCOOKER_H__

#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Animation/Cooked/SIRAnimationCooked.h>

class SIRAnimationCooker
{
public:
	SIRAnimationCooker( void ) noexcept = delete;

	static sirAnimationCookResult_t cookSkeleton( const sirAnimationCookSkeletonInput_t& source,
	                                              sirAnimationCookOutput_t *pOutput ) noexcept;
	static sirAnimationCookResult_t cookClip( const sirAnimationCookClipInput_t& source, sirAnimationCookOutput_t *pOutput ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONCOOKER_H__
