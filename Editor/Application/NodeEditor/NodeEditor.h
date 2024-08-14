#ifndef __VALDEN_NODE_EDITOR_H__
#define __VALDEN_NODE_EDITOR_H__

#pragma once

#include <Engine/Memory/Memory.h>
#include "../Application.h"
#include <Engine/Core/ThreadSystem/Thread.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui-node-editor/imgui_node_editor.h>

namespace Valden {
	typedef struct NodeFunction {
		NodeObject data;

		// where execution starts
		Node_t *pBase;
	} NodeFunction_t;

	typedef struct NodeClass NodeClass_t;
	typedef struct {
		NodeObject data;

		NodeFunction_t base;
		NodeClass_t *owner;
	} NodeMethod_t;

	typedef struct {
		NodeObject data;
	} NodeGlobalVar_t;

	typedef struct NodeClass {
		NodeObject data;
		eastl::vector<NodeMethod_t> methods;
		eastl::vector<NodeVariable_t> variables;
	} NodeClass_t;

	class CNodeEditor : public IEditorWidget
	{
	public:
		CNodeEditor( void )
			: IEditorWidget( "NodeEditor" )
		{ }
		~CNodeEditor();

		static void Init( void );

		virtual void Draw( void ) override;

		uint64_t GetVariableNodeID( const ScriptName_t& varName ) const;
		inline void SetVariableNodeID( const ScriptName_t& varName, uint64_t nodeId )
		{ m_VariableMap.try_emplace( varName, nodeId ); }

		SIRENGINE_FORCEINLINE static CNodeEditor *Get( void )
		{ return g_pNodeEditor; }
	private:
		friend class CNodeTree;

		void DrawTree( NodeClass_t& script, CNodeTree& tree );
		void DrawScript( NodeClass_t& script );
		void DrawNodeMenu( void );
		Node_t *DrawNodeTree( void );

		Node_t *FindNode( ax::NodeEditor::NodeId id );

		void CreateFunction( void );
		void CreateClass( void );

		eastl::unordered_map<ScriptName_t, uint64_t> m_VariableMap;

		eastl::vector<CNodeTree *> m_Editors;
		NodeClass_t *m_pCurrentClass;
		CNodeTree *m_pCurrent;
		NodeObject *m_pSelected;

		bool32 m_bShowNodeWindow;
		ImVec2 m_NodeWindowPos;

		ax::NodeEditor::EditorContext *m_pContext;

		static CNodeEditor *g_pNodeEditor;
	};
};

#endif