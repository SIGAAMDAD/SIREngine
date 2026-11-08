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
#include <Tools/SIRPackCooker/SIRPackShippingGuard.h>

#include <string.h>

static bool sirShippingSameKey( const SIRResourceKey& a, const SIRResourceKey& b ) noexcept
{
	return a.m_nAssetId == b.m_nAssetId && a.m_nTypeId == b.m_nTypeId && a.m_nVariantId == b.m_nVariantId;
}

static bool sirShippingIsSource( const SIRResourceKey& key, const sirPackShippingPolicy_t& policy ) noexcept
{
	for ( uint32_t i = 0; i < policy.nForbiddenKeys; ++i ) {
		if ( sirShippingSameKey( key, policy.pForbiddenKeys[i] ) )
			return true;
	}
	for ( uint32_t i = 0; i < policy.nForbiddenTypeIds; ++i ) {
		if ( key.m_nTypeId == policy.pForbiddenTypeIds[i] )
			return true;
	}
	return false;
}

static bool sirShippingContains( const uint8_t *pSource, uint64_t nSource, const sirPackShippingPattern_t& pattern ) noexcept
{
	if ( pattern.nBytes > nSource )
		return false;
	const uint64_t last = nSource - pattern.nBytes;
	for ( uint64_t i = 0; i <= last; ++i ) {
		if ( pSource[i] == pattern.pBytes[0] && memcmp( pSource + i, pattern.pBytes, ( size_t )pattern.nBytes ) == 0 )
			return true;
	}
	return false;
}

sirPackResult_t SIRPackShippingGuard::validate( const sirPackCookDesc_t& desc, const sirPackShippingPolicy_t& policy ) noexcept
{
	if ( ( policy.nForbiddenKeys != 0 && policy.pForbiddenKeys == nullptr ) ||
	     ( policy.nForbiddenTypeIds != 0 && policy.pForbiddenTypeIds == nullptr ) ||
	     ( policy.nForbiddenPatterns != 0 && policy.pForbiddenPatterns == nullptr ) ||
	     ( desc.nResourceCount != 0 && desc.pResources == nullptr ) || ( desc.nChunkCount != 0 && desc.pChunks == nullptr ) ||
	     ( desc.nDependencyCount != 0 && desc.pDependencies == nullptr ) )
		return SIR_PACK_INVALID_ARGUMENT;
	for ( uint32_t i = 0; i < policy.nForbiddenPatterns; ++i ) {
		const sirPackShippingPattern_t& p = policy.pForbiddenPatterns[i];
		if ( p.pBytes == nullptr || p.nBytes == 0 || p.nBytes > SIZE_MAX )
			return SIR_PACK_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < desc.nResourceCount; ++i ) {
		const sirPackCookResource_t& resource = desc.pResources[i];
		if ( sirShippingIsSource( resource.key, policy ) )
			return SIR_PACK_INVALID_FORMAT;
		if ( resource.nFirstDependency > desc.nDependencyCount ||
		     resource.nDependencyCount > desc.nDependencyCount - resource.nFirstDependency )
			return SIR_PACK_INVALID_FORMAT;
		for ( uint32_t j = 0; j < resource.nDependencyCount; ++j ) {
			const sirPackDependency_t& edge = desc.pDependencies[resource.nFirstDependency + j];
			// Displacement source input is not a legal runtime edge kind.
			if ( edge.eKind > SIR_RESOURCE_EDGE_KIND_STREAMED || sirShippingIsSource( edge.key, policy ) )
				return SIR_PACK_INVALID_FORMAT;
		}
	}
	for ( uint32_t i = 0; i < desc.nChunkCount; ++i ) {
		const sirPackCookChunk_t& chunk = desc.pChunks[i];
		if ( chunk.nBytes != 0 && chunk.pBytes == nullptr )
			return SIR_PACK_INVALID_ARGUMENT;
		for ( uint32_t j = 0; j < policy.nForbiddenPatterns; ++j ) {
			if ( chunk.pBytes && sirShippingContains( chunk.pBytes, chunk.nBytes, policy.pForbiddenPatterns[j] ) )
				return SIR_PACK_INVALID_FORMAT;
		}
	}
	return SIR_PACK_OK;
}

sirPackResult_t SIRPackShippingGuard::build( const sirPackCookDesc_t& desc, const sirPackShippingPolicy_t& policy, uint8_t *pCatalog,
                                             uint64_t nCatalogCapacity ) noexcept
{
	const sirPackResult_t check = validate( desc, policy );
	if ( check != SIR_PACK_OK )
		return check;
	return SIRPackCooker::build( desc, pCatalog, nCatalogCapacity );
}
