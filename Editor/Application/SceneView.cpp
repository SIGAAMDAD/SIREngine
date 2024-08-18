#include "SceneView.h"
#include "ScriptLib/ScriptCompiler.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Valden {

CUniquePtr<CSceneView> CSceneView::g_pSceneView;

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
	g_pSceneView = new CSceneView();

	CEditorApplication::Get().AddWidget( g_pSceneView.Get() );
	g_pSceneView->Create( "UnnamedScene" );
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
	if ( ImGui::TreeNodeEx( (void *)"Scripts", ImGuiTreeNodeFlags_OpenOnArrow, "Script Classes" ) ) {
		for ( auto& it : CScriptCompiler::GetObjects() ) {
			if ( ImGui::Selectable( it.GetName().c_str(), ( CScriptCompiler::GetEditorClass() == &it ) ) ) {
				CScriptCompiler::SetEdit( it );
			}
		}
		ImGui::TreePop();
	}

	ImGui::End();
}

};