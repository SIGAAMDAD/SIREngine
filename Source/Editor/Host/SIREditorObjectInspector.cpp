/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Editor/Host/SIREditorObjectInspector.h>

SIREditorObjectInspector::SIREditorObjectInspector( void ) noexcept : m_pDocument( nullptr ), m_pRegistry( nullptr )
{
}

sirEditorResult_t SIREditorObjectInspector::initialize( const SIREditorSceneDocument *pDocument,
                                                        const SIRObjectRegistry *pRegistry ) noexcept
{
	if ( pDocument == nullptr || pRegistry == nullptr || m_pDocument != nullptr ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	m_pDocument = pDocument;
	m_pRegistry = pRegistry;
	return SIR_EDITOR_OK;
}

void SIREditorObjectInspector::shutdown( void ) noexcept
{
	m_pDocument = nullptr;
	m_pRegistry = nullptr;
}

sirEditorResult_t SIREditorObjectInspector::inspect( uint64_t nNodeId, sirEditorInspectorEntry_t *pOutput, uint32_t nCapacity,
                                                     uint32_t *pOutCount ) const noexcept
{
	if ( m_pDocument == nullptr || m_pRegistry == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( nNodeId == 0 || pOutCount == nullptr || ( pOutput == nullptr && nCapacity != 0 ) ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	*pOutCount = 0;
	sirEditorNode_t node = {};
	if ( m_pDocument->findNode( nNodeId, &node ) != SIR_EDITOR_OK ) {
		return SIR_EDITOR_NOT_FOUND;
	}
	for ( uint32_t i = 0; i < m_pRegistry->getPropertyCount(); ++i ) {
		const sirObjectPropertyDescriptor_t *pDescriptor = m_pRegistry->getPropertyDescriptor( i );
		if ( pDescriptor != nullptr && m_pRegistry->findProperty( node.nTypeId, pDescriptor->nPropertyId ) == pDescriptor ) {
			++*pOutCount;
		}
	}
	if ( nCapacity < *pOutCount ) {
		return SIR_EDITOR_OUT_OF_CAPACITY;
	}
	uint32_t iOutput = 0;
	for ( uint32_t i = 0; i < m_pRegistry->getPropertyCount(); ++i ) {
		const sirObjectPropertyDescriptor_t *pDescriptor = m_pRegistry->getPropertyDescriptor( i );
		if ( pDescriptor == nullptr || m_pRegistry->findProperty( node.nTypeId, pDescriptor->nPropertyId ) != pDescriptor ) {
			continue;
		}
		sirEditorInspectorEntry_t entry = {};
		entry.nPropertyId = pDescriptor->nPropertyId;
		entry.nNameAtom = pDescriptor->nNameAtom;
		entry.eKind = pDescriptor->eKind;
		entry.bHasValue = m_pDocument->findProperty( nNodeId, pDescriptor->nPropertyId, &entry.value ) == SIR_EDITOR_OK;
		pOutput[iOutput++] = entry;
	}
	return SIR_EDITOR_OK;
}
