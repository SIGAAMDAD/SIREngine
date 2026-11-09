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
#include <Editor/Host/SIREditorWorkflow.h>

SIREditorWorkflow::SIREditorWorkflow( void ) noexcept
    : m_pDocument( nullptr ), m_pHierarchy( nullptr ), m_nSelectedId( 0 ), m_nRevision( 0 )
{
}

uint64_t SIREditorWorkflow::getSelectedId( void ) const noexcept
{
	return m_nSelectedId;
}
uint64_t SIREditorWorkflow::getLastKnownRevision( void ) const noexcept
{
	return m_nRevision;
}

sirEditorResult_t SIREditorWorkflow::initialize( SIREditorSceneDocument *pDocument, SIREditorHierarchyModel *pHierarchy ) noexcept
{
	if ( pDocument == nullptr || m_pDocument != nullptr ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	const uint64_t nRevision = pDocument->getRevision();
	if ( nRevision == 0 ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	m_pDocument = pDocument;
	m_pHierarchy = pHierarchy;
	m_nSelectedId = 0;
	m_nRevision = nRevision;
	return SIR_EDITOR_OK;
}

void SIREditorWorkflow::shutdown( void ) noexcept
{
	m_pDocument = nullptr;
	m_pHierarchy = nullptr;
	m_nSelectedId = 0;
	m_nRevision = 0;
}

sirEditorResult_t SIREditorWorkflow::synchronize( void ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( m_pHierarchy != nullptr ) {
		if ( m_pHierarchy->rebuild( *m_pDocument ) != SIR_EDITOR_HIERARCHY_OK ) {
			return SIR_EDITOR_OUT_OF_CAPACITY;
		}
	}
	m_nRevision = m_pDocument->getRevision();
	if ( m_nSelectedId != 0 ) {
		sirEditorNode_t node = {};
		if ( m_pDocument->findNode( m_nSelectedId, &node ) != SIR_EDITOR_OK ) {
			m_nSelectedId = 0;
		} else if ( m_pHierarchy != nullptr && m_pHierarchy->setSelected( *m_pDocument, m_nSelectedId ) != SIR_EDITOR_HIERARCHY_OK ) {
			return SIR_EDITOR_INVALID_HIERARCHY;
		}
	}
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorWorkflow::select( uint64_t nNodeId ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( nNodeId != 0 ) {
		sirEditorNode_t node = {};
		if ( m_pDocument->findNode( nNodeId, &node ) != SIR_EDITOR_OK ) {
			return SIR_EDITOR_NOT_FOUND;
		}
	}
	if ( m_pHierarchy != nullptr && nNodeId != 0 && m_pHierarchy->setSelected( *m_pDocument, nNodeId ) != SIR_EDITOR_HIERARCHY_OK ) {
		return SIR_EDITOR_INVALID_HIERARCHY;
	}
	m_nSelectedId = nNodeId;
	return SIR_EDITOR_OK;
}

sirEditorResult_t SIREditorWorkflow::commit( void ) noexcept
{
	const sirEditorResult_t eResult = m_pDocument->commitEdit();
	if ( eResult != SIR_EDITOR_OK ) {
		m_pDocument->cancelEdit();
		return eResult;
	}
	return synchronize();
}

sirEditorResult_t SIREditorWorkflow::createNode( const sirEditorNode_t& node ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	const sirEditorResult_t eBegin = m_pDocument->beginEdit( m_nRevision );
	if ( eBegin != SIR_EDITOR_OK ) {
		return eBegin;
	}
	const sirEditorResult_t eQueue = m_pDocument->queueUpsert( node );
	if ( eQueue != SIR_EDITOR_OK ) {
		m_pDocument->cancelEdit();
		return eQueue;
	}
	const sirEditorResult_t eResult = commit();
	if ( eResult == SIR_EDITOR_OK ) {
		return select( node.nId );
	}
	return eResult;
}

sirEditorResult_t SIREditorWorkflow::removeSelected( void ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( m_nSelectedId == 0 ) {
		return SIR_EDITOR_NOT_FOUND;
	}
	const sirEditorResult_t eBegin = m_pDocument->beginEdit( m_nRevision );
	if ( eBegin != SIR_EDITOR_OK ) {
		return eBegin;
	}
	const sirEditorResult_t eQueue = m_pDocument->queueRemove( m_nSelectedId );
	if ( eQueue != SIR_EDITOR_OK ) {
		m_pDocument->cancelEdit();
		return eQueue;
	}
	return commit();
}

sirEditorResult_t SIREditorWorkflow::setSelectedProperty( uint32_t nPropertyId, const sirPropertyValue_t& value ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( m_nSelectedId == 0 || nPropertyId == 0 ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	const sirEditorResult_t eBegin = m_pDocument->beginEdit( m_nRevision );
	if ( eBegin != SIR_EDITOR_OK ) {
		return eBegin;
	}
	sirEditorProperty_t property = {};
	property.nNodeId = m_nSelectedId;
	property.nPropertyId = nPropertyId;
	property.value = value;
	const sirEditorResult_t eQueue = m_pDocument->queueSetProperty( property );
	if ( eQueue != SIR_EDITOR_OK ) {
		m_pDocument->cancelEdit();
		return eQueue;
	}
	return commit();
}

sirEditorResult_t SIREditorWorkflow::removeSelectedProperty( uint32_t nPropertyId ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	if ( m_nSelectedId == 0 || nPropertyId == 0 ) {
		return SIR_EDITOR_INVALID_ARGUMENT;
	}
	const sirEditorResult_t eBegin = m_pDocument->beginEdit( m_nRevision );
	if ( eBegin != SIR_EDITOR_OK ) {
		return eBegin;
	}
	const sirEditorResult_t eQueue = m_pDocument->queueRemoveProperty( m_nSelectedId, nPropertyId );
	if ( eQueue != SIR_EDITOR_OK ) {
		m_pDocument->cancelEdit();
		return eQueue;
	}
	return commit();
}

sirEditorResult_t SIREditorWorkflow::undo( void ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	const sirEditorResult_t eResult = m_pDocument->undo();
	return eResult == SIR_EDITOR_OK ? synchronize() : eResult;
}

sirEditorResult_t SIREditorWorkflow::redo( void ) noexcept
{
	if ( m_pDocument == nullptr ) {
		return SIR_EDITOR_NOT_INITIALIZED;
	}
	const sirEditorResult_t eResult = m_pDocument->redo();
	return eResult == SIR_EDITOR_OK ? synchronize() : eResult;
}
