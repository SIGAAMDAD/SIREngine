/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRMESHRESOURCEBRIDGE_H__
#define __SIRENGINE_SIRMESHRESOURCEBRIDGE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshCache.h>
#include <Resources/SIRResourceGraph.h>

constexpr uint32_t SIR_MESH_RESOURCE_TYPE = UINT32_C( 0x4D455348 ); // "MESH"
constexpr uint32_t SIR_MESH_RESOURCE_FORMAT = UINT32_C( 0x00010001 );

typedef struct sirMeshPublishRequest_s
{
	sirMeshKey_t key;
	uint32_t nStagingDomain;
	uint64_t nExpectedGraphRevision;
} sirMeshPublishRequest_t;

SIRResourceKey SIRMeshResourceBridge::getKey( const sirMeshKey_t& key ) noexcept;

SIRResourceContentKey SIRMeshResourceBridge::fingerprint( const sirMeshGeometrySource_t& source ) noexcept;

SIRResourceResult SIRMeshResourceBridge::publish( SIRResourceGraphWriter& writer, const sirMeshGeometrySource_t& source,
                                                  const sirMeshPublishRequest_t& request, SIRResourceHandle* pHandle ) noexcept;

SIRResourceResult SIRMeshResourceBridge::open( const SIRResourceGraph& graph, const SIRResourceReadScope& scope,
                                               const sirMeshKey_t& key, sirMeshGeometryRead_t* pRead ) noexcept;

#endif // __SIRENGINE_SIRMESHRESOURCEBRIDGE_H__