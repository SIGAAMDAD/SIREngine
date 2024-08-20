#include "SceneView.h"
#include "ScriptLib/ScriptCompiler.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "Project/ProjectManager.h"
#include <EASTL/algorithm.h>

namespace Valden {

CUniquePtr<CSceneView> CSceneView::g_pSceneView;

void CSceneView::DrawObject( ISceneObject *pObject )
{
	ImGuiTreeNodeFlags flags = 0;

	if ( m_bRenaming && m_pRenameTarget == &pObject->GetName() ) {
		if ( ImGui::InputText( "##Renamer_SceneObjectNameEditUtil", m_pRenameTarget, ImGuiInputTextFlags_EnterReturnsTrue ) ) {
			m_bRenaming = false;
			m_pRenameTarget = NULL;
		}
//		if ( !ImGui::IsItemFocused() ) {
//			m_bRenaming = false;
//			m_pRenameTarget = NULL;
//		}
		return;
	}

	switch ( pObject->GetType() ) {
	case ESceneObjectType::Animation:
	case ESceneObjectType::Item:
	case ESceneObjectType::SyncPoint:
	case ESceneObjectType::Light:
		break;
	case ESceneObjectType::ScriptClass: {
		if ( ImGui::Selectable( pObject->GetName().c_str(), ( CScriptCompiler::GetEditorClass() == pObject ) ) ) {
			CScriptCompiler::SetEdit( Cast<CScriptObject>( pObject ) );
		}
		break; }
	case ESceneObjectType::Actor:
	case ESceneObjectType::ScriptNameSpace: {
		flags |= ImGuiTreeNodeFlags_None;
		if ( ImGui::TreeNodeEx( (void *)pObject, flags, pObject->GetName().c_str() ) ) {
			switch ( pObject->GetType() ) {
			case ESceneObjectType::Actor:
				break;
			case ESceneObjectType::ScriptNameSpace: {
				for ( auto& it : Cast<CScriptNameSpace>( pObject )->GetClasses() ) {
					DrawObject( it );
				}
				break; }
			};
			ImGui::TreePop();
		}
		break; }
	};

	if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
		m_pEditTarget = pObject;
		ImGui::OpenPopup( "##SceneObjectEditUtilPopup" );
	}
	if ( m_pEditTarget == pObject ) {
		if ( ImGui::BeginPopup( "##SceneObjectEditUtilPopup" ) ) {
			ImGui::SeparatorText( "Common" );
			ImGui::Indent( 0.75f );
			if ( ImGui::MenuItem( "Rename##Rename_SceneObjectEditUtilPopup" ) ) {
				m_pRenameTarget = &m_pEditTarget->GetName();
				m_bRenaming = true;
			}
			if ( ImGui::MenuItem( "Delete##Delete_SceneObjectEditUtilPopup" ) ) {
				RemoveObject( m_pEditTarget );
				ImGui::Unindent( 0.75f );
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return;
			}
			ImGui::Unindent();

			switch ( m_pEditTarget->GetType() ) {
			case ESceneObjectType::Actor:
			case ESceneObjectType::Animation:
			case ESceneObjectType::Item:
			case ESceneObjectType::SyncPoint:
				break;
			case ESceneObjectType::Light:
				break;
			case ESceneObjectType::ScriptClass:
				break;
			case ESceneObjectType::ScriptNameSpace:
				if ( ImGui::MenuItem( "Add Class##AddClass_SceneObjectEditUtilPopup" ) ) {
					AddObject( "New Class", ESceneObjectType::ScriptClass );
					Cast<CScriptNameSpace>( m_pEditTarget )->AddClass(
						Cast<CScriptObject>( m_SceneObjects[ ESceneObjectType::ScriptClass ].back() )
					);
				}
				break;
			};
			ImGui::EndPopup();
		}
	}
}

void CSceneView::RemoveObject( ISceneObject *pObject )
{
	uint32_t nType = pObject->GetType();

	SIRENGINE_LOG( "Removing ISceneObject \"%s\"...", pObject->GetName().c_str() );

	switch ( pObject->GetType() ) {
	case ESceneObjectType::ScriptClass:
		Cast<CScriptObject>( pObject )->GetNameSpace()->RemoveClass( Cast<CScriptObject>( pObject ) );
		break;
	};

	Mem_Free( pObject );

	eastl::erase( m_SceneObjects[ nType ], pObject );
}

void CSceneView::AddObject( const CString& name, uint32_t nType )
{
	ISceneObject *sceneObject;

	switch ( nType ) {
	case ESceneObjectType::Actor:
	case ESceneObjectType::Animation:
	case ESceneObjectType::Item:
	case ESceneObjectType::SyncPoint:
		break;
	case ESceneObjectType::Light:
		break;
	case ESceneObjectType::ScriptClass:
		sceneObject = new ( Mem_Alloc( sizeof( CScriptObject ) ) ) CScriptObject();
		CScriptCompiler::Get().AddClass( Cast<CScriptObject>( sceneObject ) );
		break;
	case ESceneObjectType::ScriptNameSpace:
		sceneObject = new ( Mem_Alloc( sizeof( CScriptNameSpace ) ) ) CScriptNameSpace( name );
		break;
	};

	m_SceneObjects[ nType ].emplace_back( sceneObject );
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
	
	if ( ImGui::Button( ICON_FA_PLUS "Add##SceneViewAdd" ) ) {
		ImGui::OpenPopup( "##AddSceneObjectPopup" );
	}
	if ( ImGui::BeginPopup( "##AddSceneObjectPopup" ) ) {
		ImGui::SeparatorText( "Scripting" );
		ImGui::Indent( 0.75f );
		if ( ImGui::MenuItem( "Script Class##ScriptClass_SceneViewAdd" ) ) {
			AddObject( "New Class", ESceneObjectType::ScriptClass );
			CProjectManager::Get()->GetProject()->SetModified( true );
		}
		if ( ImGui::MenuItem( "Script NameSpace##ScriptNameSpace_SceneViewAdd" ) ) {
			AddObject( "New NameSpace", ESceneObjectType::ScriptNameSpace );
			CProjectManager::Get()->GetProject()->SetModified( true );
		}
		ImGui::Unindent( 0.75f );

		ImGui::EndPopup();
	}

	if ( ImGui::TreeNodeEx( "Scripts" ) ) {
		for ( auto& it : m_SceneObjects[ ESceneObjectType::ScriptNameSpace ] ) {
			DrawObject( it );
		}
		ImGui::TreePop();
	}

	ImGui::End();
}

};