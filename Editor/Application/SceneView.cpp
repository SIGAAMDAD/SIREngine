#include "SceneView.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Valden {

eastl::unique_ptr<CSceneView> CSceneView::g_pSceneView;

void CSceneView::DrawObject( CSceneObject& object )
{
	if ( ImGui::TreeNodeEx( (void *)&object, ImGuiTreeNodeFlags_OpenOnArrow, object.GetName().c_str() ) ) {
		ImGui::TreePop();
	}
}

void CSceneView::Create( const CString& name )
{
}

void CSceneView::Init( void )
{
	g_pSceneView = eastl::make_unique<CSceneView>();

	CEditorApplication::Get().AddWidget( g_pSceneView.get() );
	g_pSceneView->Create( "UnnamedScene" );

	g_pSceneView->m_Resources.GetBase().DirectoryName = "Resources";
	g_pSceneView->m_SceneObjects.GetBase().DirectoryName = "Scene Objects";
}

void CSceneView::Dock( void )
{
	CEditorApplication::Get().DockWindowRight( "Scene View##SceneViewWindow" );
}

void CSceneView::Draw( void )
{
	ImGui::Begin( "Scene View##SceneViewWindow", NULL );
	
	if ( ImGui::BeginTabBar( "##SceneViewQuickTabUtil" ) ) {
//		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 1.0f, 0.0f, 1.0f ) );
		if ( ImGui::BeginTabItem( ICON_FA_PLUS "Add" ) ) {
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	m_Resources.Draw();
	m_SceneObjects.Draw();

	ImGui::End();
}

};