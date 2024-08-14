#include "NodeBuilder.h"
#include "NodeTree.h"
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui/imgui_internal.h>
#include "NodeEditor.h"

namespace Valden {

static constexpr float s_TouchTime = 1.0f;
static SIREngine::CThreadAtomic<uint64_t> s_nGlobalID( 0 );

static inline uint64_t GetNextID( void )
{
	return s_nGlobalID.fetch_add( 1 );
}

static inline ax::NodeEditor::LinkId GetNextLinkID( void )
{
	return ax::NodeEditor::LinkId( GetNextID() );
}

static inline ImRect ImGui_GetItemRect()
{
	return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

static inline ImRect ImRect_Expanded(const ImRect& rect, float x, float y)
{
	auto result = rect;
	result.Min.x -= x;
	result.Min.y -= y;
	result.Max.x += x;
	result.Max.y += y;
	return result;
}
static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
	using namespace ImGui;
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	ImGuiID id = window->GetID("##Splitter");
	ImRect bb;
	bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
	bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
	return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

enum class EIconType: ImU32 {
	Flow,
	Circle,
	Square,
	Grid,
	RoundSquare,
	Diamond
};

void DrawIcon(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, EIconType type, bool filled, ImU32 color, ImU32 innerColor)
{
		  auto rect           = ImRect(a, b);
		  auto rect_x         = rect.Min.x;
		  auto rect_y         = rect.Min.y;
		  auto rect_w         = rect.Max.x - rect.Min.x;
		  auto rect_h         = rect.Max.y - rect.Min.y;
		  auto rect_center_x  = (rect.Min.x + rect.Max.x) * 0.5f;
		  auto rect_center_y  = (rect.Min.y + rect.Max.y) * 0.5f;
		  auto rect_center    = ImVec2(rect_center_x, rect_center_y);
	const auto outline_scale  = rect_w / 24.0f;
	const auto extra_segments = static_cast<int>(2 * outline_scale); // for full circle

	if (type == EIconType::Flow)
	{
		const auto origin_scale = rect_w / 24.0f;

		const auto offset_x  = 1.0f * origin_scale;
		const auto offset_y  = 0.0f * origin_scale;
		const auto margin     = (filled ? 2.0f : 2.0f) * origin_scale;
		const auto rounding   = 0.1f * origin_scale;
		const auto tip_round  = 0.7f; // percentage of triangle edge (for tip)
		//const auto edge_round = 0.7f; // percentage of triangle edge (for corner)
		const auto canvas = ImRect(
			rect.Min.x + margin + offset_x,
			rect.Min.y + margin + offset_y,
			rect.Max.x - margin + offset_x,
			rect.Max.y - margin + offset_y);
		const auto canvas_x = canvas.Min.x;
		const auto canvas_y = canvas.Min.y;
		const auto canvas_w = canvas.Max.x - canvas.Min.x;
		const auto canvas_h = canvas.Max.y - canvas.Min.y;

		const auto left   = canvas_x + canvas_w            * 0.5f * 0.3f;
		const auto right  = canvas_x + canvas_w - canvas_w * 0.5f * 0.3f;
		const auto top    = canvas_y + canvas_h            * 0.5f * 0.2f;
		const auto bottom = canvas_y + canvas_h - canvas_h * 0.5f * 0.2f;
		const auto center_y = (top + bottom) * 0.5f;
		//const auto angle = AX_PI * 0.5f * 0.5f * 0.5f;

		const auto tip_top    = ImVec2(canvas_x + canvas_w * 0.5f, top);
		const auto tip_right  = ImVec2(right, center_y);
		const auto tip_bottom = ImVec2(canvas_x + canvas_w * 0.5f, bottom);

		drawList->PathLineTo(ImVec2(left, top) + ImVec2(0, rounding));
		drawList->PathBezierCubicCurveTo(
			ImVec2(left, top),
			ImVec2(left, top),
			ImVec2(left, top) + ImVec2(rounding, 0));
		drawList->PathLineTo(tip_top);
		drawList->PathLineTo(tip_top + (tip_right - tip_top) * tip_round);
		drawList->PathBezierCubicCurveTo(
			tip_right,
			tip_right,
			tip_bottom + (tip_right - tip_bottom) * tip_round);
		drawList->PathLineTo(tip_bottom);
		drawList->PathLineTo(ImVec2(left, bottom) + ImVec2(rounding, 0));
		drawList->PathBezierCubicCurveTo(
			ImVec2(left, bottom),
			ImVec2(left, bottom),
			ImVec2(left, bottom) - ImVec2(0, rounding));

		if (!filled)
		{
			if (innerColor & 0xFF000000)
				drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

			drawList->PathStroke(color, true, 2.0f * outline_scale);
		}
		else
			drawList->PathFillConvex(color);
	}
	else
	{
		auto triangleStart = rect_center_x + 0.32f * rect_w;

		auto rect_offset = -static_cast<int>(rect_w * 0.25f * 0.25f);

		rect.Min.x    += rect_offset;
		rect.Max.x    += rect_offset;
		rect_x        += rect_offset;
		rect_center_x += rect_offset * 0.5f;
		rect_center.x += rect_offset * 0.5f;

		if (type == EIconType::Circle)
		{
			const auto c = rect_center;

			if (!filled)
			{
				const auto r = 0.5f * rect_w / 2.0f - 0.5f;

				if (innerColor & 0xFF000000)
					drawList->AddCircleFilled(c, r, innerColor, 12 + extra_segments);
				drawList->AddCircle(c, r, color, 12 + extra_segments, 2.0f * outline_scale);
			}
			else
			{
				drawList->AddCircleFilled(c, 0.5f * rect_w / 2.0f, color, 12 + extra_segments);
			}
		}

		if (type == EIconType::Square)
		{
			if (filled)
			{
				const auto r  = 0.5f * rect_w / 2.0f;
				const auto p0 = rect_center - ImVec2(r, r);
				const auto p1 = rect_center + ImVec2(r, r);

#if IMGUI_VERSION_NUM > 18101
				drawList->AddRectFilled(p0, p1, color, 0, ImDrawFlags_RoundCornersAll);
#else
				drawList->AddRectFilled(p0, p1, color, 0, 15);
#endif
			}
			else
			{
				const auto r = 0.5f * rect_w / 2.0f - 0.5f;
				const auto p0 = rect_center - ImVec2(r, r);
				const auto p1 = rect_center + ImVec2(r, r);

				if (innerColor & 0xFF000000)
				{
#if IMGUI_VERSION_NUM > 18101
					drawList->AddRectFilled(p0, p1, innerColor, 0, ImDrawFlags_RoundCornersAll);
#else
					drawList->AddRectFilled(p0, p1, innerColor, 0, 15);
#endif
				}

#if IMGUI_VERSION_NUM > 18101
				drawList->AddRect(p0, p1, color, 0, ImDrawFlags_RoundCornersAll, 2.0f * outline_scale);
#else
				drawList->AddRect(p0, p1, color, 0, 15, 2.0f * outline_scale);
#endif
			}
		}

		if (type == EIconType::Grid)
		{
			const auto r = 0.5f * rect_w / 2.0f;
			const auto w = ceilf(r / 3.0f);

			const auto baseTl = ImVec2(floorf(rect_center_x - w * 2.5f), floorf(rect_center_y - w * 2.5f));
			const auto baseBr = ImVec2(floorf(baseTl.x + w), floorf(baseTl.y + w));

			auto tl = baseTl;
			auto br = baseBr;
			for (int i = 0; i < 3; ++i)
			{
				tl.x = baseTl.x;
				br.x = baseBr.x;
				drawList->AddRectFilled(tl, br, color);
				tl.x += w * 2;
				br.x += w * 2;
				if (i != 1 || filled)
					drawList->AddRectFilled(tl, br, color);
				tl.x += w * 2;
				br.x += w * 2;
				drawList->AddRectFilled(tl, br, color);

				tl.y += w * 2;
				br.y += w * 2;
			}

			triangleStart = br.x + w + 1.0f / 24.0f * rect_w;
		}

		if (type == EIconType::RoundSquare)
		{
			if (filled)
			{
				const auto r  = 0.5f * rect_w / 2.0f;
				const auto cr = r * 0.5f;
				const auto p0 = rect_center - ImVec2(r, r);
				const auto p1 = rect_center + ImVec2(r, r);

#if IMGUI_VERSION_NUM > 18101
				drawList->AddRectFilled(p0, p1, color, cr, ImDrawFlags_RoundCornersAll);
#else
				drawList->AddRectFilled(p0, p1, color, cr, 15);
#endif
			}
			else
			{
				const auto r = 0.5f * rect_w / 2.0f - 0.5f;
				const auto cr = r * 0.5f;
				const auto p0 = rect_center - ImVec2(r, r);
				const auto p1 = rect_center + ImVec2(r, r);

				if (innerColor & 0xFF000000)
				{
#if IMGUI_VERSION_NUM > 18101
					drawList->AddRectFilled(p0, p1, innerColor, cr, ImDrawFlags_RoundCornersAll);
#else
					drawList->AddRectFilled(p0, p1, innerColor, cr, 15);
#endif
				}

#if IMGUI_VERSION_NUM > 18101
				drawList->AddRect(p0, p1, color, cr, ImDrawFlags_RoundCornersAll, 2.0f * outline_scale);
#else
				drawList->AddRect(p0, p1, color, cr, 15, 2.0f * outline_scale);
#endif
			}
		}
		else if (type == EIconType::Diamond)
		{
			if (filled)
			{
				const auto r = 0.607f * rect_w / 2.0f;
				const auto c = rect_center;

				drawList->PathLineTo(c + ImVec2( 0, -r));
				drawList->PathLineTo(c + ImVec2( r,  0));
				drawList->PathLineTo(c + ImVec2( 0,  r));
				drawList->PathLineTo(c + ImVec2(-r,  0));
				drawList->PathFillConvex(color);
			}
			else
			{
				const auto r = 0.607f * rect_w / 2.0f - 0.5f;
				const auto c = rect_center;

				drawList->PathLineTo(c + ImVec2( 0, -r));
				drawList->PathLineTo(c + ImVec2( r,  0));
				drawList->PathLineTo(c + ImVec2( 0,  r));
				drawList->PathLineTo(c + ImVec2(-r,  0));

				if (innerColor & 0xFF000000)
					drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

				drawList->PathStroke(color, true, 2.0f * outline_scale);
			}
		}
		else
		{
			const auto triangleTip = triangleStart + rect_w * (0.45f - 0.32f);

			drawList->AddTriangleFilled(
				ImVec2(ceilf(triangleTip), rect_y + rect_h * 0.5f),
				ImVec2(triangleStart, rect_center_y + 0.15f * rect_h),
				ImVec2(triangleStart, rect_center_y - 0.15f * rect_h),
				color);
		}
	}
}


static void DrawIcon( const ImVec2& size, EIconType type, bool filled, const ImVec4& color/* = ImVec4(1, 1, 1, 1)*/, const ImVec4& innerColor/* = ImVec4(0, 0, 0, 0)*/ )
{
	if (ImGui::IsRectVisible(size))
	{
		auto cursorPos = ImGui::GetCursorScreenPos();
		auto drawList  = ImGui::GetWindowDrawList();
		DrawIcon(drawList, cursorPos, cursorPos + size, type, filled, ImColor(color), ImColor(innerColor));
	}

	ImGui::Dummy(size);
}

ImColor GetIconColor( EPinType nType )
{
	switch ( nType ) {
	default:
	case EPinType::Flow:     return ImColor(255, 255, 255);
	case EPinType::Bool:     return ImColor(220,  48,  48);
	case EPinType::Int:      return ImColor( 68, 201, 156);
	case EPinType::Float:    return ImColor(147, 226,  74);
	case EPinType::String:   return ImColor(124,  21, 153);
	case EPinType::Object:   return ImColor( 51, 150, 215);
	case EPinType::Function: return ImColor(218,   0, 183);
	case EPinType::Delegate: return ImColor(255,  48,  48);
	};
}

void DrawPinIcon(const NodePin_t& pin, bool connected, int alpha)
{
	const int PinIconSize = 24;
	EIconType iconType;
	ImColor  color = GetIconColor(pin.Type);
	color.Value.w = alpha / 255.0f;
	switch (pin.Type) {
	case EPinType::Flow:     iconType = EIconType::Flow;   break;
	case EPinType::Bool:     iconType = EIconType::Circle; break;
	case EPinType::Int:      iconType = EIconType::Circle; break;
	case EPinType::Float:    iconType = EIconType::Circle; break;
	case EPinType::String:   iconType = EIconType::Circle; break;
	case EPinType::Object:   iconType = EIconType::Circle; break;
	case EPinType::Function: iconType = EIconType::Circle; break;
	case EPinType::Delegate: iconType = EIconType::Square; break;
	default:
		return;
	};

	DrawIcon(ImVec2(static_cast<float>(PinIconSize), static_cast<float>(PinIconSize)), iconType, connected, color, ImColor(32, 32, 32, alpha));
}

CBlueprintNodeBuilder::CBlueprintNodeBuilder( ImTextureID texture, int textureWidth, int textureHeight )
	: HeaderTextureId( texture ), HeaderTextureWidth( textureWidth ),
	HeaderTextureHeight( textureHeight ), CurrentNodeId( 0 ), CurrentStage( EStage::Invalid ), HasHeader( false )
{ }

void CBlueprintNodeBuilder::Begin( ax::NodeEditor::NodeId id )
{
	HasHeader  = false;
	HeaderMin = HeaderMax = ImVec2();

	ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

	ax::NodeEditor::BeginNode(id);
	CurrentNodeId = id;

	ImGui::PushID( id.AsPointer() );

	SetStage(EStage::Begin);
}

void CBlueprintNodeBuilder::End( void )
{
	SetStage( EStage::End );

	ax::NodeEditor::EndNode();

	if ( ImGui::IsItemVisible() ) {
		auto alpha = static_cast<int>( 255 * ImGui::GetStyle().Alpha );

		auto drawList = ax::NodeEditor::GetNodeBackgroundDrawList( CurrentNodeId );

		const auto halfBorderWidth = ax::NodeEditor::GetStyle().NodeBorderWidth * 0.5f;

		auto headerColor = IM_COL32(0, 0, 0, alpha) | (HeaderColor & IM_COL32(255, 255, 255, 0));
		if ((HeaderMax.x > HeaderMin.x) && (HeaderMax.y > HeaderMin.y) && HeaderTextureId ) {
			const auto uv = ImVec2(
				(HeaderMax.x - HeaderMin.x) / (float)(4.0f * HeaderTextureWidth),
				(HeaderMax.y - HeaderMin.y) / (float)(4.0f * HeaderTextureHeight));

			drawList->AddImageRounded(HeaderTextureId,
				HeaderMin - ImVec2(8 - halfBorderWidth, 4 - halfBorderWidth),
				HeaderMax + ImVec2(8 - halfBorderWidth, 0),
				ImVec2(0.0f, 0.0f), uv,
#if IMGUI_VERSION_NUM > 18101
				headerColor, ax::NodeEditor::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
#else
				headerColor, ax::NodeEditor::GetStyle().NodeRounding, 1 | 2);
#endif

			if (ContentMin.y > HeaderMax.y)
			{
				drawList->AddLine(
					ImVec2(HeaderMin.x - (8 - halfBorderWidth), HeaderMax.y - 0.5f),
					ImVec2(HeaderMax.x + (8 - halfBorderWidth), HeaderMax.y - 0.5f),
					ImColor(255, 255, 255, 96 * alpha / (3 * 255)), 1.0f);
			}
		}
	}

	CurrentNodeId = 0;

	ImGui::PopID();

	ax::NodeEditor::PopStyleVar();

	SetStage(EStage::Invalid);
}

void CBlueprintNodeBuilder::Header(const ImVec4& color)
{
	HeaderColor = ImColor(color);
	SetStage(EStage::Header);
}

void CBlueprintNodeBuilder::EndHeader()
{
	SetStage(EStage::Content);
}

void CBlueprintNodeBuilder::Input(ax::NodeEditor::PinId id)
{
	if (CurrentStage == EStage::Begin)
		SetStage(EStage::Content);

	const auto applyPadding = (CurrentStage == EStage::Input);

	SetStage(EStage::Input);

	if (applyPadding)
		ImGui::Spring(0);

	Pin(id, ax::NodeEditor::PinKind::Input);

	ImGui::BeginHorizontal(id.AsPointer());
}

void CBlueprintNodeBuilder::EndInput()
{
	ImGui::EndHorizontal();

	EndPin();
}

void CBlueprintNodeBuilder::Middle()
{
	if (CurrentStage == EStage::Begin)
		SetStage(EStage::Content);

	SetStage(EStage::Middle);
}

void CBlueprintNodeBuilder::Output(ax::NodeEditor::PinId id)
{
	if (CurrentStage == EStage::Begin)
		SetStage(EStage::Content);

	const auto applyPadding = (CurrentStage == EStage::Output);

	SetStage(EStage::Output);

	if (applyPadding)
		ImGui::Spring(0);

	Pin(id, ax::NodeEditor::PinKind::Output);

	ImGui::BeginHorizontal(id.AsPointer());
}

void CBlueprintNodeBuilder::EndOutput( void )
{
	ImGui::EndHorizontal();

	EndPin();
}

bool CBlueprintNodeBuilder::SetStage( EStage stage )
{
	if ( stage == CurrentStage ) {
		return false;
	}

	auto oldStage = CurrentStage;
	CurrentStage = stage;

	ImVec2 cursor;
	switch (oldStage) {
		case EStage::Begin:
			break;

		case EStage::Header:
			ImGui::EndHorizontal();
			HeaderMin = ImGui::GetItemRectMin();
			HeaderMax = ImGui::GetItemRectMax();

			// spacing between header and content
			ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.y * 2.0f);

			break;

		case EStage::Content:
			break;

		case EStage::Input:
			ax::NodeEditor::PopStyleVar(2);

			ImGui::Spring(1, 0);
			ImGui::EndVertical();

			// #debug
			// ImGui::GetWindowDrawList()->AddRect(
			//     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

			break;

		case EStage::Middle:
			ImGui::EndVertical();

			// #debug
			// ImGui::GetWindowDrawList()->AddRect(
			//     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

			break;

		case EStage::Output:
			ax::NodeEditor::PopStyleVar(2);

			ImGui::Spring(1, 0);
			ImGui::EndVertical();

			// #debug
			// ImGui::GetWindowDrawList()->AddRect(
			//     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

			break;

		case EStage::End:
			break;

		case EStage::Invalid:
			break;
	}

	switch (stage)
	{
		case EStage::Begin:
			ImGui::BeginVertical( "node" );
			break;

		case EStage::Header:
			HasHeader = true;

			ImGui::BeginHorizontal( "header" );
			break;

		case EStage::Content:
			if (oldStage == EStage::Begin)
				ImGui::Spring(0);

			ImGui::BeginHorizontal("content");
			ImGui::Spring(0, 0);
			break;

		case EStage::Input:
			ImGui::BeginVertical("inputs", ImVec2(0, 0), 0.0f);

			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotSize, ImVec2(0, 0));

			if (!HasHeader)
				ImGui::Spring(1, 0);
			break;

		case EStage::Middle:
			ImGui::Spring(1);
			ImGui::BeginVertical("middle", ImVec2(0, 0), 1.0f);
			break;

		case EStage::Output:
			if (oldStage == EStage::Middle || oldStage == EStage::Input)
				ImGui::Spring(1);
			else
				ImGui::Spring(1, 0);
			ImGui::BeginVertical("outputs", ImVec2(0, 0), 1.0f);

			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
			ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PivotSize, ImVec2(0, 0));

			if (!HasHeader)
				ImGui::Spring(1, 0);
			break;

		case EStage::End:
			if (oldStage == EStage::Input)
				ImGui::Spring(1, 0);
			if (oldStage != EStage::Begin)
				ImGui::EndHorizontal();
			ContentMin = ImGui::GetItemRectMin();
			ContentMax = ImGui::GetItemRectMax();

			//ImGui::Spring(0);
			ImGui::EndVertical();
			NodeMin = ImGui::GetItemRectMin();
			NodeMax = ImGui::GetItemRectMax();
			break;

		case EStage::Invalid:
			break;
	}

	return true;
}

void CBlueprintNodeBuilder::Pin(ax::NodeEditor::PinId id, ax::NodeEditor::PinKind kind)
{
	ax::NodeEditor::BeginPin(id, kind);
}

void CBlueprintNodeBuilder::EndPin()
{
	ax::NodeEditor::EndPin();

	// #debug
	// ImGui::GetWindowDrawList()->AddRectFilled(
	//     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 64));
}


void CNodeTree::BuildNode( Node_t *pNode )
{
	for ( auto& it : pNode->Inputs ) {
		it.Node = pNode;
		it.Kind = EPinKind::Input;
	}
	for ( auto& it : pNode->Outputs ) {
		it.Node = pNode;
		it.Kind = EPinKind::Output;
	}
}

void CNodeTree::BuildNodes( void )
{
	for ( auto& it : m_NodePool ) {
		BuildNode( &it );
	}
}

void CNodeTree::TouchNode(  ax::NodeEditor::NodeId id )
{
	m_NodeTouchTime[id] = s_TouchTime;
}

float CNodeTree::GetTouchProgress( ax::NodeEditor::NodeId id )
{
	auto it = m_NodeTouchTime.find( id );
	if ( it != m_NodeTouchTime.end() && it->second > 0.0f ) {
		return ( s_TouchTime - it->second ) / s_TouchTime;
	} else {
		return 0.0f;
	}
}

void CNodeTree::UpdateTouch( void )
{
	const auto deltaTime = ImGui::GetIO().DeltaTime;
	for ( auto& entry : m_NodeTouchTime ) {
		if ( entry.second > 0.0f ) {
			entry.second -= deltaTime;
		}
	}
}

Node_t *CNodeTree::FindNode( ax::NodeEditor::NodeId id )
{
	for ( auto& node : m_NodePool ) {
		if ( node.nID == id ) {
			return &node;
		}
	}
	return nullptr;
}

NodeLink_t *CNodeTree::FindLink( ax::NodeEditor::LinkId id )
{
	for ( auto& link : m_LinkPool ) {
		if ( link.nID == id ) {
			return &link;
		}
	}
	return nullptr;
}

NodePin_t *CNodeTree::FindPin( ax::NodeEditor::PinId id )
{
	if ( !id ) {
		return nullptr;
	}
	for ( auto& node : m_NodePool ) {
		for ( auto& pin : node.Inputs ) {
			if ( pin.nID == id ) {
				return &pin;
			}
		}
		for ( auto& pin : node.Outputs ) {
			if ( pin.nID == id ) {
				return &pin;
			}
		}
	}
	return nullptr;
}

Node_t *CNodeTree::GetLinkedNode( ax::NodeEditor::PinId id )
{
	if ( !id ) {
		return NULL;
	}
	for ( auto& it : m_LinkPool ) {
		if ( it.EndPinID == id ) {
			ax::NodeEditor::PinId start, end;

			ax::NodeEditor::GetLinkPins( it.nID, &start, &end );

			for ( auto& node : m_NodePool ) {
				for ( auto& pin : node.Outputs ) {
					if ( pin.nID == start ) {
						return &node;
					}
				}
			}
		}
	}
	return NULL;
}

bool CNodeTree::IsPinLinked( ax::NodeEditor::PinId id )
{
	if ( !id ) {
		return false;
	}
	for ( auto& link : m_LinkPool ) {
		if ( link.StartPinID == id || link.EndPinID == id ) {
			return true;
		}
	}
	return false;
}

bool CNodeTree::CanCreateLink( NodePin_t *a, NodePin_t *b )
{
	if ( !a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->Node == b->Node ) {
		return false;
	}
	return true;
}

void CNodeTree::Draw( void )
{
	static ax::NodeEditor::NodeId contextNodeId = 0;
	static ax::NodeEditor::LinkId contextLinkId = 0;
	static ax::NodeEditor::PinId  contextPinId = 0;
	static bool createNewNode = false;
	static NodePin_t *newNodeLinkPin = NULL;
	static NodePin_t *newLinkPin = NULL;

	static float leftPaneWidth  = 400.0f;
	static float rightPaneWidth = 800.0f;
	Splitter( true, 4.0f, &leftPaneWidth, &rightPaneWidth, 50.0f, 50.0f );

	ImGui::SameLine( 0.0f, 12.0f );
	{
		auto cursorTopLeft = ImGui::GetCursorScreenPos();

		CBlueprintNodeBuilder builder( NULL, 0, 0 );

		for ( auto& node : m_NodePool ) {
			if ( node.nType != ENodeType::Blueprint && node.nType != ENodeType::Simple ) {
				continue;
			}

			const auto isSimple = node.nType == ENodeType::Simple;

			bool hasOutputDelegates = false;

			for (auto& output : node.Outputs) {
				if (output.Type == EPinType::Delegate) {
					hasOutputDelegates = true;
				}

				ImGui::PushID( output.nID.AsPointer() );
				builder.Begin(node.nID);
				ImGui::PopID();

				if (!isSimple) {
					builder.Header(node.Color);
					ImGui::Spring(0);
					ImGui::TextUnformatted(node.Name.c_str());
					ImGui::Spring(1);
					ImGui::Dummy(ImVec2(0, 28));
					if (hasOutputDelegates) {
						ImGui::BeginVertical("delegates", ImVec2(0, 28));
						ImGui::Spring(1, 0);
						for (auto& output : node.Outputs) {
							if (output.Type != EPinType::Delegate) {
								continue;
							}

							auto alpha = ImGui::GetStyle().Alpha;
							if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin) {
								alpha = alpha * (48.0f / 255.0f);
							}

							ax::NodeEditor::BeginPin(output.nID, ax::NodeEditor::PinKind::Output);
							ax::NodeEditor::PinPivotAlignment(ImVec2(1.0f, 0.5f));
							ax::NodeEditor::PinPivotSize(ImVec2(0, 0));
							ImGui::BeginHorizontal(output.nID.AsPointer());
							ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
							if (!output.Name.empty())
							{
								ImGui::TextUnformatted(output.Name.c_str());
								ImGui::Spring(0);
							}
							DrawPinIcon(output, IsPinLinked(output.nID), (int)(alpha * 255));
							ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
							ImGui::EndHorizontal();
							ImGui::PopStyleVar();
							ax::NodeEditor::EndPin();

							//DrawItemRect(ImColor(255, 0, 0));
						}
						ImGui::Spring(1, 0);
						ImGui::EndVertical();
						ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.x / 2);
					}
					else {
						ImGui::Spring(0);
					}
					builder.EndHeader();
				}

				for (auto& input : node.Inputs) {
					auto alpha = ImGui::GetStyle().Alpha;
					if (newLinkPin && !CanCreateLink(newLinkPin, &input) && &input != newLinkPin)
						alpha = alpha * (48.0f / 255.0f);

					builder.Input(input.nID);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
					DrawPinIcon(input, IsPinLinked(input.nID), (int)(alpha * 255));
					ImGui::Spring(0);
					if (!input.Name.empty())
					{
						ImGui::TextUnformatted(input.Name.c_str());
						ImGui::Spring(0);
					}
					if ( input.Type == EPinType::Bool ) {
						const Node_t *linked = GetLinkedNode( input.nID );
						if ( linked ) {
							ImGui::Button( linked->Name.c_str() );
						} else {
							// set to a constant
							ImGui::Button( "True" );
						}
						ImGui::Spring( 0 );
					}
					ImGui::PopStyleVar();
					builder.EndInput();
				}

				if (isSimple)
				{
					builder.Middle();

					ImGui::Spring(1, 0);
					ImGui::TextUnformatted(node.Name.c_str());
					ImGui::Spring(1, 0);
				}

				for (auto& output : node.Outputs)
				{
					if (!isSimple && output.Type == EPinType::Delegate)
						continue;
						
					auto alpha = ImGui::GetStyle().Alpha;
					if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
						alpha = alpha * (48.0f / 255.0f);

					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
					builder.Output(output.nID);
					if (output.Type == EPinType::String)
					{
						static char buffer[128] = "Edit Me\nMultiline!";
						static bool wasActive = false;
						ImGui::PushItemWidth(100.0f);
						ImGui::InputText("##edit", buffer, 127);
						ImGui::PopItemWidth();
						if (ImGui::IsItemActive() && !wasActive)
						{
							ax::NodeEditor::EnableShortcuts(false);
							wasActive = true;
						}
						else if (!ImGui::IsItemActive() && wasActive)
						{
							ax::NodeEditor::EnableShortcuts(true);
							wasActive = false;
						}
						ImGui::Spring(0);
					}
					if (!output.Name.empty())
					{
						ImGui::Spring(0);
						ImGui::TextUnformatted(output.Name.c_str());
					}
					ImGui::Spring(0);
					DrawPinIcon(output, IsPinLinked(output.nID), (int)(alpha * 255));
					ImGui::PopStyleVar();
					builder.EndOutput();
				}
				builder.End();
			}

			for (auto& node : m_NodePool)
			{
				if (node.nType != ENodeType::Tree)
					continue;

				const float rounding = 5.0f;
				const float padding  = 12.0f;

				const auto pinBackground = ax::NodeEditor::GetStyle().Colors[ax::NodeEditor::StyleColor_NodeBg];

				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg,        ImColor(128, 128, 128, 200));
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBorder,    ImColor( 32,  32,  32, 200));
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_PinRect,       ImColor( 60, 180, 255, 150));
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_PinRectBorder, ImColor( 60, 180, 255, 150));

				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodePadding,  ImVec4(0, 0, 0, 0));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeRounding, rounding);
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_SourceDirection, ImVec2(0.0f,  1.0f));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_TargetDirection, ImVec2(0.0f, -1.0f));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_LinkStrength, 0.0f);
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinBorderWidth, 1.0f);
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinRadius, 5.0f);
				ax::NodeEditor::BeginNode(node.nID);

				ImGui::BeginVertical(node.nID.AsPointer());
				ImGui::BeginHorizontal("inputs");
				ImGui::Spring(0, padding * 2);

				ImRect inputsRect;
				int inputAlpha = 200;
				if (!node.Inputs.empty())
				{
						auto& pin = node.Inputs[0];
						ImGui::Dummy(ImVec2(0, padding));
						ImGui::Spring(1, 0);
						inputsRect = ImGui_GetItemRect();

						ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinArrowSize, 10.0f);
						ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinArrowWidth, 10.0f);
#if IMGUI_VERSION_NUM > 18101
						ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinCorners, ImDrawFlags_RoundCornersBottom);
#else
						ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinCorners, 12);
#endif
						ax::NodeEditor::BeginPin(pin.nID, ax::NodeEditor::PinKind::Input);
						ax::NodeEditor::PinPivotRect(inputsRect.GetTL(), inputsRect.GetBR());
						ax::NodeEditor::PinRect(inputsRect.GetTL(), inputsRect.GetBR());
						ax::NodeEditor::EndPin();
						ax::NodeEditor::PopStyleVar(3);

						if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
							inputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
				}
				else
					ImGui::Dummy(ImVec2(0, padding));

				ImGui::Spring(0, padding * 2);
				ImGui::EndHorizontal();

				ImGui::BeginHorizontal("content_frame");
				ImGui::Spring(1, padding);

				ImGui::BeginVertical("content", ImVec2(0.0f, 0.0f));
				ImGui::Dummy(ImVec2(160, 0));
				ImGui::Spring(1);
				ImGui::TextUnformatted(node.Name.c_str());
				ImGui::Spring(1);
				ImGui::EndVertical();
				auto contentRect = ImGui_GetItemRect();

				ImGui::Spring(1, padding);
				ImGui::EndHorizontal();

				ImGui::BeginHorizontal("outputs");
				ImGui::Spring(0, padding * 2);

				ImRect outputsRect;
				int outputAlpha = 200;
				if (!node.Outputs.empty())
				{
					auto& pin = node.Outputs[0];
					ImGui::Dummy(ImVec2(0, padding));
					ImGui::Spring(1, 0);
					outputsRect = ImGui_GetItemRect();

#if IMGUI_VERSION_NUM > 18101
					ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinCorners, ImDrawFlags_RoundCornersTop);
#else
					ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinCorners, 3);
#endif
					ax::NodeEditor::BeginPin(pin.nID, ax::NodeEditor::PinKind::Output);
					ax::NodeEditor::PinPivotRect(outputsRect.GetTL(), outputsRect.GetBR());
					ax::NodeEditor::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
					ax::NodeEditor::EndPin();
					ax::NodeEditor::PopStyleVar();

					if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
						outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
				}
				else
					ImGui::Dummy(ImVec2(0, padding));

				ImGui::Spring(0, padding * 2);
				ImGui::EndHorizontal();

				ImGui::EndVertical();

				ax::NodeEditor::EndNode();
				ax::NodeEditor::PopStyleVar(7);
				ax::NodeEditor::PopStyleColor(4);

				auto drawList = ax::NodeEditor::GetNodeBackgroundDrawList(node.nID);

				//const auto fringeScale = ImGui::GetStyle().AntiAliasFringeScale;
				//const auto unitSize    = 1.0f / fringeScale;

				//const auto ImDrawList_AddRect = [](ImDrawList* drawList, const ImVec2& a, const ImVec2& b, ImU32 col, float rounding, int rounding_corners, float thickness)
				//{
				//    if ((col >> 24) == 0)
				//        return;
				//    drawList->PathRect(a, b, rounding, rounding_corners);
				//    drawList->PathStroke(col, true, thickness);
				//};

#if IMGUI_VERSION_NUM > 18101
				const auto    topRoundCornersFlags = ImDrawFlags_RoundCornersTop;
				const auto bottomRoundCornersFlags = ImDrawFlags_RoundCornersBottom;
#else
				const auto    topRoundCornersFlags = 1 | 2;
				const auto bottomRoundCornersFlags = 4 | 8;
#endif

				drawList->AddRectFilled(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
					IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, bottomRoundCornersFlags);
				//ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
				drawList->AddRect(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
					IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, bottomRoundCornersFlags);
				//ImGui::PopStyleVar();
				drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
					IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, topRoundCornersFlags);
				//ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
				drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
					IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, topRoundCornersFlags);
				//ImGui::PopStyleVar();
				drawList->AddRectFilled(contentRect.GetTL(), contentRect.GetBR(), IM_COL32(24, 64, 128, 200), 0.0f);
				//ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
				drawList->AddRect(
					contentRect.GetTL(),
					contentRect.GetBR(),
					IM_COL32(48, 128, 255, 100), 0.0f);
				//ImGui::PopStyleVar();
			}

/*
			for (auto& node : m_NodePool)
			{
				if (node.nType != ENodeType::Houdini)
					continue;

				const float rounding = 10.0f;
				const float padding  = 12.0f;


				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg,        ImColor(229, 229, 229, 200));
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBorder,    ImColor(125, 125, 125, 200));
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_PinRect,       ImColor(229, 229, 229, 60));
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_PinRectBorder, ImColor(125, 125, 125, 60));

				const auto pinBackground = ax::NodeEditor::GetStyle().Colors[ax::NodeEditor::StyleColor_NodeBg];

				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodePadding,  ImVec4(0, 0, 0, 0));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_NodeRounding, rounding);
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_SourceDirection, ImVec2(0.0f,  1.0f));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_TargetDirection, ImVec2(0.0f, -1.0f));
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_LinkStrength, 0.0f);
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinBorderWidth, 1.0f);
				ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinRadius, 6.0f);
				ax::NodeEditor::BeginNode(node.nID);

				ImGui::BeginVertical(node.nID.AsPointer());
				if (!node.Inputs.empty())
				{
					ImGui::BeginHorizontal("inputs");
					ImGui::Spring(1, 0);

					ImRect inputsRect;
					int inputAlpha = 200;
					for (auto& pin : node.Inputs)
					{
						ImGui::Dummy(ImVec2(padding, padding));
						inputsRect = ImGui_GetItemRect();
						ImGui::Spring(1, 0);
						inputsRect.Min.y -= padding;
						inputsRect.Max.y -= padding;

#if IMGUI_VERSION_NUM > 18101
						const auto allRoundCornersFlags = ImDrawFlags_RoundCornersAll;
#else
						const auto allRoundCornersFlags = 15;
#endif
						//ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinArrowSize, 10.0f);
						//ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinArrowWidth, 10.0f);
						ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinCorners, allRoundCornersFlags);

						ax::NodeEditor::BeginPin(pin.nID, ax::NodeEditor::PinKind::Input);
						ax::NodeEditor::PinPivotRect(inputsRect.GetCenter(), inputsRect.GetCenter());
						ax::NodeEditor::PinRect(inputsRect.GetTL(), inputsRect.GetBR());
						ax::NodeEditor::EndPin();
						//ax::NodeEditor::PopStyleVar(3);
						ax::NodeEditor::PopStyleVar(1);

						auto drawList = ImGui::GetWindowDrawList();
						drawList->AddRectFilled(inputsRect.GetTL(), inputsRect.GetBR(),
							IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, allRoundCornersFlags);
						drawList->AddRect(inputsRect.GetTL(), inputsRect.GetBR(),
							IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, allRoundCornersFlags);

						if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
							inputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
					}

					//ImGui::Spring(1, 0);
					ImGui::EndHorizontal();
				}

				ImGui::BeginHorizontal("content_frame");
				ImGui::Spring(1, padding);

				ImGui::BeginVertical("content", ImVec2(0.0f, 0.0f));
				ImGui::Dummy(ImVec2(160, 0));
				ImGui::Spring(1);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
				ImGui::TextUnformatted(node.Name.c_str());
				ImGui::PopStyleColor();
				ImGui::Spring(1);
				ImGui::EndVertical();
				auto contentRect = ImGui_GetItemRect();

				ImGui::Spring(1, padding);
				ImGui::EndHorizontal();

				if (!node.Outputs.empty())
				{
					ImGui::BeginHorizontal("outputs");
					ImGui::Spring(1, 0);

					ImRect outputsRect;
					int outputAlpha = 200;
					for (auto& pin : node.Outputs)
					{
						ImGui::Dummy(ImVec2(padding, padding));
						outputsRect = ImGui_GetItemRect();
						ImGui::Spring(1, 0);
						outputsRect.Min.y += padding;
						outputsRect.Max.y += padding;

#if IMGUI_VERSION_NUM > 18101
						const auto allRoundCornersFlags = ImDrawFlags_RoundCornersAll;
						const auto topRoundCornersFlags = ImDrawFlags_RoundCornersTop;
#else
						const auto allRoundCornersFlags = 15;
						const auto topRoundCornersFlags = 3;
#endif

						ax::NodeEditor::PushStyleVar(ax::NodeEditor::StyleVar_PinCorners, topRoundCornersFlags);
						ax::NodeEditor::BeginPin(pin.nID, ax::NodeEditor::PinKind::Output);
						ax::NodeEditor::PinPivotRect(outputsRect.GetCenter(), outputsRect.GetCenter());
						ax::NodeEditor::PinRect(outputsRect.GetTL(), outputsRect.GetBR());
						ax::NodeEditor::EndPin();
						ax::NodeEditor::PopStyleVar();


						auto drawList = ImGui::GetWindowDrawList();
						drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR(),
							IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, allRoundCornersFlags);
						drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR(),
							IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, allRoundCornersFlags);


						if (newLinkPin && !CanCreateLink(newLinkPin, &pin) && &pin != newLinkPin)
							outputAlpha = (int)(255 * ImGui::GetStyle().Alpha * (48.0f / 255.0f));
					}

					ImGui::EndHorizontal();
				}

				ImGui::EndVertical();

				ax::NodeEditor::EndNode();
				ax::NodeEditor::PopStyleVar(7);
				ax::NodeEditor::PopStyleColor(4);

				// auto drawList = ax::NodeEditor::GetNodeBackgroundDrawList(node.nID);

				//const auto fringeScale = ImGui::GetStyle().AntiAliasFringeScale;
				//const auto unitSize    = 1.0f / fringeScale;

				//const auto ImDrawList_AddRect = [](ImDrawList* drawList, const ImVec2& a, const ImVec2& b, ImU32 col, float rounding, int rounding_corners, float thickness)
				//{
				//    if ((col >> 24) == 0)
				//        return;
				//    drawList->PathRect(a, b, rounding, rounding_corners);
				//    drawList->PathStroke(col, true, thickness);
				//};

				//drawList->AddRectFilled(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
				//    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 12);
				//ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
				//drawList->AddRect(inputsRect.GetTL() + ImVec2(0, 1), inputsRect.GetBR(),
				//    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), inputAlpha), 4.0f, 12);
				//ImGui::PopStyleVar();
				//drawList->AddRectFilled(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
				//    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 3);
				////ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
				//drawList->AddRect(outputsRect.GetTL(), outputsRect.GetBR() - ImVec2(0, 1),
				//    IM_COL32((int)(255 * pinBackground.x), (int)(255 * pinBackground.y), (int)(255 * pinBackground.z), outputAlpha), 4.0f, 3);
				////ImGui::PopStyleVar();
				//drawList->AddRectFilled(contentRect.GetTL(), contentRect.GetBR(), IM_COL32(24, 64, 128, 200), 0.0f);
				//ImGui::PushStyleVar(ImGuiStyleVar_AntiAliasFringeScale, 1.0f);
				//drawList->AddRect(
				//    contentRect.GetTL(),
				//    contentRect.GetBR(),
				//    IM_COL32(48, 128, 255, 100), 0.0f);
				//ImGui::PopStyleVar();
			}

			for (auto& node : m_NodePool)
			{
				if (node.Type != NodeType::Comment)
					continue;

				const float commentAlpha = 0.75f;

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha);
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBg, ImColor(255, 255, 255, 64));
				ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBorder, ImColor(255, 255, 255, 64));
				ax::NodeEditor::BeginNode(node.nID);
				ImGui::PushID(node.nID.AsPointer());
				ImGui::BeginVertical("content");
				ImGui::BeginHorizontal("horizontal");
				ImGui::Spring(1);
				ImGui::TextUnformatted(node.Name.c_str());
				ImGui::Spring(1);
				ImGui::EndHorizontal();
				ax::NodeEditor::Group(node.Size);
				ImGui::EndVertical();
				ImGui::PopID();
				ax::NodeEditor::EndNode();
				ax::NodeEditor::PopStyleColor(2);
				ImGui::PopStyleVar();

				if (ax::NodeEditor::BeginGroupHint(node.nID))
				{
					//auto alpha   = static_cast<int>(commentAlpha * ImGui::GetStyle().Alpha * 255);
					auto bgAlpha = static_cast<int>(ImGui::GetStyle().Alpha * 255);

					//ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha * ImGui::GetStyle().Alpha);

					auto min = ax::NodeEditor::GetGroupMin();
					//auto max = ax::NodeEditor::GetGroupMax();

					ImGui::SetCursorScreenPos(min - ImVec2(-8, ImGui::GetTextLineHeightWithSpacing() + 4));
					ImGui::BeginGroup();
					ImGui::TextUnformatted(node.Name.c_str());
					ImGui::EndGroup();

					auto drawList = ax::NodeEditor::GetHintBackgroundDrawList();

					auto hintBounds      = ImGui_GetItemRect();
					auto hintFrameBounds = ImRect_Expanded(hintBounds, 8, 4);

					drawList->AddRectFilled(
						hintFrameBounds.GetTL(),
						hintFrameBounds.GetBR(),
						IM_COL32(255, 255, 255, 64 * bgAlpha / 255), 4.0f);

					drawList->AddRect(
						hintFrameBounds.GetTL(),
						hintFrameBounds.GetBR(),
						IM_COL32(255, 255, 255, 128 * bgAlpha / 255), 4.0f);

					//ImGui::PopStyleVar();
				}
				ax::NodeEditor::EndGroupHint();
			}
*/
			for (auto& link : m_LinkPool) {
				ax::NodeEditor::Link(link.nID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
			}

			if (!createNewNode)
			{
				if (ax::NodeEditor::BeginCreate(ImColor(255, 255, 255), 2.0f))
				{
					auto showLabel = [](const char* label, ImColor color)
					{
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
						auto size = ImGui::CalcTextSize(label);

						auto padding = ImGui::GetStyle().FramePadding;
						auto spacing = ImGui::GetStyle().ItemSpacing;

						ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

						auto rectMin = ImGui::GetCursorScreenPos() - padding;
						auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

						auto drawList = ImGui::GetWindowDrawList();
						drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
						ImGui::TextUnformatted(label);
					};

					ax::NodeEditor::PinId startPinId = 0, endPinId = 0;
					if (ax::NodeEditor::QueryNewLink(&startPinId, &endPinId))
					{
						auto startPin = FindPin(startPinId);
						auto endPin   = FindPin(endPinId);

						newLinkPin = startPin ? startPin : endPin;

						if (startPin->Kind == EPinKind::Input)
						{
							eastl::swap(startPin, endPin);
							eastl::swap(startPinId, endPinId);
						}

						if (startPin && endPin)
						{
							if (endPin == startPin)
							{
								ax::NodeEditor::RejectNewItem(ImColor(255, 0, 0), 2.0f);
							}
							else if (endPin->Kind == startPin->Kind)
							{
								showLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
								ax::NodeEditor::RejectNewItem(ImColor(255, 0, 0), 2.0f);
							}
							//else if (endPin->Node == startPin->Node)
							//{
							//    showLabel("x Cannot connect to self", ImColor(45, 32, 32, 180));
							//    ax::NodeEditor::RejectNewItem(ImColor(255, 0, 0), 1.0f);
							//}
							else if (endPin->Type != startPin->Type)
							{
								showLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
								ax::NodeEditor::RejectNewItem(ImColor(255, 128, 128), 1.0f);
							}
							else
							{
								showLabel("+ Create Link", ImColor(32, 45, 32, 180));
								if (ax::NodeEditor::AcceptNewItem(ImColor(128, 255, 128), 4.0f))
								{
									m_LinkPool.emplace_back(NodeLink_t(GetNextID(), startPinId, endPinId));
									m_LinkPool.back().Color = GetIconColor(startPin->Type);
								}
							}
						}
					}

					ax::NodeEditor::PinId pinId = 0;
					if (ax::NodeEditor::QueryNewNode(&pinId))
					{
						newLinkPin = FindPin(pinId);
						if (newLinkPin)
							showLabel("+ Create Node", ImColor(32, 45, 32, 180));

						if (ax::NodeEditor::AcceptNewItem())
						{
							createNewNode  = true;
							newNodeLinkPin = FindPin(pinId);
							newLinkPin = nullptr;
							ax::NodeEditor::Suspend();
							ImGui::OpenPopup("Create New Node");
							ax::NodeEditor::Resume();
						}
					}
				}
				else {
					newLinkPin = nullptr;
				}

				ax::NodeEditor::EndCreate();

				if (ax::NodeEditor::BeginDelete())
				{
					ax::NodeEditor::NodeId nodeId = 0;
					while (ax::NodeEditor::QueryDeletedNode(&nodeId))
					{
						if (ax::NodeEditor::AcceptDeletedItem()) {
							auto id = eastl::find_if(m_NodePool.begin(), m_NodePool.end(), [nodeId](auto& node) { return node.nID == nodeId; });
							if (id != m_NodePool.end()) {
								m_NodePool.erase(id);
							}
						}
					}

					ax::NodeEditor::LinkId linkId = 0;
					while (ax::NodeEditor::QueryDeletedLink(&linkId)) {
						if (ax::NodeEditor::AcceptDeletedItem()) {
							auto id = eastl::find_if(m_LinkPool.begin(), m_LinkPool.end(), [linkId](auto& link) { return link.nID == linkId; });
							if (id != m_LinkPool.end()) {
								m_LinkPool.erase(id);
							}
						}
					}
				}
				ax::NodeEditor::EndDelete();
			}

			ImGui::SetCursorScreenPos(cursorTopLeft);
		}
	}

	auto openPopupPosition = ImGui::GetMousePos();
	ax::NodeEditor::Suspend();

	if (ax::NodeEditor::ShowNodeContextMenu(&contextNodeId)) {
		ImGui::OpenPopup("Node Context Menu");
	} else if (ax::NodeEditor::ShowPinContextMenu(&contextPinId)) {
		ImGui::OpenPopup("Pin Context Menu");
	} else if (ax::NodeEditor::ShowLinkContextMenu(&contextLinkId)) {
		ImGui::OpenPopup("Link Context Menu");
	} else if (ax::NodeEditor::ShowBackgroundContextMenu()) {
		ImGui::OpenPopup("Create New Node");
		newNodeLinkPin = nullptr;
	}
	ax::NodeEditor::Resume();

	ax::NodeEditor::Suspend();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	if (ImGui::BeginPopup("Node Context Menu")) {
		Node_t *node = FindNode(contextNodeId);

		ImGui::TextUnformatted("Node Context Menu");
		ImGui::Separator();
		if (node) {
			ImGui::Text("ID: %p", node->nID.AsPointer());
			ImGui::Text("Type: %s", node->nType == ENodeType::Blueprint ? "Blueprint" : (node->nType == ENodeType::Tree ? "Tree" : "Comment"));
			ImGui::Text("Inputs: %d", (int)node->Inputs.size());
			ImGui::Text("Outputs: %d", (int)node->Outputs.size());
		}
		else {
			ImGui::Text("Unknown node: %p", contextNodeId.AsPointer());
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Delete")) {
			ax::NodeEditor::DeleteNode(contextNodeId);
			m_NodePool.erase( node );
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Pin Context Menu")) {
		NodePin_t *pin = FindPin(contextPinId);

		ImGui::TextUnformatted("Pin Context Menu");
		ImGui::Separator();
		if (pin) {
			ImGui::Text("ID: %p", pin->nID.AsPointer());
			if (pin->Node) {
				ImGui::Text("Node: %p", pin->Node->nID.AsPointer());
			} else {
				ImGui::Text("Node: %s", "<none>");
			}
		}
		else {
			ImGui::Text("Unknown pin: %p", contextPinId.AsPointer());
		}

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Link Context Menu")) {
		NodeLink_t *link = FindLink(contextLinkId);

		ImGui::TextUnformatted("Link Context Menu");
		ImGui::Separator();
		if ( link ) {
			ImGui::Text("ID: %p", link->nID.AsPointer());
			ImGui::Text("From: %p", link->StartPinID.AsPointer());
			ImGui::Text("To: %p", link->EndPinID.AsPointer());
		}
		else {
			ImGui::Text("Unknown link: %p", contextLinkId.AsPointer());
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Delete")) {
			ax::NodeEditor::DeleteLink(contextLinkId);
			m_LinkPool.erase( link );
		}
		ImGui::EndPopup();
	}

	if ( ImGui::BeginPopup( "Create New Node" ) ) {
		auto newNodePostion = openPopupPosition;
		//ImGui::SetCursorScreenPos(ImGui::GetMousePosOnOpeningCurrentPopup());

		//auto drawList = ImGui::GetWindowDrawList();
		//drawList->AddCircleFilled(ImGui::GetMousePosOnOpeningCurrentPopup(), 10.0f, 0xFFFF00FF);

		Node_t *node = CNodeEditor::Get()->DrawNodeTree();

		if ( node ) {
			CreateNode( node, newNodePostion, newLinkPin );
		}

		ImGui::EndPopup();
	}
	else {
		createNewNode = false;
	}
	ImGui::PopStyleVar();
	ax::NodeEditor::Resume();


	/*
		cubic_bezier_t c;
		c.p0 = pointf(100, 600);
		c.p1 = pointf(300, 1200);
		c.p2 = pointf(500, 100);
		c.p3 = pointf(900, 600);

		auto drawList = ImGui::GetWindowDrawList();
		auto offset_radius = 15.0f;
		auto acceptPoint = [drawList, offset_radius](const bezier_subdivide_result_t& r)
		{
			drawList->AddCircle(to_imvec(r.point), 4.0f, IM_COL32(255, 0, 255, 255));

			auto nt = r.tangent.normalized();
			nt = pointf(-nt.y, nt.x);

			drawList->AddLine(to_imvec(r.point), to_imvec(r.point + nt * offset_radius), IM_COL32(255, 0, 0, 255), 1.0f);
		};

		drawList->AddBezierCurve(to_imvec(c.p0), to_imvec(c.p1), to_imvec(c.p2), to_imvec(c.p3), IM_COL32(255, 255, 255, 255), 1.0f);
		cubic_bezier_subdivide(acceptPoint, c);
	*/

	ImVec2 editorMin = ImGui::GetItemRectMin();
	ImVec2 editorMax = ImGui::GetItemRectMax();

	if ( false ) {
		int nodeCount = ax::NodeEditor::GetNodeCount();
		eastl::vector<ax::NodeEditor::NodeId> orderedNodeIds;
		orderedNodeIds.resize(static_cast<size_t>(nodeCount));
		ax::NodeEditor::GetOrderedNodeIds(orderedNodeIds.data(), nodeCount);


		ImDrawList *drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(editorMin, editorMax);

		int ordinal = 0;
		for ( auto& nodeId : orderedNodeIds ) {
			ImVec2 p0 = ax::NodeEditor::GetNodePosition(nodeId);
			ImVec2 p1 = p0 + ax::NodeEditor::GetNodeSize(nodeId);
			p0 = ax::NodeEditor::CanvasToScreen(p0);
			p1 = ax::NodeEditor::CanvasToScreen(p1);


			ImGuiTextBuffer builder;
			builder.appendf("#%d", ordinal++);

			ImVec2 textSize   = ImGui::CalcTextSize(builder.c_str());
			ImVec2 padding    = ImVec2(2.0f, 2.0f);
			ImVec2 widgetSize = textSize + padding * 2;

			ImVec2 widgetPosition = ImVec2(p1.x, p0.y) + ImVec2(0.0f, -widgetSize.y);

			drawList->AddRectFilled(widgetPosition, widgetPosition + widgetSize, IM_COL32(100, 80, 80, 190), 3.0f, ImDrawFlags_RoundCornersAll);
			drawList->AddRect(widgetPosition, widgetPosition + widgetSize, IM_COL32(200, 160, 160, 190), 3.0f, ImDrawFlags_RoundCornersAll);
			drawList->AddText(widgetPosition + padding, IM_COL32(255, 255, 255, 255), builder.c_str());
		}

		drawList->PopClipRect();
	}
}

void CNodeTree::CreateNode( Node_t *pNode, const ImVec2& position, NodePin_t *linkPin )
{
	BuildNodes();
	ax::NodeEditor::SetNodePosition( pNode->nID, position );
	if ( NodePin_t *startPin = linkPin ) {
		eastl::vector<NodePin_t>& pins = startPin->Kind == EPinKind::Input ? pNode->Outputs : pNode->Inputs;
		for ( auto& pin : pins ) {
			if ( CanCreateLink( startPin, &pin ) ) {
				NodePin_t *endPin = &pin;
				if ( startPin->Kind == EPinKind::Input ) {
					eastl::swap( startPin, endPin );
				}
				m_LinkPool.emplace_back( NodeLink_t( GetNextID(), startPin->nID, endPin->nID ) );
				m_LinkPool.back().Color = GetIconColor( startPin->Type );
				break;
			}
		}
	}
}

Node_t *CNodeTree::SpawnBranchNode( void )
{
	m_NodePool.emplace_back( GetNextID(), "Branch" );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "", EPinType::Flow );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Condition", EPinType::Bool );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "True", EPinType::Flow );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "False", EPinType::Flow );

	BuildNode( &m_NodePool.back() );

	ax::NodeEditor::SetNodePosition( m_NodePool.back().nID, ImVec2( 0, 0 ) );

	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnDoNNode( void )
{
	m_NodePool.emplace_back( GetNextID(), "Do N" );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Enter", EPinType::Flow );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "N", EPinType::Int );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Loop Body", EPinType::Delegate );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Exit", EPinType::Flow );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Counter", EPinType::Int );

	BuildNode( &m_NodePool.back() );

	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnForLoopWithBreakNode( void )
{
	m_NodePool.emplace_back( GetNextID(), "For Loop with Break" );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Enter", EPinType::Flow );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Start Index", EPinType::Int );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Stop Index", EPinType::Int );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Break Condition", EPinType::Bool );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Counter", EPinType::Int );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Loop Body", EPinType::Function );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Exit", EPinType::Flow );

	BuildNode( &m_NodePool.back() );

	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnForEachNode( void )
{
	m_NodePool.emplace_back( GetNextID(), "For Each" );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Enter", EPinType::Flow );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Container", EPinType::Object );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Index", EPinType::Object );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Loop Body", EPinType::Delegate );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Exit", EPinType::Flow );

	BuildNode( &m_NodePool.back() );

	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnWhileLoopNode( void )
{
	m_NodePool.emplace_back( GetNextID(), "While Loop" );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Enter", EPinType::Flow );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Condition", EPinType::Bool );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Loop Body", EPinType::Delegate );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Exit", EPinType::Flow );

	BuildNode( &m_NodePool.back() );

	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnDelayNode( void )
{
	m_NodePool.emplace_back( GetNextID(), "Delay" );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Enter", EPinType::Flow );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Time", EPinType::Int );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Exit", EPinType::Flow );

	BuildNode( &m_NodePool.back() );

	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnDelayWhile( void )
{
	m_NodePool.emplace_back( GetNextID(), "Delay While" );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Enter", EPinType::Flow );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Condition", EPinType::Bool );
	m_NodePool.back().Inputs.emplace_back( GetNextID(), "Time", EPinType::Int );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "Exit", EPinType::Flow );

	BuildNode( &m_NodePool.back() );

	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnGetVariableBooleanNode( const ScriptName_t& varName )
{
	uint64_t id = CNodeEditor::Get()->GetVariableNodeID( varName );
	if ( !id ) {
		id = GetNextID();
		CNodeEditor::Get()->SetVariableNodeID( varName, id );
	}

	m_NodePool.emplace_back( id, varName.c_str() );
	m_NodePool.back().Outputs.emplace_back( id, "", EPinType::Bool );

	m_NodePool.back().nType = ENodeType::Simple;
	BuildNode( &m_NodePool.back() );
	
	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnGetVariableIntNode( void )
{
	const uint64_t id = GetNextID();
	m_NodePool.emplace_back( id, "Get Int" );
	m_NodePool.back().Outputs.emplace_back( id, "", EPinType::Int );

	BuildNode( &m_NodePool.back() );
	
	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnGetVariableFloatNode( void )
{
	m_NodePool.emplace_back( GetNextID(), "Get Float" );
	m_NodePool.back().Outputs.emplace_back( GetNextID(), "", EPinType::Float );

	BuildNode( &m_NodePool.back() );
	
	return &m_NodePool.back();
}

Node_t *CNodeTree::SpawnEventOnInitNode( void )
{
}

Node_t *CNodeTree::SpawnEventOnShutdownNode( void )
{
}

Node_t *CNodeTree::SpawnEventOnFrameNode( void )
{
}

Node_t *CNodeTree::SpawnEventOnSaveGameNode( void )
{
}

Node_t *CNodeTree::SpawnEventOnLoadGameNode( void )
{
}

Node_t *CNodeTree::SpawnAddNode( void )
{
}

Node_t *CNodeTree::SpawnSubNode( void )
{
}

Node_t *CNodeTree::SpawnMultiplyNode( void )
{
}

Node_t *CNodeTree::SpawnDivideNode( void )
{
}

};