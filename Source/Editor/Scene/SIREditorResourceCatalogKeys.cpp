/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Editor/Scene/SIREditorResourceKeys.h>
#include <Resources/SIRResourceCatalog.h>

bool sirEditorCollectCatalogKeys( SIRResourceCatalog& catalog, sirEditorResourceKeyBuffer_t *pBuffer ) noexcept
{
	if ( pBuffer == nullptr ) {
		return false;
	}
	pBuffer->nCount = 0;
	if ( pBuffer->pKeys == nullptr || pBuffer->pOrderScratch == nullptr || catalog.getResourceCount() == 0 ||
	     pBuffer->nCapacity < catalog.getResourceCount() ) {
		return false;
	}
	uint32_t nCount = 0;
	if ( catalog.buildLoadOrder( pBuffer->pOrderScratch, pBuffer->nCapacity, &nCount ) != SIR_RESOURCE_CATALOG_OK ||
	     nCount != catalog.getResourceCount() ) {
		return false;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		sirResourceEntry_t entry = {};
		if ( catalog.findResource( pBuffer->pOrderScratch[i], &entry ) != SIR_RESOURCE_CATALOG_OK || entry.nTypeId == 0 ) {
			return false;
		}
		pBuffer->pKeys[i] = { entry.nId, entry.nTypeId };
	}
	pBuffer->nCount = nCount;
	return true;
}
