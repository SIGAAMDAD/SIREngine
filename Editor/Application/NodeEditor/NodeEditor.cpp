#include "NodeBuilder.h"
#include "NodeTree.h"
#include "NodeVariable.h"
#include "NodeEditor.h"
#include <EASTL/vector.h>
#include <imgui/imgui_internal.h>

namespace Valden {

CNodeEditor *CNodeEditor::g_pNodeEditor;

Node_t *CNodeEditor::FindNode( ax::NodeEditor::NodeId id )
{
	Node_t *pNode;
	for ( auto& it : m_Editors ) {
		if ( ( pNode = it->FindNode( id ) ) != NULL ) {
			return pNode;
		}
	}
	return NULL;
}

void CNodeEditor::Init( void )
{
	g_pNodeEditor = new CNodeEditor();
	CEditorApplication::Get().AddWidget( g_pNodeEditor );

	ax::NodeEditor::Config config;
	config.SettingsFile = "Valden/Config/NodeEditorSettings.json";
	config.UserPointer = g_pNodeEditor;

    config.LoadNodeSettings = []( ax::NodeEditor::NodeId nodeId, char *data, void *userPointer) -> size_t {
        auto self = (CNodeEditor *)userPointer;
        auto node = self->FindNode( nodeId );
        
		if ( !node ) {
            return 0;
		}
        if ( data != nullptr ) {
            memcpy( data, node->State.data(), node->State.size() );
		}

        return node->State.size();
    };

//	config.SaveNodeSettings = []( ax::NodeEditor::NodeId nodeId, const char *data, size_t size, ax::NodeEditor::SaveReasonFlags reason,
//		void *userPointer) -> bool
//	{
//	    auto self = (CNodeEditor *)userPointer;
//	    auto node = FindNode( nodeId );
//	    if ( !node ) {
//	        return false;
//		}
//	    node->State.assign( data, size );
//	    TouchNode( nodeId );
//
//	    return true;
//	};

	g_pNodeEditor->m_pContext = ax::NodeEditor::CreateEditor( &config );

	ax::NodeEditor::SetCurrentEditor( g_pNodeEditor->m_pContext );

	g_pNodeEditor->m_Editors.emplace_back( new CNodeTree() );
	g_pNodeEditor->m_pCurrent = g_pNodeEditor->m_Editors.back();
	g_pNodeEditor->m_pSelected = NULL;
	g_pNodeEditor->m_pCurrentClass = new NodeClass_t;
}

CNodeEditor::~CNodeEditor()
{
	ax::NodeEditor::DestroyEditor( m_pContext );
}

void CNodeEditor::DrawTree( NodeClass_t& script, CNodeTree& tree )
{
	uint64_t i;

	//for ( i = 0; i < tree.NumNodes(); i++ ) {
	//}
}

void CNodeEditor::DrawScript( NodeClass_t& script )
{
}

void CNodeEditor::Draw( void )
{
//	if ( ImGui::IsKeyDown( ImGuiKey_MouseRight ) ) {
//		m_bShowNodeWindow = true;
//		m_NodeWindowPos = ImGui::GetCursorScreenPos();
//	}
//
//	if ( m_bShowNodeWindow ) {
//		ImGui::Begin( "Nodes", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize
//			| ImGuiWindowFlags_NoCollapse );
//		ImGui::SetWindowPos( m_NodeWindowPos );
//		
//		DrawNodeTree();
//		
//		ImGui::End();
//
//		if ( ImGui::IsKeyDown( ImGuiKey_MouseLeft ) ) {
//			m_bShowNodeWindow = false;
//		}
//	}


	ImGui::Begin( "Node Editor", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );
	ImGui::SetWindowPos( ImVec2( 0.0f, 24.0f ) );
	ImGui::SetWindowSize(
		ImVec2(
			SIREngine::Application::Get()->GetAppInfo().nWindowWidth,
			SIREngine::Application::Get()->GetAppInfo().nWindowHeight - 24
		)
	);

	ImGui::Begin( "##NodeMenu", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );
	ImGui::SetWindowSize( ImVec2(
		SIREngine::Application::Get()->GetAppInfo().nWindowWidth * 0.25f,
		SIREngine::Application::Get()->GetAppInfo().nWindowHeight - 24
	) );
	ImGui::SetWindowPos( ImVec2( 0.0f, 24.0f ) );

	DrawNodeMenu();

	ImGui::End();

	ImGui::Begin( "##NodeGraph", NULL, ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );
	ImGui::SetWindowSize( ImVec2(
		SIREngine::Application::Get()->GetAppInfo().nWindowWidth * 0.75f,
		SIREngine::Application::Get()->GetAppInfo().nWindowHeight - 24
	) );
	ImGui::SetWindowPos( ImVec2( SIREngine::Application::Get()->GetAppInfo().nWindowWidth * 0.25f, 24.0f ) );

	ax::NodeEditor::Begin( "Node Editor",
		ImVec2(
			SIREngine::Application::Get()->GetAppInfo().nWindowWidth * 0.75f,
			SIREngine::Application::Get()->GetAppInfo().nWindowHeight - 24
		)
	);

	m_pCurrent->Draw();

	ax::NodeEditor::End();

	ImGui::End();

	ImGui::End();
}

#define TOOLTIP( ... ) \
	if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNone ) ) { ImGui::SetTooltip( __VA_ARGS__ ); }

#define TREE_BRANCH( name ) \
	ImGui::TreeNodeEx( (void *)name, ImGuiTreeNodeFlags_OpenOnArrow, name )

#define TREE_LEAF( name ) \
	ImGui::TreeNodeEx( (void *)name, ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf, name )


Node_t *CNodeEditor::DrawNodeTree( void )
{
	Node_t *pNode = NULL;
	if ( TREE_BRANCH( "Utility" ) ) {
		if ( TREE_BRANCH( "Flow Control" ) ) {
			if ( ImGui::MenuItem( "Branch" ) ) {
				pNode = m_pCurrent->SpawnBranchNode();
			}
			if ( ImGui::MenuItem( "Do N" ) ) {
				pNode = m_pCurrent->SpawnDoNNode();
			}
			if ( ImGui::MenuItem( "For Loop with Break" ) ) {
				pNode = m_pCurrent->SpawnForLoopWithBreakNode();
			}
			if ( ImGui::MenuItem( "For Each" ) ) {
				pNode = m_pCurrent->SpawnForEachNode();
			}
			if ( ImGui::MenuItem( "While Loop" ) ) {
				pNode = m_pCurrent->SpawnWhileLoopNode();
			}
			TOOLTIP( "Flow Control" );
			ImGui::TreePop();
		}
		TOOLTIP( "Utility" );
		ImGui::TreePop();
	}
	if ( TREE_BRANCH( "Variables" ) ) {
		if ( TREE_BRANCH( "Get Boolean" ) ) {
			for ( auto& it : m_pCurrentClass->variables ) {
				if ( it.GetType() != EVariableType::Boolean ) {
					continue;
				}
				if ( ImGui::MenuItem( it.GetName().c_str() ) ) {
					pNode = m_pCurrent->SpawnGetVariableBooleanNode( it.GetName() );
				}
			}
			ImGui::TreePop();
		}
		if ( ImGui::MenuItem( "Get Int" ) ) {
			pNode = m_pCurrent->SpawnGetVariableIntNode();
		}
		if ( ImGui::MenuItem( "Get Float" ) ) {
			pNode = m_pCurrent->SpawnGetVariableFloatNode();
		}
		if ( ImGui::MenuItem( "Set" ) ) {

		}
		TOOLTIP( "Variables" );
		ImGui::TreePop();
	}
	if ( pNode ) {
		ax::NodeEditor::SelectNode( pNode->nID );
		ax::NodeEditor::NavigateToSelection();
	}
	return pNode;
}

void CNodeEditor::DrawNodeMenu( void )
{
	if ( ImGui::CollapsingHeader( "Class" ) ) {
		if ( TREE_BRANCH( "Add" ) ) {
			if ( ImGui::Button( "Variable" ) ) {
				m_pCurrentClass->variables.emplace_back( NodeVariable_t( "Unnamed", EVariableType::Int ) );
				m_pSelected = (NodeObject *)&m_pCurrentClass->variables.back();
			}
			ImGui::TreePop();
		}
	}
	if ( ImGui::CollapsingHeader( "Details" ) ) {
		if ( m_pSelected != NULL ) {
			switch ( m_pSelected->nType ) {
			case ENodeObjectType::Class:
			case ENodeObjectType::Variable: {

				if ( ImGui::CollapsingHeader( "Variable" ) ) {
					if ( ImGui::BeginTable( "##VariableData", 2 ) )
					{
						ImGui::TableNextColumn();
						ImGui::TextUnformatted( "Variable Name" );
						ImGui::TableNextColumn();
						if ( ImGui::InputText( "##VariableNameInputTextPrompt", (eastl::string *)&( (NodeVariable_t *)m_pSelected )->GetName(),
							ImGuiInputTextFlags_EscapeClearsAll ) )
						{
							Node_t *node = m_pCurrent->FindNode( ( (NodeVariable_t *)m_pSelected )->GetID() );
							node->Name = ;
//							ImGui::TextUnformatted( ( (NodeVariable_t *)m_pSelected )->GetName().c_str() );
						}

						ImGui::TableNextRow();
						
						ImGui::TableNextColumn();
						ImGui::TextUnformatted( "Variable Type" );
						ImGui::TableNextColumn();
						if ( ImGui::BeginCombo( "##VariableTypeDropDown",
							VariableTypeStrings[ (uint32_t)( (NodeVariable_t *)m_pSelected )->GetType() ] ) )
						{
							for ( uint32_t i = 0; i < SIREngine_StaticArrayLength( VariableTypeStrings ); i++ ) {
								if ( ImGui::Selectable( VariableTypeStrings[i],
									( (uint32_t)( (NodeVariable_t *)m_pSelected )->GetType() == i ) ) )
								{
									( (NodeVariable_t *)m_pSelected )->SetType( (EVariableType)i );
								}
							}
							ImGui::EndCombo();
						}
						ImGui::EndTable();
					}
				}

				break; }
			case ENodeObjectType::Method:
				break;
			};
		}
	}
	ImGui::SeparatorText( "Edit" );
}

uint64_t CNodeEditor::GetVariableNodeID( const ScriptName_t& varName ) const
{
	const auto it = m_VariableMap.find( varName );
	if ( it != m_VariableMap.end() ) {
		return it->second;
	}
	return 0;
}

};