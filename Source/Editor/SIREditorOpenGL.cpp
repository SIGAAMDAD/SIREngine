#include <Editor/SIREditorOpenGL.h>

#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Editor/Host/SIREditorAuthoringSession.h>
#include <Renderer/Drivers/OpenGL/SIRGLProgrammableDevice.h>
#include <UI/SIRImGuiWindowBridge.h>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <stdint.h>

namespace
{
SIRRenderSurfacePlatform sirEditorSurfacePlatform( sirWindowPlatform ePlatform ) noexcept
{
	switch ( ePlatform ) {
	case sirWindowPlatform::SIR_WINDOW_PLATFORM_WIN32: return SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_WIN32;
	case sirWindowPlatform::SIR_WINDOW_PLATFORM_X11: return SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_X11;
	case sirWindowPlatform::SIR_WINDOW_PLATFORM_WAYLAND: return SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_WAYLAND;
	default: return SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_NONE;
	}
}

struct sirEditorOpenGLState_t
{
	SIRGLProgrammableDevice device;
	SIRGLProgrammableDevice::PresentWindow present;
	SIRMemoryManager memory;
	SIREditorAuthoringSession authoring;
	bool authoringReady{};
	bool imguiReady{};
	uint32_t width{};
	uint32_t height{};
	uint64_t editingNode{};
	double editingOffset{};
	bool scalarEditActive{};
};

static constexpr const char *SIR_EDITOR_AUTHORING_FIXTURE =
	"[gd_scene format=3]\n\n"
	"[node name=\"EditorRoot\" type=\"Node\"]\n\n"
	"[node name=\"FixtureLeft\" type=\"Node\" parent=\".\"]\n"
	"offset_x = -0.75\n\n"
	"[node name=\"FixtureRight\" type=\"Node\" parent=\".\"]\n"
	"offset_x = 0.75\n";

static bool sirEditorOpenAuthoring( sirEditorOpenGLState_t *pState ) noexcept
{
	if ( pState->authoringReady ) return true;
	const sirMemoryConfig_t config = { 64 * 1024 * 1024, 32 * 1024 * 1024, 2 * 1024 * 1024, 1 };
	if ( !pState->memory.init( config ) ) return false;
	SIRMemorySegment *pSegment = pState->memory.createSegment( "EditorAuthoring", 24 * 1024 * 1024, 16 * 1024 * 1024,
	                                                            SIRMemoryDomain::SIR_MEMORY_DOMAIN_EDITOR );
	if ( pSegment == nullptr || !pState->authoring.initialize( pSegment, SIR_EDITOR_AUTHORING_FIXTURE, strlen( SIR_EDITOR_AUTHORING_FIXTURE ) ) ) {
		pState->memory.shutdown();
		return false;
	}
	pState->authoringReady = true;
	return true;
}

static const char *sirEditorNodeName( const SIREditorAuthoringSession& session, uint64_t nNodeId, uint32_t *pLength ) noexcept
{
	const sirEditorTextSceneIndex_t& scene = session.getWorkspace().getSceneIndex();
	for ( uint32_t iNode = 0; iNode < scene.nNodeCount; ++iNode ) {
		const sirEditorTextSceneNode_t& entry = scene.storage.pNodes[iNode];
		if ( entry.node.nId == nNodeId ) {
			if ( pLength != nullptr ) *pLength = entry.nNameLength;
			return session.getWorkspace().getSource() + entry.nNameOffset;
		}
	}
	if ( pLength != nullptr ) *pLength = 0;
	return "Unknown";
}

static float sirEditorNodeOffset( SIREditorAuthoringSession& session, uint64_t nNodeId ) noexcept
{
	sirEditorInspectorEntry_t entries[8] = {};
	uint32_t nEntries = 0;
	if ( session.getInspector().inspect( nNodeId, entries, 8, &nEntries ) != SIREditorResult::SIR_EDITOR_OK ) return 0.0f;
	for ( uint32_t iEntry = 0; iEntry < nEntries; ++iEntry ) {
		if ( entries[iEntry].eKind == SIRPropertyKind::SIR_PROPERTY_REAL64 && entries[iEntry].bHasValue ) {
			double fValue = 0.0;
			memcpy( &fValue, &entries[iEntry].value.nPayload, sizeof( fValue ) );
			return static_cast<float>( fValue );
		}
	}
	return 0.0f;
}

static void sirEditorDrawHierarchy( sirEditorOpenGLState_t *pState ) noexcept
{
	if ( !pState->authoringReady ) {
		ImGui::TextUnformatted( "Authoring session unavailable." );
		return;
	}
	SIREditorHierarchyModel& hierarchy = pState->authoring.getWorkspace().getHierarchy();
	for ( uint32_t iRow = 0; iRow < hierarchy.getVisibleCount(); ++iRow ) {
		sirEditorHierarchyRow_t row = {};
		if ( hierarchy.getVisibleRow( iRow, &row ) != SIREditorHierarchyResult::SIR_EDITOR_HIERARCHY_OK ) continue;
		uint32_t nNameLength = 0;
		const char *pszName = sirEditorNodeName( pState->authoring, row.nId, &nNameLength );
		char szLabel[128] = {};
		snprintf( szLabel, sizeof( szLabel ), "%*s%.*s##node-%llu", static_cast<int>( row.nDepth * 16 ), "", static_cast<int>( nNameLength ), pszName,
		          static_cast<unsigned long long>( row.nId ) );
		if ( ImGui::Selectable( szLabel, row.bSelected ) ) {
			static_cast<void>( pState->authoring.getWorkflow().select( row.nId ) );
			pState->scalarEditActive = false;
		}
	}
}

static void sirEditorDrawInspector( sirEditorOpenGLState_t *pState ) noexcept
{
	if ( !pState->authoringReady ) {
		ImGui::TextUnformatted( "Authoring session unavailable." );
		return;
	}
	SIREditorWorkflow& workflow = pState->authoring.getWorkflow();
	const uint64_t nSelected = workflow.getSelectedId();
	if ( nSelected == 0 ) {
		ImGui::TextUnformatted( "Select FixtureLeft or FixtureRight." );
		return;
	}
	sirEditorInspectorEntry_t entries[8] = {};
	uint32_t nEntries = 0;
	if ( pState->authoring.getInspector().inspect( nSelected, entries, 8, &nEntries ) != SIREditorResult::SIR_EDITOR_OK ) {
		ImGui::TextUnformatted( "Selection is stale." );
		return;
	}
	uint32_t nNameLength = 0;
	const char *pszName = sirEditorNodeName( pState->authoring, nSelected, &nNameLength );
	ImGui::Text( "Node: %.*s", static_cast<int>( nNameLength ), pszName );
	for ( uint32_t iEntry = 0; iEntry < nEntries; ++iEntry ) {
		const sirEditorInspectorEntry_t& entry = entries[iEntry];
		if ( entry.eKind != SIRPropertyKind::SIR_PROPERTY_REAL64 || !entry.bHasValue ) continue;
		double fValue = 0.0;
		memcpy( &fValue, &entry.value.nPayload, sizeof( fValue ) );
		if ( !pState->scalarEditActive || pState->editingNode != nSelected ) {
			pState->editingNode = nSelected;
			pState->editingOffset = fValue;
			pState->scalarEditActive = false;
		}
		if ( ImGui::InputDouble( "offset_x", &pState->editingOffset, 0.05, 0.25, "%.3f" ) ) pState->scalarEditActive = true;
		if ( pState->scalarEditActive && ImGui::IsItemDeactivatedAfterEdit() ) {
			sirPropertyValue_t value = {};
			value.eKind = SIRPropertyKind::SIR_PROPERTY_REAL64;
			memcpy( &value.nPayload, &pState->editingOffset, sizeof( pState->editingOffset ) );
			static_cast<void>( workflow.setSelectedProperty( entry.nPropertyId, value ) );
			pState->scalarEditActive = false;
		}
	}
	if ( ImGui::Button( "Undo" ) ) static_cast<void>( workflow.undo() );
	ImGui::SameLine();
	if ( ImGui::Button( "Redo" ) ) static_cast<void>( workflow.redo() );
}

void sirEditorOpenGLReleaseSurface( sirEditorOpenGLState_t *pState ) noexcept
{
	if ( pState == nullptr ) return;
	if ( pState->imguiReady ) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();
		pState->imguiReady = false;
	}
	if ( pState->present.active ) static_cast<void>( pState->device.destroyPresentWindow( &pState->present ) );
}

bool sirEditorOpenGLFrame( SIRWindowSystem&, sirWindowHandle_t, const sirWindowSnapshot_t& snapshot,
                           const sirWindowNativeView_t& view, void *pContext ) noexcept
{
	sirEditorOpenGLState_t *pState = static_cast<sirEditorOpenGLState_t *>( pContext );
	if ( pState == nullptr ) return false;
	if ( snapshot.bCloseRequested || snapshot.nWidth == 0 || snapshot.nHeight == 0 ) {
		sirEditorOpenGLReleaseSurface( pState );
		return false;
	}
	if ( !pState->imguiReady ) {
		if ( !sirEditorOpenAuthoring( pState ) ) return false;
		const SIRRenderNativeSurface native{
			sirEditorSurfacePlatform( view.ePlatform ), reinterpret_cast<void *>( view.nNativeWindow ),
			reinterpret_cast<void *>( view.nDisplayOrInstance ), view.nWindowGeneration };
		const SIRRenderSwapchainDesc swapchain{ {}, snapshot.nWidth, snapshot.nHeight, 2,
			SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_UNORM, SIRRenderPresentMode::SIR_RENDER_PRESENT_FIFO, false, false };
		if ( native.ePlatform == SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_NONE ||
			 pState->device.initializeForWindow( native ) != SIRRenderResult::SIR_RENDER_SUCCESS ||
			 pState->device.createPresentWindow( native, swapchain, &pState->present ) != SIRRenderResult::SIR_RENDER_SUCCESS ) {
			sirEditorOpenGLReleaseSurface( pState );
			return false;
		}
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		if ( !ImGui_ImplOpenGL3_Init( "#version 330 core" ) ) {
			sirEditorOpenGLReleaseSurface( pState );
			return false;
		}
		pState->imguiReady = true;
		pState->width = snapshot.nWidth;
		pState->height = snapshot.nHeight;
	} else if ( pState->width != snapshot.nWidth || pState->height != snapshot.nHeight ) {
		if ( pState->device.resizePresentWindow( &pState->present, snapshot.nWidth, snapshot.nHeight ) != SIRRenderResult::SIR_RENDER_SUCCESS ) {
			sirEditorOpenGLReleaseSurface( pState );
			return false;
		}
		pState->width = snapshot.nWidth;
		pState->height = snapshot.nHeight;
	}

	if ( pState->device.beginPresentWindow( &pState->present ) != SIRRenderResult::SIR_RENDER_SUCCESS ) {
		sirEditorOpenGLReleaseSurface( pState );
		return false;
	}
	const float leftOffset = pState->authoringReady && pState->authoring.getFixtureNodeCount() > 0
		? sirEditorNodeOffset( pState->authoring, pState->authoring.getFixtureNodeId( 0 ) ) : -0.75f;
	const float rightOffset = pState->authoringReady && pState->authoring.getFixtureNodeCount() > 1
		? sirEditorNodeOffset( pState->authoring, pState->authoring.getFixtureNodeId( 1 ) ) : 0.75f;
	if ( pState->device.renderEditorScene( snapshot.nWidth, snapshot.nHeight, leftOffset, rightOffset ) != SIRRenderResult::SIR_RENDER_SUCCESS ) {
		sirEditorOpenGLReleaseSurface( pState );
		return false;
	}
	ImGui_ImplOpenGL3_NewFrame();
	sirImGuiApplyWindowSnapshot( &snapshot, 1.0f / 60.0f );
	ImGui::NewFrame();
	if ( ImGui::Begin( "Scene" ) ) {
		ImGui::TextUnformatted( "SIREngine editor authoring viewport" );
		ImGui::Text( "OpenGL %u.%u core", pState->device.getProbe().nMajor, pState->device.getProbe().nMinor );
		ImGui::Separator();
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float aspect = snapshot.nHeight == 0 ? 1.0f : static_cast<float>( snapshot.nWidth ) / static_cast<float>( snapshot.nHeight );
		const float imageWidth = available.x;
		const float imageHeight = std::min( available.y, imageWidth / aspect );
		ImGui::Image( static_cast<ImTextureID>( pState->device.getEditorSceneTexture() ),
		              ImVec2( imageWidth, imageHeight ), ImVec2( 0.0f, 1.0f ), ImVec2( 1.0f, 0.0f ) );
	}
	ImGui::End();
	if ( ImGui::Begin( "Hierarchy" ) ) {
		sirEditorDrawHierarchy( pState );
	}
	ImGui::End();
	if ( ImGui::Begin( "Inspector" ) ) {
		sirEditorDrawInspector( pState );
	}
	ImGui::End();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
	if ( pState->device.swapPresentWindow( &pState->present ) != SIRRenderResult::SIR_RENDER_SUCCESS ) {
		sirEditorOpenGLReleaseSurface( pState );
		return false;
	}
	return true;
}
}

int sirEditorRunOpenGL( void ) noexcept
{
	sirEditorOpenGLState_t state;
	const sirWindowCreateDesc_t window{ "SIREngine Editor", 1280, 720, 0 };
	const int result = SIRNativeWindowHost::runNativeWithFrame( &window, 1, sirEditorOpenGLFrame, &state );
	sirEditorOpenGLReleaseSurface( &state );
	if ( state.authoringReady ) state.authoring.shutdown();
	state.memory.shutdown();
	static_cast<void>( state.device.shutdown() );
	return result;
}
