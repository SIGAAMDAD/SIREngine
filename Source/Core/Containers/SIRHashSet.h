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

#ifndef __SIRENGINE_SIRHASHSET_H__
#define __SIRENGINE_SIRHASHSET_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRFlatHashSet.h>

template <typename K, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class SIRHashSet : public SIRFlatHashSet<K, Hash, Equal>
{
public:
	using SIRFlatHashSet<K, Hash, Equal>::SIRFlatHashSet;
};

#endif // __SIRENGINE_SIRHASHSET_H__
