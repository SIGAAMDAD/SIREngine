#ifndef __VALDEN_NODE_TREE_H__
#define __VALDEN_NODE_TREE_H__

#pragma once

#include <Engine/Memory/Memory.h>
#include <Engine/Core/ThreadSystem/Thread.h>
#include <EASTL/map.h>

namespace Valden {
	struct NodeIdLess {
	    inline bool operator()( const ax::NodeEditor::NodeId& lhs, const ax::NodeEditor::NodeId& rhs ) const
		{ return lhs.AsPointer() < rhs.AsPointer(); }
	};

	class CNodeTree
	{
	public:
		CNodeTree( void )
		{ }
		~CNodeTree()
		{ }

		void BuildNode( Node_t *pNode );
		void BuildNodes( void );

		Node_t *SpawnBranchNode( void );
		Node_t *SpawnDoNNode( void );
		Node_t *SpawnForLoopWithBreakNode( void );
		Node_t *SpawnForEachNode( void );
		Node_t *SpawnWhileLoopNode( void );
		Node_t *SpawnDelayNode( void );
		Node_t *SpawnDelayWhile( void );

		Node_t *SpawnGetVariableBooleanNode( const ScriptName_t& varName );
		Node_t *SpawnGetVariableIntNode( void );
		Node_t *SpawnGetVariableFloatNode( void );

		Node_t *SpawnEventOnInitNode( void );
		Node_t *SpawnEventOnShutdownNode( void );
		Node_t *SpawnEventOnFrameNode( void );
		Node_t *SpawnEventOnSaveGameNode( void );
		Node_t *SpawnEventOnLoadGameNode( void );

		Node_t *SpawnAddNode( void );
		Node_t *SpawnSubNode( void );
		Node_t *SpawnMultiplyNode( void );
		Node_t *SpawnDivideNode( void );

		void CreateNode( Node_t *pNode, const ImVec2& position, NodePin_t *pin );
		void Draw( void );

		void TouchNode(  ax::NodeEditor::NodeId id );
		float GetTouchProgress( ax::NodeEditor::NodeId id );
		void UpdateTouch( void );
		Node_t *FindNode( ax::NodeEditor::NodeId id );
		NodeLink_t *FindLink( ax::NodeEditor::LinkId id );
		NodePin_t *FindPin( ax::NodeEditor::PinId id );
		bool IsPinLinked( ax::NodeEditor::PinId id );
		bool CanCreateLink( NodePin_t *a, NodePin_t *b );
		Node_t *GetLinkedNode( ax::NodeEditor::PinId id );
	private:
		NodeVariable_t *m_pSelectedVar;

		eastl::vector<Node_t> m_NodePool;
		eastl::vector<NodeLink_t> m_LinkPool;
		eastl::map<ax::NodeEditor::NodeId, float, NodeIdLess> m_NodeTouchTime;
	};
};

#endif