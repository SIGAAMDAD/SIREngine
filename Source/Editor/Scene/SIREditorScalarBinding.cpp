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

#include <Editor/Scene/SIREditorScalarBinding.h>

#include <limits.h>
#include <string.h>

static bool sirEditorBridgeOverlaps( const void *pLeft, size_t nLeft, const void *pRight, size_t nRight ) noexcept
{
	if ( pLeft == nullptr || pRight == nullptr || nLeft == 0 || nRight == 0 ) {
		return false;
	}
	const uintptr_t nA = ( uintptr_t )pLeft;
	const uintptr_t nB = ( uintptr_t )pRight;
	if ( nLeft > UINTPTR_MAX - nA || nRight > UINTPTR_MAX - nB ) {
		return true;
	}
	return nA < nB + nRight && nB < nA + nLeft;
}

static bool sirEditorBridgeValid( const sirEditorScalarBridge_t& context ) noexcept
{
	if ( context.pDocument == nullptr || context.pRegistry == nullptr || context.pIndex == nullptr || context.pszSource == nullptr ) {
		return false;
	}
	if ( context.pIndex->nCount > context.pIndex->nCapacity ||
	     ( context.pIndex->nCount != 0 && context.pIndex->pProperties == nullptr ) ) {
		return false;
	}
	if ( context.nBindingCount > context.pIndex->nCount ) {
		return false;
	}
	if ( context.nBindingCount != 0 && ( context.pBindings == nullptr || context.pScratch == nullptr ) ) {
		return false;
	}
	return true;
}

static bool sirEditorBridgeBindingValid( const sirEditorScalarBridge_t& context, uint32_t iBinding,
                                          sirEditorNode_t *pOutNode ) noexcept
{
	const sirEditorScalarBinding_t& binding = context.pBindings[iBinding];
	if ( binding.nNodeId == 0 || binding.nPropertyId == 0 || binding.iSourceProperty >= context.pIndex->nCount ) {
		return false;
	}
	const sirEditorTextProperty_t& source = context.pIndex->pProperties[binding.iSourceProperty];
	if ( source.nNodeId != binding.nNodeId || source.nValueLength == 0 ||
	     source.nValueOffset > context.nSourceBytes || source.nValueLength > context.nSourceBytes - source.nValueOffset ) {
		return false;
	}
	for ( uint32_t iEarlier = 0; iEarlier < iBinding; ++iEarlier ) {
		const sirEditorScalarBinding_t& prior = context.pBindings[iEarlier];
		if ( prior.iSourceProperty == binding.iSourceProperty ||
		     ( prior.nNodeId == binding.nNodeId && prior.nPropertyId == binding.nPropertyId ) ) {
			return false;
		}
	}
	return context.pDocument->findNode( binding.nNodeId, pOutNode ) == SIR_EDITOR_OK;
}

static bool sirEditorDiscoveryIsScalar( sirPropertyKind_t eKind ) noexcept
{
	return eKind == SIR_PROPERTY_BOOL || eKind == SIR_PROPERTY_INT64 ||
	       eKind == SIR_PROPERTY_UINT64 || eKind == SIR_PROPERTY_REAL64;
}

static sirEditorScalarBridgeResult_t sirEditorDiscoverPass( const sirEditorScalarDiscovery_t& context, bool bWrite,
                                                            uint32_t *pOutCount ) noexcept
{
	uint32_t nMatches = 0;
	for ( uint32_t iSource = 0; iSource < context.pIndex->nCount; ++iSource ) {
		const sirEditorTextProperty_t& source = context.pIndex->pProperties[iSource];
		if ( source.nNodeId == 0 || source.nNameLength == 0 || source.nNameOffset > context.nSourceBytes ||
		     source.nNameLength > context.nSourceBytes - source.nNameOffset ) {
			return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		sirEditorNode_t node = {};
		if ( context.pDocument->findNode( source.nNodeId, &node ) != SIR_EDITOR_OK ) {
			return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		const sirObjectPropertyDescriptor_t *pFound = nullptr;
		for ( uint32_t iProperty = 0; iProperty < context.pRegistry->getPropertyCount(); ++iProperty ) {
			const sirObjectPropertyDescriptor_t *pDescriptor = context.pRegistry->getPropertyDescriptor( iProperty );
			if ( pDescriptor == nullptr || !sirEditorDiscoveryIsScalar( pDescriptor->eKind ) ) {
				continue;
			}
			if ( context.pRegistry->findProperty( node.nTypeId, pDescriptor->nPropertyId ) != pDescriptor ) {
				continue;
			}
			if ( !context.pMatchesAtom( pDescriptor->nNameAtom, context.pszSource + source.nNameOffset,
			                            source.nNameLength, context.pContext ) ) {
				continue;
			}
			if ( pFound != nullptr ) {
				return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
			}
			pFound = pDescriptor;
		}
		if ( pFound == nullptr ) {
			continue;
		}
		for ( uint32_t iPrior = 0; iPrior < iSource; ++iPrior ) {
			const sirEditorTextProperty_t& previous = context.pIndex->pProperties[iPrior];
			if ( previous.nNodeId != source.nNodeId || previous.nNameLength != source.nNameLength ||
			     previous.nNameOffset > context.nSourceBytes ||
			     previous.nNameLength > context.nSourceBytes - previous.nNameOffset ) {
				continue;
			}
			if ( memcmp( context.pszSource + previous.nNameOffset, context.pszSource + source.nNameOffset,
			             source.nNameLength ) == 0 ) {
				return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
			}
		}
		if ( bWrite ) {
			context.pOutput[nMatches] = { source.nNodeId, pFound->nPropertyId, iSource };
		}
		++nMatches;
	}
	*pOutCount = nMatches;
	return SIR_EDITOR_SCALAR_BRIDGE_OK;
}

sirEditorScalarBridgeResult_t sirEditorDiscoverScalarBindings( const sirEditorScalarDiscovery_t& context,
                                                               uint32_t *pOutCount ) noexcept
{
	if ( pOutCount == nullptr ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	*pOutCount = 0;
	if ( context.pDocument == nullptr || context.pRegistry == nullptr || context.pIndex == nullptr ||
	     context.pszSource == nullptr || context.pMatchesAtom == nullptr || context.nSourceBytes == 0 ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( context.pIndex->nCount > context.pIndex->nCapacity ||
	     ( context.pIndex->nCount != 0 && context.pIndex->pProperties == nullptr ) ||
	     ( context.nOutputCapacity != 0 && context.pOutput == nullptr ) ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( context.nOutputCapacity != 0 &&
	     ( sirEditorBridgeOverlaps( context.pOutput, ( size_t )context.nOutputCapacity * sizeof( sirEditorScalarBinding_t ),
	                                context.pIndex->pProperties,
	                                ( size_t )context.pIndex->nCount * sizeof( sirEditorTextProperty_t ) ) ||
	       sirEditorBridgeOverlaps( context.pOutput, ( size_t )context.nOutputCapacity * sizeof( sirEditorScalarBinding_t ),
	                                context.pszSource, context.nSourceBytes ) ) ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	uint32_t nMatches = 0;
	sirEditorScalarBridgeResult_t eResult = sirEditorDiscoverPass( context, false, &nMatches );
	if ( eResult != SIR_EDITOR_SCALAR_BRIDGE_OK ) {
		return eResult;
	}
	*pOutCount = nMatches;
	if ( context.pOutput == nullptr && context.nOutputCapacity == 0 ) {
		return SIR_EDITOR_SCALAR_BRIDGE_OK;
	}
	if ( context.nOutputCapacity < nMatches ) {
		return SIR_EDITOR_SCALAR_BRIDGE_OUT_OF_CAPACITY;
	}
	uint32_t nWritten = 0;
	eResult = sirEditorDiscoverPass( context, true, &nWritten );
	return eResult == SIR_EDITOR_SCALAR_BRIDGE_OK && nWritten == nMatches ? SIR_EDITOR_SCALAR_BRIDGE_OK
	                                                                      : SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
}

sirEditorScalarBridgeResult_t sirEditorImportBoundScalars( const sirEditorScalarBridge_t& context ) noexcept
{
	if ( !sirEditorBridgeValid( context ) ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( context.nBindingCount == 0 ) {
		return SIR_EDITOR_SCALAR_BRIDGE_OK;
	}
	for ( uint32_t iBinding = 0; iBinding < context.nBindingCount; ++iBinding ) {
		sirEditorNode_t node = {};
		if ( !sirEditorBridgeBindingValid( context, iBinding, &node ) ) {
			return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		const sirEditorScalarBinding_t& binding = context.pBindings[iBinding];
		const sirObjectPropertyDescriptor_t *pDescriptor = context.pRegistry->findProperty( node.nTypeId, binding.nPropertyId );
		if ( pDescriptor == nullptr ) {
			return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		const sirEditorTextProperty_t& source = context.pIndex->pProperties[binding.iSourceProperty];
		sirEditorProperty_t& staged = context.pScratch[iBinding];
		staged = {};
		staged.nNodeId = binding.nNodeId;
		staged.nPropertyId = binding.nPropertyId;
		const sirEditorScalarResult_t eParse = sirEditorDecodeScalar( context.pszSource + source.nValueOffset,
		                                                             source.nValueLength, pDescriptor->eKind, &staged.value );
		if ( eParse != SIR_EDITOR_SCALAR_OK ) {
			return eParse == SIR_EDITOR_SCALAR_UNSUPPORTED_KIND ? SIR_EDITOR_SCALAR_BRIDGE_UNSUPPORTED_VALUE
			                                                    : SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		if ( context.pRegistry->validateProperty( node.nTypeId, binding.nPropertyId, staged.value ) != SIR_OBJECT_OK ) {
			return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
	}
	if ( context.pDocument->setPropertyRegistry( context.pRegistry ) != SIR_EDITOR_OK ) {
		return SIR_EDITOR_SCALAR_BRIDGE_DOCUMENT_ERROR;
	}
	const sirEditorResult_t eBegin = context.pDocument->beginEdit( context.pDocument->getRevision() );
	if ( eBegin != SIR_EDITOR_OK ) {
		return SIR_EDITOR_SCALAR_BRIDGE_DOCUMENT_ERROR;
	}
	for ( uint32_t iBinding = 0; iBinding < context.nBindingCount; ++iBinding ) {
		if ( context.pDocument->queueSetProperty( context.pScratch[iBinding] ) != SIR_EDITOR_OK ) {
			context.pDocument->cancelEdit();
			return SIR_EDITOR_SCALAR_BRIDGE_DOCUMENT_ERROR;
		}
	}
	if ( context.pDocument->commitEdit() != SIR_EDITOR_OK ) {
		context.pDocument->cancelEdit();
		return SIR_EDITOR_SCALAR_BRIDGE_DOCUMENT_ERROR;
	}
	return SIR_EDITOR_SCALAR_BRIDGE_OK;
}

sirEditorScalarBridgeResult_t sirEditorExportBoundScalars( const sirEditorScalarBridge_t& context, char *pszOutput,
                                                           size_t nCapacity, size_t *pOutBytes ) noexcept
{
	if ( pOutBytes == nullptr || ( pszOutput == nullptr && nCapacity != 0 ) || !sirEditorBridgeValid( context ) ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	*pOutBytes = 0;
	if ( context.nBindingCount > SIZE_MAX / 128 ||
	     context.nEncodedScalarBytes < ( size_t )context.nBindingCount * 128 ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( context.nBindingCount != 0 && ( context.pEdits == nullptr || context.pszEncodedScalars == nullptr ) ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	if ( sirEditorBridgeOverlaps( pszOutput, nCapacity, context.pszEncodedScalars, context.nEncodedScalarBytes ) ||
	     sirEditorBridgeOverlaps( pszOutput, nCapacity, context.pEdits,
	                              ( size_t )context.nBindingCount * sizeof( sirEditorTextEdit_t ) ) ) {
		return SIR_EDITOR_SCALAR_BRIDGE_INVALID_ARGUMENT;
	}
	for ( uint32_t iBinding = 0; iBinding < context.nBindingCount; ++iBinding ) {
		sirEditorNode_t node = {};
		if ( !sirEditorBridgeBindingValid( context, iBinding, &node ) ) {
			return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		const sirEditorScalarBinding_t& binding = context.pBindings[iBinding];
		sirPropertyValue_t value = {};
		if ( context.pDocument->findProperty( binding.nNodeId, binding.nPropertyId, &value ) != SIR_EDITOR_OK ) {
			return SIR_EDITOR_SCALAR_BRIDGE_DOCUMENT_ERROR;
		}
		if ( context.pRegistry->validateProperty( node.nTypeId, binding.nPropertyId, value ) != SIR_OBJECT_OK ) {
			return SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		size_t nEncoded = 0;
		char *pszEncoded = context.pszEncodedScalars + ( size_t )iBinding * 128;
		const sirEditorScalarResult_t eWrite = sirEditorEncodeScalar( value, pszEncoded, 128, &nEncoded );
		if ( eWrite != SIR_EDITOR_SCALAR_OK ) {
			return eWrite == SIR_EDITOR_SCALAR_UNSUPPORTED_KIND ? SIR_EDITOR_SCALAR_BRIDGE_UNSUPPORTED_VALUE
			                                                    : SIR_EDITOR_SCALAR_BRIDGE_INVALID_BINDING;
		}
		context.pEdits[iBinding] = { binding.iSourceProperty, pszEncoded, ( uint32_t )nEncoded };
	}
	const sirEditorTextWriteResult_t eWrite = sirEditorWriteScalarPatches( context.pszSource, context.nSourceBytes,
	                                                                      *context.pIndex, context.pEdits, context.nBindingCount,
	                                                                      pszOutput, nCapacity, pOutBytes );
	return eWrite == SIR_EDITOR_WRITE_OK ? SIR_EDITOR_SCALAR_BRIDGE_OK : SIR_EDITOR_SCALAR_BRIDGE_WRITE_ERROR;
}
