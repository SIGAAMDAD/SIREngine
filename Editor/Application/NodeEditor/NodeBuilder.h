#ifndef __VALDEN_NODE_TYPES_H__
#define __VALDEN_NODE_TYPES_H__

#pragma once

#include <Engine/Memory/Memory.h>
#include <Engine/Core/Events/KeyCodes.h>
#include <EASTL/string.h>
#include "NodeVariable.h"
#include <imgui-node-editor/imgui_node_editor.h>
#include <EASTL/vector.h>

namespace Valden {
	class CNodeTree;

	enum class ENodeType {
		Blueprint,
		Simple,
		Tree,
		Comment,
	};
	
	enum class EPinType {
		Flow,
		Bool,
		Int,
		Float,
		String,
		Object,
		Function,
		Delegate,
	};

	enum class EPinKind {
		Output,
		Input
	};

	typedef struct Node Node_t;
	typedef struct NodePin {
		NodePin( uint64_t nId, const char *name, EPinType type )
			: nID( nId ), Node( NULL ), Name( name ), Type( type ), Kind( EPinKind::Input )
		{ }

		ax::NodeEditor::PinId nID;
		Node_t *Node;
		eastl::string Name;
		EPinType Type;
		EPinKind Kind;
	} NodePin_t;

	typedef struct NodeLink {
		NodeLink( ax::NodeEditor::LinkId nId, ax::NodeEditor::PinId startPinId, ax::NodeEditor::PinId endPinId )
			: nID( nId ), StartPinID( startPinId ), EndPinID( endPinId ), Color( 255, 255, 255 )
		{ }

		ax::NodeEditor::LinkId nID;
		ax::NodeEditor::PinId StartPinID;
		ax::NodeEditor::PinId EndPinID;
		ImColor Color;
	} NodeLink_t;

	typedef struct Node {
		Node( uint64_t nId, const char *name, ImColor color = ImColor(255, 255, 255))
			: nID( nId ), Name( name ), Color( color ), nType( ENodeType::Blueprint ), nSize( 0, 0 )
		{ }

		ax::NodeEditor::NodeId nID;
		eastl::string Name;
		eastl::vector<NodePin_t> Inputs;
		eastl::vector<NodePin_t> Outputs;
		ImColor Color;
		ENodeType nType;
		ImVec2 nSize;
	
		eastl::string State;
		eastl::string SavedState;
	} Node_t;

	class CBlueprintNodeBuilder
	{
	public:
	    CBlueprintNodeBuilder( ImTextureID texture = nullptr, int textureWidth = 0, int textureHeight = 0 );

	    void Begin( ax::NodeEditor::NodeId id );
	    void End( void );

	    void Header( const ImVec4& color = ImVec4( 1, 1, 1, 1 ) );
	    void EndHeader( void );

	    void Input( ax::NodeEditor::PinId id );
	    void EndInput( void );

	    void Middle( void );

	    void Output( ax::NodeEditor::PinId id );
	    void EndOutput( void );
	private:
	    enum class EStage {
	        Invalid,
	        Begin,
	        Header,
	        Content,
	        Input,
	        Output,
	        Middle,
	        End
	    };

	    bool SetStage( EStage stage );

	    void Pin( ax::NodeEditor::PinId id, ax::NodeEditor::PinKind kind );
	    void EndPin( void );

	    ImTextureID HeaderTextureId;
	    int HeaderTextureWidth;
	    int HeaderTextureHeight;
	    ax::NodeEditor::NodeId CurrentNodeId;
	    EStage CurrentStage;
	    ImU32 HeaderColor;
	    ImVec2 NodeMin;
	    ImVec2 NodeMax;
	    ImVec2 HeaderMin;
	    ImVec2 HeaderMax;
	    ImVec2 ContentMin;
	    ImVec2 ContentMax;
	    bool HasHeader;
	};
};

#endif