#include "SceneView.h"

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
}

void CSceneView::Draw( void )
{
	ImGui::Begin( "Scene View##SceneViewWindow", NULL );

	if ( ImGui::TreeNodeEx( (void *)"Resources", ImGuiTreeNodeFlags_OpenOnArrow, "Resources" ) ) {
		ImGui::TreePop();
	}
	if ( ImGui::TreeNodeEx( (void *)"SceneData", ImGuiTreeNodeFlags_OpenOnArrow, "Scene Objects" ) ) {
		ImGui::TreePop();
	}

	ImGui::End();
}

};