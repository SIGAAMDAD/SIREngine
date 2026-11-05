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

#ifndef __SIRENGINE_SIRHASHMAP_H__
#define __SIRENGINE_SIRHASHMAP_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRFlatHashMap.h>

// The ordinary hash map explicitly selects the flat, relocation-capable backend.
template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class SIRHashMap : public SIRFlatHashMap<K, V, Hash, Equal>
{
public:
	using SIRFlatHashMap<K, V, Hash, Equal>::SIRFlatHashMap;
};

#endif // __SIRENGINE_SIRHASHMAP_H__
