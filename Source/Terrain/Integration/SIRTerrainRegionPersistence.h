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

#ifndef __SIR_TERRAINREGIONPERSISTENCE_H__
#define __SIR_TERRAINREGIONPERSISTENCE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/File/SIRPlatformFile.h>
#include <Terrain/Data/SIRTerrainRegionBundle.h>

enum class SIRTerrainPersistenceResult : uint8_t {
	SIR_TERRAIN_PERSISTENCE_SUCCESS = 0,
	SIR_TERRAIN_PERSISTENCE_INVALID_ARGUMENT,
	SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE,
	SIR_TERRAIN_PERSISTENCE_BUFFER_TOO_SMALL,
	SIR_TERRAIN_PERSISTENCE_NOT_FOUND,
	SIR_TERRAIN_PERSISTENCE_STAGING_EXISTS,
	SIR_TERRAIN_PERSISTENCE_IO_ERROR
};

// Terrain/editor persistence only: Core/Platform owns OS handles and atomic
// rename semantics. Caller owns the cooked payload and readback storage.
// Returned bundle views borrow the readback storage and do not pin files.
class SIRTerrainRegionPersistence final
{
public:
	static SIRTerrainPersistenceResult saveAtomic( const char *pszDestination, const char *pszStaging,
	                                                 sirTerrainConstByteSpan_t packed ) noexcept;
	static SIRTerrainPersistenceResult load( const char *pszPath, sirTerrainMutableByteSpan_t storage, sirTerrainRegionBundleView_t& view,
	                                           size_t& nLoaded ) noexcept;

protected:
private:
};

#endif // __SIR_TERRAINREGIONPERSISTENCE_H__
