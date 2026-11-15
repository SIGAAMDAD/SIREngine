/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRGEOMETRYBVHCOOKER_H__
#define __SIRENGINE_SIRGEOMETRYBVHCOOKER_H__

#include <SIRPCH.hpp>

#include <vector>

struct sirGeometryCookInput_s;

// The sole static mesh BVH is deterministically cooked from the exact
// canonical triangle topology. Runtime systems only borrow this payload;
// they do not construct competing physics/audio/navigation BVHs.
class SIRGeometryBVHCooker
{
public:
	static bool cook( const sirGeometryCookInput_s& input, std::vector<uint8_t> *pBytes, uint32_t *pNodeCount ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRGEOMETRYBVHCOOKER_H__
