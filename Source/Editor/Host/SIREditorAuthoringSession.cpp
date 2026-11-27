#include <Editor/Host/SIREditorAuthoringSession.h>

#include <Editor/Scene/SIREditorScalarCodec.h>

#include <string.h>

static bool sirEditorSessionMatchesOffsetX( uint64_t nAtom, const char *pszSource, uint32_t nBytes, void *pContext ) noexcept
{
	static_cast<void>(pContext);
	return nAtom == 1 && nBytes == 8 && memcmp( pszSource, "offset_x", 8 ) == 0;
}

SIREditorAuthoringSession::SIREditorAuthoringSession( void ) noexcept = default;

SIREditorAuthoringSession::~SIREditorAuthoringSession()
{
	shutdown();
}

static bool sirEditorSessionHasType( const sirObjectTypeDescriptor_t *pTypes, uint32_t nCount, uint32_t nTypeId ) noexcept
{
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( pTypes[i].nTypeId == nTypeId ) return true;
	}
	return false;
}

bool SIREditorAuthoringSession::initialize( SIRMemorySegment *pEditorSegment, const char *pszSource, size_t nSourceBytes ) noexcept
{
	if ( m_bReady || pEditorSegment == nullptr || pszSource == nullptr || nSourceBytes == 0 ) return false;
	if ( !m_Workspace.initialize( pEditorSegment ) ||
	     m_Workspace.openTextSource( pszSource, nSourceBytes ) != SIREditorWorkspaceResult::SIR_EDITOR_WORKSPACE_OK ) {
		shutdown();
		return false;
	}

	const sirEditorTextSceneIndex_t& scene = m_Workspace.getSceneIndex();
	for ( uint32_t i = 0; i < scene.nNodeCount && m_nFixtureCount < 8; ++i ) {
		if ( scene.storage.pNodes[i].node.nParentId != 0 ) m_FixtureIds[m_nFixtureCount++] = scene.storage.pNodes[i].node.nId;
	}
	if ( m_nFixtureCount == 0 ) {
		shutdown();
		return false;
	}

	if ( m_Registry.initialize( m_Types, 8, m_Properties, 8 ) != SIRObjectResult::SIR_OBJECT_OK ) {
		shutdown();
		return false;
	}
	for ( uint32_t i = 0; i < scene.nNodeCount && i < 8; ++i ) {
		const uint32_t nTypeId = scene.storage.pNodes[i].node.nTypeId;
		if ( !sirEditorSessionHasType( m_Types, m_Registry.getTypeCount(), nTypeId ) &&
		     m_Registry.registerType( { nTypeId, 0, static_cast<uint64_t>( nTypeId ) } ) != SIRObjectResult::SIR_OBJECT_OK ) {
			shutdown();
			return false;
		}
	}
	m_nPropertyId = 1;
	const sirEditorNode_t firstFixture = scene.storage.pNodes[1].node;
	if ( m_Registry.registerProperty( { m_nPropertyId, firstFixture.nTypeId, 1, SIRPropertyKind::SIR_PROPERTY_REAL64, {} } ) != SIRObjectResult::SIR_OBJECT_OK ||
	     m_Registry.seal() != SIRObjectResult::SIR_OBJECT_OK ||
	     m_Workspace.getDocument().setPropertyRegistry( &m_Registry ) != SIREditorResult::SIR_EDITOR_OK ) {
		shutdown();
		return false;
	}
	m_bRegistryReady = true;

	sirEditorScalarDiscovery_t discovery = {};
	discovery.pDocument = &m_Workspace.getDocument();
	discovery.pRegistry = &m_Registry;
	discovery.pIndex = &m_Workspace.getPropertyIndex();
	discovery.pszSource = m_Workspace.getSource();
	discovery.nSourceBytes = m_Workspace.getSourceBytes();
	discovery.pMatchesAtom = sirEditorSessionMatchesOffsetX;
	uint32_t nBindings = 0;
	if ( sirEditorDiscoverScalarBindings( discovery, &nBindings ) != SIREditorScalarBridgeResult::SIR_EDITOR_SCALAR_BRIDGE_OK || nBindings > 8 ) {
		shutdown();
		return false;
	}
	discovery.pOutput = m_Bindings;
	discovery.nOutputCapacity = 8;
	if ( sirEditorDiscoverScalarBindings( discovery, &nBindings ) != SIREditorScalarBridgeResult::SIR_EDITOR_SCALAR_BRIDGE_OK ) {
		shutdown();
		return false;
	}
	sirEditorScalarBridge_t bridge = {};
	bridge.pDocument = &m_Workspace.getDocument();
	bridge.pRegistry = &m_Registry;
	bridge.pIndex = &m_Workspace.getPropertyIndex();
	bridge.pszSource = m_Workspace.getSource();
	bridge.nSourceBytes = m_Workspace.getSourceBytes();
	bridge.pBindings = m_Bindings;
	bridge.nBindingCount = nBindings;
	bridge.pScratch = m_Scratch;
	bridge.pEdits = m_Edits;
	bridge.pszEncodedScalars = m_EncodedScalars;
	bridge.nEncodedScalarBytes = sizeof( m_EncodedScalars );
	if ( sirEditorImportBoundScalars( bridge ) != SIREditorScalarBridgeResult::SIR_EDITOR_SCALAR_BRIDGE_OK ) {
		shutdown();
		return false;
	}
	if ( m_Workflow.initialize( &m_Workspace.getDocument(), &m_Workspace.getHierarchy() ) != SIREditorResult::SIR_EDITOR_OK ||
	     m_Workflow.synchronize() != SIREditorResult::SIR_EDITOR_OK ||
	     m_Inspector.initialize( &m_Workspace.getDocument(), &m_Registry ) != SIREditorResult::SIR_EDITOR_OK ) {
		shutdown();
		return false;
	}
	if ( m_Workspace.getHierarchy().rebuild( m_Workspace.getDocument() ) != SIREditorHierarchyResult::SIR_EDITOR_HIERARCHY_OK ) {
		shutdown();
		return false;
	}
	m_bReady = true;
	return true;
}

void SIREditorAuthoringSession::shutdown( void ) noexcept
{
	m_Inspector.shutdown();
	m_Workflow.shutdown();
	if ( m_bRegistryReady ) m_Registry.shutdown();
	m_bRegistryReady = false;
	m_Workspace.shutdown();
	m_nFixtureCount = 0;
	m_nPropertyId = 0;
	m_bReady = false;
}

uint64_t SIREditorAuthoringSession::getFixtureNodeId( uint32_t iFixture ) const noexcept
{
	return iFixture < m_nFixtureCount ? m_FixtureIds[iFixture] : 0;
}
