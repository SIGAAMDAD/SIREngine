#include <Editor/Host/SIREditorSceneWorkspace.h>

#include <Core/Memory/SIRMemorySegment.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

static constexpr size_t SIR_EDITOR_MAX_SOURCE_BYTES = 32ULL * 1024ULL * 1024ULL;

static bool sirEditorHostHasTag( const char *pLine, const char *pEnd, const char *pszTag ) noexcept
{
	while ( pLine < pEnd && ( *pLine == ' ' || *pLine == '\t' ) ) {
		++pLine;
	}
	const size_t nTagBytes = strlen( pszTag );
	return ( size_t )( pEnd - pLine ) > nTagBytes && memcmp( pLine, pszTag, nTagBytes ) == 0 &&
	       ( pLine[nTagBytes] == ' ' || pLine[nTagBytes] == ']' || pLine[nTagBytes] == '\t' );
}

static bool sirEditorHostCountTags( const char *pSource, size_t nBytes, uint32_t *pNodeCount, uint32_t *pResourceCount ) noexcept
{
	uint32_t nNodes = 0;
	uint32_t nResources = 0;
	const char *pCursor = pSource;
	const char *pEnd = pSource + nBytes;
	while ( pCursor < pEnd ) {
		const char *pLineEnd = ( const char * )memchr( pCursor, '\n', ( size_t )( pEnd - pCursor ) );
		if ( pLineEnd == nullptr ) {
			pLineEnd = pEnd;
		}
		if ( sirEditorHostHasTag( pCursor, pLineEnd, "[node" ) ) {
			if ( nNodes == UINT32_MAX ) {
				return false;
			}
			++nNodes;
		} else if ( sirEditorHostHasTag( pCursor, pLineEnd, "[ext_resource" ) ||
		            sirEditorHostHasTag( pCursor, pLineEnd, "[sub_resource" ) ) {
			if ( nResources == UINT32_MAX ) {
				return false;
			}
			++nResources;
		}
		pCursor = pLineEnd == pEnd ? pEnd : pLineEnd + 1;
	}
	*pNodeCount = nNodes;
	*pResourceCount = nResources;
	return true;
}

template <typename T>
static T *sirEditorHostAllocate( SIRLinearAllocator &arena, uint32_t nCount ) noexcept
{
	if ( nCount == 0 ) {
		return nullptr;
	}
	return ( T * )arena.alloc( ( uint64_t )nCount * sizeof( T ), alignof( T ) );
}

SIREditorSceneWorkspace::SIREditorSceneWorkspace( void ) noexcept
    : m_Scene{}, m_Resources{}, m_Properties{}, m_pszSource( nullptr ), m_nSourceBytes( 0 ), m_nErrorLine( 0 ), m_bInitialized( false ),
      m_bOpen( false )
{
}

SIREditorSceneWorkspace::~SIREditorSceneWorkspace()
{
	shutdown();
}

bool SIREditorSceneWorkspace::initialize( SIRMemorySegment *pEditorSegment ) noexcept
{
	if ( m_bInitialized || pEditorSegment == nullptr || pEditorSegment->getDomain() != SIR_MEMORY_DOMAIN_EDITOR ) {
		return false;
	}
	m_bInitialized = m_Arena.init( pEditorSegment );
	return m_bInitialized;
}

char *SIREditorSceneWorkspace::allocateSource( size_t nSourceBytes ) noexcept
{
	if ( !m_bInitialized || m_bOpen || m_pszSource != nullptr || nSourceBytes == 0 || nSourceBytes > SIR_EDITOR_MAX_SOURCE_BYTES ) {
		return nullptr;
	}
	char *pText = ( char * )m_Arena.alloc( ( uint64_t )nSourceBytes + 1, alignof( char ) );
	if ( pText != nullptr ) {
		m_pszSource = pText;
		m_nSourceBytes = nSourceBytes;
		pText[nSourceBytes] = '\0';
	}
	return pText;
}

void SIREditorSceneWorkspace::rollbackOpen( void ) noexcept
{
	m_Hierarchy.shutdown();
	m_Document.shutdown();
	if ( m_bInitialized ) {
		m_Arena.reset( false );
	}
	m_Scene = {};
	m_Resources = {};
	m_Properties = {};
	m_pszSource = nullptr;
	m_nSourceBytes = 0;
	m_bOpen = false;
}

sirEditorWorkspaceResult_t SIREditorSceneWorkspace::finishOpen( size_t nSourceBytes ) noexcept
{
	uint32_t nNodes = 0;
	uint32_t nResources = 0;
	if ( !sirEditorHostCountTags( m_pszSource, nSourceBytes, &nNodes, &nResources ) ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_INVALID_SCENE;
	}
	if ( nNodes == 0 || nNodes > UINT32_MAX / 2 ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_INVALID_SCENE;
	}
	const uint32_t nResourceCapacity = nResources == 0 ? 1 : nResources;
	const uint64_t nPathBytes = ( uint64_t )nSourceBytes * 3 + ( uint64_t )nNodes * 16 + 1;
	if ( nPathBytes > UINT32_MAX ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}

	m_Scene.storage.pNodes = sirEditorHostAllocate<sirEditorTextSceneNode_t>( m_Arena, nNodes );
	m_Scene.storage.pszPaths = ( char * )m_Arena.alloc( nPathBytes, alignof( char ) );
	m_Scene.storage.nNodeCapacity = nNodes;
	m_Scene.storage.nPathCapacity = ( uint32_t )nPathBytes;
	m_Resources.pResources = sirEditorHostAllocate<sirEditorTextResource_t>( m_Arena, nResourceCapacity );
	m_Resources.nCapacity = nResourceCapacity;
	if ( m_Scene.storage.pNodes == nullptr || m_Scene.storage.pszPaths == nullptr || m_Resources.pResources == nullptr ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}

	const sirEditorTextSceneResult_t eScan = sirEditorScanTextScene( m_pszSource, nSourceBytes, &m_Scene );
	if ( eScan != SIR_EDITOR_TEXT_OK ) {
		m_nErrorLine = m_Scene.nErrorLine;
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_INVALID_SCENE;
	}
	const sirEditorTextResourceResult_t eResources = sirEditorScanTextResources( m_pszSource, nSourceBytes, &m_Resources );
	if ( eResources != SIR_EDITOR_RESOURCE_OK ) {
		m_nErrorLine = m_Resources.nErrorLine;
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_INVALID_RESOURCES;
	}

	sirEditorTextPropertyIndex_t measuredProperties = {};
	const sirEditorTextPropertyResult_t eMeasure =
	    sirEditorScanNodeTextProperties( m_pszSource, nSourceBytes, m_Scene, &measuredProperties );
	if ( eMeasure != SIR_EDITOR_PROPERTY_OK ) {
		m_nErrorLine = measuredProperties.nErrorLine;
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_INVALID_PROPERTIES;
	}
	m_Properties.nCapacity = measuredProperties.nCount;
	m_Properties.pProperties = sirEditorHostAllocate<sirEditorTextProperty_t>( m_Arena, m_Properties.nCapacity );
	if ( m_Properties.nCapacity != 0 && m_Properties.pProperties == nullptr ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}
	const sirEditorTextPropertyResult_t eProperties = sirEditorScanNodeTextProperties( m_pszSource, nSourceBytes, m_Scene, &m_Properties );
	if ( eProperties != SIR_EDITOR_PROPERTY_OK || m_Properties.nCount != measuredProperties.nCount ) {
		m_nErrorLine = m_Properties.nErrorLine;
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_INVALID_PROPERTIES;
	}

	const uint32_t nHistoryDeltas = nNodes * 2;
	uint32_t nIndexCapacity = 1;
	while ( nIndexCapacity < nNodes * 2 ) {
		nIndexCapacity <<= 1;
	}
	sirEditorDocumentStorage_t documentStorage = {};
	documentStorage.pNodes = sirEditorHostAllocate<sirEditorNode_t>( m_Arena, nNodes );
	documentStorage.pStaging = sirEditorHostAllocate<sirEditorNode_t>( m_Arena, nNodes );
	documentStorage.pPending = sirEditorHostAllocate<sirEditorMutation_t>( m_Arena, nNodes );
	documentStorage.pPendingDeltas = sirEditorHostAllocate<sirEditorDelta_t>( m_Arena, nNodes );
	documentStorage.pHistoryDeltas = sirEditorHostAllocate<sirEditorDelta_t>( m_Arena, nHistoryDeltas );
	documentStorage.pHistoryFrames = sirEditorHostAllocate<sirEditorHistoryFrame_t>( m_Arena, 2 );
	documentStorage.pIndex = sirEditorHostAllocate<sirEditorIndexEntry_t>( m_Arena, nIndexCapacity );
	documentStorage.nNodeCapacity = nNodes;
	documentStorage.nPendingCapacity = nNodes;
	documentStorage.nDeltaCapacity = nHistoryDeltas;
	documentStorage.nFrameCapacity = 2;
	documentStorage.nIndexCapacity = nIndexCapacity;
	// Give the authoring workspace typed property storage without creating a
	// second owner of the authored scene graph or its undo/redo history.
	if ( m_Properties.nCount > UINT32_MAX / 4 - nNodes ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}
	const uint32_t nPropertySlots = m_Properties.nCount + nNodes;
	const uint32_t nPropertyDeltaSlots = nPropertySlots * 2;
	documentStorage.pProperties = sirEditorHostAllocate<sirEditorProperty_t>( m_Arena, nPropertySlots );
	documentStorage.pPropertyStaging = sirEditorHostAllocate<sirEditorProperty_t>( m_Arena, nPropertySlots );
	documentStorage.pPendingProperties = sirEditorHostAllocate<sirEditorPropertyMutation_t>( m_Arena, nPropertySlots );
	documentStorage.pPendingPropertyDeltas = sirEditorHostAllocate<sirEditorPropertyDelta_t>( m_Arena, nPropertyDeltaSlots );
	documentStorage.pHistoryPropertyDeltas = sirEditorHostAllocate<sirEditorPropertyDelta_t>( m_Arena, nPropertyDeltaSlots );
	documentStorage.nPropertyCapacity = nPropertySlots;
	documentStorage.nPendingPropertyCapacity = nPropertySlots;
	documentStorage.nPropertyDeltaCapacity = nPropertyDeltaSlots;
	if ( documentStorage.pProperties == nullptr || documentStorage.pPropertyStaging == nullptr ||
	     documentStorage.pPendingProperties == nullptr || documentStorage.pPendingPropertyDeltas == nullptr ||
	     documentStorage.pHistoryPropertyDeltas == nullptr ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}
	sirEditorNode_t *pReconcileScratch = sirEditorHostAllocate<sirEditorNode_t>( m_Arena, nNodes );
	if ( pReconcileScratch == nullptr || m_Document.initialize( documentStorage ) != SIR_EDITOR_OK ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}
	if ( sirEditorReconcileTextScene( m_Document, m_Scene, pReconcileScratch, nNodes ) != SIR_EDITOR_OK ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_DOCUMENT_FAILED;
	}

	sirEditorHierarchyStorage_t hierarchyStorage = {};
	hierarchyStorage.pSnapshot = sirEditorHostAllocate<sirEditorNode_t>( m_Arena, nNodes );
	hierarchyStorage.pLinks = sirEditorHostAllocate<sirEditorHierarchyLink_t>( m_Arena, nNodes );
	hierarchyStorage.pState = sirEditorHostAllocate<sirEditorHierarchyState_t>( m_Arena, nNodes );
	hierarchyStorage.pNextState = sirEditorHostAllocate<sirEditorHierarchyState_t>( m_Arena, nNodes );
	hierarchyStorage.pStack = sirEditorHostAllocate<sirEditorHierarchyCursor_t>( m_Arena, nNodes );
	hierarchyStorage.pVisible = sirEditorHostAllocate<sirEditorHierarchyRow_t>( m_Arena, nNodes );
	hierarchyStorage.pListRows = sirEditorHostAllocate<sirEditorVirtualRow_t>( m_Arena, nNodes );
	hierarchyStorage.pFenwick = sirEditorHostAllocate<uint64_t>( m_Arena, nNodes + 1 );
	hierarchyStorage.nCapacity = nNodes;
	sirEditorHierarchyOrder_t *pOrder = sirEditorHostAllocate<sirEditorHierarchyOrder_t>( m_Arena, nNodes );
	uint32_t nOrderCount = 0;
	if ( pOrder == nullptr || m_Hierarchy.initialize( hierarchyStorage, 20 ) != SIR_EDITOR_HIERARCHY_OK ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_HIERARCHY_FAILED;
	}
	if ( sirEditorHierarchyOrderFromTextScene( m_Scene, pOrder, nNodes, &nOrderCount ) != SIR_EDITOR_HIERARCHY_OK ||
	     m_Hierarchy.rebuild( m_Document, pOrder, nOrderCount ) != SIR_EDITOR_HIERARCHY_OK ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_HIERARCHY_FAILED;
	}

	m_bOpen = true;
	m_nErrorLine = 0;
	return SIR_EDITOR_WORKSPACE_OK;
}

sirEditorWorkspaceResult_t SIREditorSceneWorkspace::openTextSource( const char *pszSource, size_t nSourceBytes ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_WORKSPACE_NOT_INITIALIZED;
	}
	if ( m_bOpen ) {
		return SIR_EDITOR_WORKSPACE_ALREADY_OPEN;
	}
	if ( pszSource == nullptr || nSourceBytes == 0 ) {
		return SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT;
	}
	m_nErrorLine = 0;
	char *pDestination = allocateSource( nSourceBytes );
	if ( pDestination == nullptr ) {
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}
	memcpy( pDestination, pszSource, nSourceBytes );
	return finishOpen( nSourceBytes );
}

sirEditorWorkspaceResult_t SIREditorSceneWorkspace::openTextFile( const char *pszPath ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_EDITOR_WORKSPACE_NOT_INITIALIZED;
	}
	if ( m_bOpen ) {
		return SIR_EDITOR_WORKSPACE_ALREADY_OPEN;
	}
	if ( pszPath == nullptr || *pszPath == '\0' ) {
		return SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT;
	}
	FILE *pFile = fopen( pszPath, "rb" );
	if ( pFile == nullptr ) {
		return SIR_EDITOR_WORKSPACE_FILE_IO;
	}
	if ( fseek( pFile, 0, SEEK_END ) != 0 ) {
		fclose( pFile );
		return SIR_EDITOR_WORKSPACE_FILE_IO;
	}
	long nFileBytes = ftell( pFile );
	if ( nFileBytes <= 0 || ( uint64_t )nFileBytes > SIR_EDITOR_MAX_SOURCE_BYTES || fseek( pFile, 0, SEEK_SET ) != 0 ) {
		fclose( pFile );
		return SIR_EDITOR_WORKSPACE_FILE_IO;
	}
	char *pDestination = allocateSource( ( size_t )nFileBytes );
	if ( pDestination == nullptr ) {
		fclose( pFile );
		return SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY;
	}
	const size_t nRead = fread( pDestination, 1, ( size_t )nFileBytes, pFile );
	const bool bRead = nRead == ( size_t )nFileBytes && ferror( pFile ) == 0;
	fclose( pFile );
	if ( !bRead ) {
		rollbackOpen();
		return SIR_EDITOR_WORKSPACE_FILE_IO;
	}
	return finishOpen( ( size_t )nFileBytes );
}

void SIREditorSceneWorkspace::close( void ) noexcept
{
	if ( m_bInitialized ) {
		rollbackOpen();
	}
	m_nErrorLine = 0;
}

bool SIREditorSceneWorkspace::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return true;
	}
	close();
	if ( !m_Arena.shutdown() ) {
		return false;
	}
	m_bInitialized = false;
	return true;
}

bool SIREditorSceneWorkspace::isOpen( void ) const noexcept
{
	return m_bOpen;
}
uint32_t SIREditorSceneWorkspace::getErrorLine( void ) const noexcept
{
	return m_nErrorLine;
}
const char *SIREditorSceneWorkspace::getSource( void ) const noexcept
{
	return m_bOpen ? m_pszSource : nullptr;
}
size_t SIREditorSceneWorkspace::getSourceBytes( void ) const noexcept
{
	return m_bOpen ? m_nSourceBytes : 0;
}
const sirEditorTextSceneIndex_t &SIREditorSceneWorkspace::getSceneIndex( void ) const noexcept
{
	return m_Scene;
}
const sirEditorTextResourceIndex_t &SIREditorSceneWorkspace::getResourceIndex( void ) const noexcept
{
	return m_Resources;
}
const sirEditorTextPropertyIndex_t &SIREditorSceneWorkspace::getPropertyIndex( void ) const noexcept
{
	return m_Properties;
}
SIREditorSceneDocument &SIREditorSceneWorkspace::getDocument( void ) noexcept
{
	return m_Document;
}
const SIREditorSceneDocument &SIREditorSceneWorkspace::getDocument( void ) const noexcept
{
	return m_Document;
}
SIREditorHierarchyModel &SIREditorSceneWorkspace::getHierarchy( void ) noexcept
{
	return m_Hierarchy;
}
