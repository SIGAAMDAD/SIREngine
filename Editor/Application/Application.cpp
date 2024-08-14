#define IMGUI_UNLIMITED_FRAME_RATE

#include "Application.h"
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/imgui_internal.h>
#include <Engine/Core/Util.h>
#include "TextEditor.h"
#include "Project/ProjectManager.h"
#include "SceneView.h"
#include <Engine/Core/Events/EventManager.h>
#include <string.h>
#include <Engine/RenderLib/RenderLib.h>
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <Engine/RenderLib/Backend/OpenGL/GLContext.h>
#include <GLFW/glfw3.h>

#include "Roboto-Regular.embed"

namespace ImGui {

struct InputTextCallback_UserData
{
    eastl::string*            Str;
    ImGuiInputTextCallback  ChainCallback;
    void*                   ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        eastl::string* str = user_data->Str;
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }
    else if (user_data->ChainCallback)
    {
        // Forward to user callback, if any
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

bool InputText(const char* label, eastl::string *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

bool InputTextMultiline(const char* label, eastl::string *str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
}

bool InputTextWithHint(const char* label, const char* hint, eastl::string *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputTextWithHint(label, hint, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

};

namespace Valden {

CEditorApplication CEditorApplication::g_Application;

void CEditorApplication::Init( void )
{
	SIRENGINE_LOG( "Initializing Editor Instance..." );

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;		// Enable Keyboard Controls
//	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;		// Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;			// Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
//	io.ConfigViewportsNoAutoMerge = false;
//	io.ConfigViewportsNoTaskBarIcon = false;
	io.WantSaveIniSettings = true;
	io.IniFilename = "Valden/Config/ImGuiSettings.ini";
	SIRENGINE_LOG( "ImGuiContext Created." );

	ImGuiStyle& style = ImGui::GetStyle();
	if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
		style.WindowRounding = 0.0f;
		style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
	}

//	ImGui_ImplGlfw_InitForOpenGL( SIREngine::RenderLib::Backend::GetRenderContext()->GetWindowHandle(), true );
	ImGui_ImplSDL2_InitForOpenGL( SIREngine::RenderLib::Backend::GetRenderContext()->GetWindowHandle(),
		dynamic_cast<SIREngine::RenderLib::Backend::OpenGL::GLContext *>( SIREngine::RenderLib::Backend::GetRenderContext() )->GetInternalContext() );
	ImGui_ImplOpenGL3_Init( "#version 330" );
	SIRENGINE_LOG( "ImGui OpenGL3 & SDL2 backend allocated." );

	ImGui::StyleColorsDark();

	// Load default font
	ImFontConfig fontConfig;
	fontConfig.FontDataOwnedByAtlas = false;
	ImFont *robotoFont = io.Fonts->AddFontFromMemoryTTF( (void *)g_RobotoRegular, sizeof( g_RobotoRegular ), 20.0f, &fontConfig );
	io.FontDefault = robotoFont;

	ImGui::GetIO().Fonts->Build();

	CTextEditorManager::Init();
	CProjectManager::Init();
	CSceneView::Init();
}

void CEditorApplication::Frame( int64_t msec )
{
	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize.x = SIREngine::Application::Get()->GetAppInfo().nWindowWidth;
	io.DisplaySize.y = SIREngine::Application::Get()->GetAppInfo().nWindowHeight;
	io.DisplayFramebufferScale = io.DisplaySize;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	{
		ImVec2 size;
		const ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
		const ImGuiViewport *viewport = ImGui::GetMainViewport();

		size = { viewport->WorkSize.x, viewport->WorkSize.y };
		//size = { viewport->WorkSize.x / 2.0f, viewport->WorkSize.y };

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;

		ImGui::SetNextWindowPos( viewport->WorkPos );
		ImGui::SetNextWindowSize( viewport->WorkSize );
		ImGui::SetNextWindowViewport( viewport->ID );
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", nullptr, window_flags);
		ImGui::SetWindowSize( viewport->Size );
		ImGui::PopStyleVar();
		
		size = ImGui::GetWindowSize();

		ImGui::PopStyleVar( 2 );

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if ( io.ConfigFlags & ImGuiConfigFlags_DockingEnable ) {
			ImGuiID dockspace_id = ImGui::GetID( "VulkanAppDockspace" );
			ImGui::DockSpace( dockspace_id, ImVec2( 0.0f, 0.0f ), dockspace_flags );

			ImGuiDockNode *pNode = (ImGuiDockNode *)ImGui::GetCurrentContext()->DockContext.Nodes.GetVoidPtr( dockspace_id );
			if ( ImGui::DockNodeBeginAmendTabBar( pNode ) ) {
				if ( ImGui::BeginTabItem( "Test" ) ) {
					ImGui::EndTabItem();
				}
				ImGui::DockNodeEndAmendTabBar();
			}
		}
		ImGui::End();
	}

	for ( auto& it : m_Widgets ) {
		it->Draw();
	}

	ImGui::BeginMainMenuBar();
	if ( ImGui::BeginMenu( "File" ) ) {
		if ( ImGui::MenuItem( "Open Script" ) ) {
			
		}
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Edit" ) ) {
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Project" ) ) {
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Help" ) ) {
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();

	ImGui::Render();
	ImDrawData *pDrawData = ImGui::GetDrawData();
	bool bMainIsMinimized = ( pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f );
	if ( !bMainIsMinimized ) {
		ImGui_ImplOpenGL3_RenderDrawData( pDrawData );
	}

	if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void CEditorApplication::Shutdown( void )
{
	m_Widgets.clear();

	CProjectManager::SaveCache();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

};