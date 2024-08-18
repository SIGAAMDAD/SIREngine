#define IMGUI_UNLIMITED_FRAME_RATE

#include "Application.h"
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/imgui_internal.h>
#include <Engine/Core/Util.h>
#include "ScriptLib/ScriptTextEditor.h"
#include "Project/ProjectManager.h"
#include "StatsWindow.h"
#include "SceneView.h"
#include "ScriptLib/ScriptCompiler.h"
#include "ContentBrowser/ContentBrowser.h"
#include <Engine/Core/Events/EventManager.h>
#include <string.h>
#include <Engine/RenderLib/RenderLib.h>
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <Engine/RenderLib/Backend/OpenGL/GLContext.h>
#include <libnotify/notify.h>
#include <implot/implot.h>

#include "Roboto-Regular.embed"

namespace ImGui {

struct InputTextCallback_UserData
{
	CString*            Str;
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
        CString* str = user_data->Str;
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

bool InputText(const char* label, CString *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

bool InputTextMultiline(const char* label, CString *str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputTextMultiline(label, (char*)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback, &cb_user_data);
}

bool InputTextWithHint(const char* label, const char* hint, CString *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
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

typedef struct {
	uint32_t nKey0;
	uint32_t nKey1;
	eastl::function<void()> Func;
} ShortCut_t;

static CHashMap<CString, ShortCut_t> s_Shortcuts;

CVar<uint32_t> AutoSaveTime(
	"valden.AutoSaveTime",
	100000,
	Cvar_Save,
	"Sets the amount of time between editor autosaves",
	CVG_USERINFO
);
CVar<bool> AutoSaveEnabled(
	"valden.AutoSaveEnabled",
	true,
	Cvar_Save,
	"Enables editor autosaving",
	CVG_USERINFO
);

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

	ImGui_ImplSDL2_InitForOpenGL( RenderLib::Backend::GetRenderContext()->GetWindowHandle(),
		dynamic_cast<RenderLib::Backend::OpenGL::GLContext *>( RenderLib::Backend::GetRenderContext() )->GetInternalContext() );
	ImGui_ImplOpenGL3_Init( "#version 330" );
	SIRENGINE_LOG( "ImGui OpenGL3 & SDL2 backend allocated." );

	ImGui::StyleColorsDark();

	// Load default font
	ImFontConfig fontConfig;
	fontConfig.FontDataOwnedByAtlas = false;
	ImFont *robotoFont = io.Fonts->AddFontFromMemoryTTF( (void *)g_RobotoRegular, sizeof( g_RobotoRegular ), 20.0f, &fontConfig );
	io.FontDefault = robotoFont;

	float iconFontSize = 32.0f * 2.0f / 3.0f;

	{
		static const ImWchar szIconRanges[] = { ICON_MIN_FA_EDITOR, ICON_MAX_FA_EDITOR, 0 };
		fontConfig.MergeMode = true;
		fontConfig.PixelSnapH = true;
		fontConfig.GlyphMinAdvanceX = iconFontSize;
		io.Fonts->AddFontFromFileTTF( "IconsFontAwesomeEditor.ttf", iconFontSize, &fontConfig, szIconRanges );
	}
	{
		static const ImWchar szIconRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
		fontConfig.MergeMode = true;
		fontConfig.PixelSnapH = true;
		fontConfig.GlyphMinAdvanceX = iconFontSize;
		io.Fonts->AddFontFromFileTTF( FONT_ICON_FILE_NAME_FAS, iconFontSize, &fontConfig, szIconRanges );
	}
	ImGui::GetIO().Fonts->Build();

	g_pFileSystem->AddCacheDirectory( "Valden/Bitmaps" );

	CTextEditorManager::Init();
	CProjectManager::Init();
	CSceneView::Init();
	ContentBrowser::CContentBrowser::Init();
	CScriptCompiler::Get().Init();

	s_Shortcuts.reserve( 4 );
	s_Shortcuts[ "Save" ] =			{ .nKey0 = ImGuiKey_S, .nKey1 = 0, .Func = [this]( void ) -> void { SaveProject(); } };
	s_Shortcuts[ "Save All" ] = 	{ .nKey0 = ImGuiKey_S, .nKey1 = ImGuiKey_LeftShift, .Func = [this]( void ) -> void { SaveProject(); } };
	s_Shortcuts[ "New Level" ] =	{ .nKey0 = ImGuiKey_N, .nKey1 = 0, .Func = [this]( void ) -> void {  } };
	s_Shortcuts[ "Open Level" ] =	{ .nKey0 = ImGuiKey_O, .nKey1 = 0, .Func = [this]( void ) -> void {  } };
	
	ImPlot::CreateContext();

	AddWidget( &CStatsWindow::Get() );
}

static void CheckAutoSave( void )
{
	if ( !AutoSaveEnabled.GetValue() ) {
		return;
	}

	static time_t nStart = 0;
	time_t now;

	time( &now );

	if ( CProjectManager::Get()->GetProject()->IsModified() && !nStart ) {
		nStart = now;
		return;
	}

	if ( ( now - nStart ) > ( 60 * AutoSaveTime.GetValue() ) && CProjectManager::Get()->GetProject()->IsModified() ) {
		SIRENGINE_LOG( "Beginning AutoSave..." );
		ImGui::InsertNotification( { ImGuiToastType::Info, 1000, "Autosaving..." } );
		CEditorApplication::Get().SaveProject();
		SIRENGINE_LOG( "AutoSave Completed." );

		nStart = now;
	}
}

void CEditorApplication::Frame( int64_t msec )
{
	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize.x = Application::Get()->GetAppInfo().nWindowWidth;
	io.DisplaySize.y = Application::Get()->GetAppInfo().nWindowHeight;
	io.DisplayFramebufferScale = io.DisplaySize;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::PushStyleColor( ImGuiCol_FrameBg,
		ImVec4( style.Colors[ ImGuiCol_FrameBg ].x,
				style.Colors[ ImGuiCol_FrameBg ].y,
				style.Colors[ ImGuiCol_FrameBg ].z,
				1.0f ) );
	
	ImGui::PushStyleColor( ImGuiCol_FrameBgActive,
		ImVec4( style.Colors[ ImGuiCol_FrameBgActive ].x,
				style.Colors[ ImGuiCol_FrameBgActive ].y,
				style.Colors[ ImGuiCol_FrameBgActive ].z,
				1.0f ) );
	
	ImGui::PushStyleColor( ImGuiCol_FrameBgHovered,
		ImVec4( style.Colors[ ImGuiCol_FrameBgHovered ].x,
				style.Colors[ ImGuiCol_FrameBgHovered ].y,
				style.Colors[ ImGuiCol_FrameBgHovered ].z,
				1.0f ) );
	
	ImGui::PushStyleColor( ImGuiCol_WindowBg,
		ImVec4( style.Colors[ ImGuiCol_WindowBg ].x,
				style.Colors[ ImGuiCol_WindowBg ].y,
				style.Colors[ ImGuiCol_WindowBg ].z,
				1.0f ) );

	CheckAutoSave();

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
		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if ( dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode ) {
			window_flags |= ImGuiWindowFlags_NoBackground;
		}

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
		ImGui::Begin( "DockSpace Demo", nullptr, window_flags );
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

	DrawMainMenuBar();
	DrawBottomMenu();
	DrawViewport();

	//
	// check for shortcuts
	//
	if ( ImGui::IsKeyDown( ImGuiKey_LeftCtrl ) ) {
		for ( auto it = s_Shortcuts.cbegin(); it != s_Shortcuts.end(); it++ ) {
			if ( ImGui::IsKeyDown( (ImGuiKey)it->second.nKey0 ) ) {
				if ( it->second.nKey1 != 0 ) {
					if ( ImGui::IsKeyDown( (ImGuiKey)it->second.nKey1 ) ) {
						it->second.Func();
					}
				} else {
					it->second.Func();
				}
			}
		}
	}

	ImGui::RenderNotifications();

	ImGui::PopStyleColor( 4 );

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

void CEditorApplication::DrawEditorSettings( void )
{
	if ( !m_bEditorSettingsActive ) {
		return;
	}

	if ( ImGui::Begin( "Valden Preferences", &m_bEditorSettingsActive, ImGuiWindowFlags_NoCollapse ) ) {
		if ( ImGui::BeginTable( "##ValdenPreferencesCategoriesTable", 2 ) ) {
			ImGui::EndTable();
		}
		ImGui::End();
	}
}

void CEditorApplication::DrawProjectSettings( void )
{
	static int nSelectedPreference = 0;
	if ( !m_bProjectSettingsActive ) {
		return;
	}

	if ( ImGui::Begin( "Project Settings", &m_bProjectSettingsActive, ImGuiWindowFlags_NoCollapse ) ) {
		if ( ImGui::BeginTable( "##ProjectPreferencesCategoriesTable", 2 ) ) {
			
			ImGui::TableNextColumn();

			if ( ImGui::CollapsingHeader( "Scripting" ) ) {
				ImGui::Indent( 0.75f );
				if ( ImGui::MenuItem( "Compiler" ) ) {
					nSelectedPreference = 0;
				}
				ImGui::Unindent( 0.75f );
			}

			if ( ImGui::CollapsingHeader( "Graphics" ) ) {
				ImGui::Indent( 0.75f );
				if ( ImGui::MenuItem( "Performance" ) ) {
					nSelectedPreference = 2;
				}
				if ( ImGui::MenuItem( "Memory" ) ) {
					nSelectedPreference = 3;
				}
				ImGui::Unindent( 0.75f );
			}

			ImGui::TableNextColumn();
			
			switch ( nSelectedPreference ) {
			case 0: { // General

				break; }
			case 1: { // Graphics:Performance
				ImGui::BeginTable( "##GraphicsQualityPreferencesTable", 2 );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Anti Aliasing" );

				ImGui::TableNextColumn();
				ImGui::TextUnformatted( "Anisotropic Filtering" );

				ImGui::TableNextColumn();

				ImGui::EndTable();

				ImGui::BeginTable( "##GraphicsQualityPreferencesSelectTable", 4 );

				ImGui::EndTable();
				break; }
			case 2: { // Graphics:Memory
				if ( ImGui::SliderInt( "GPU Hardware Buffer Max Size", NULL, 0, 10*1024*1024 ) ) {

				}
				break; }
			};

			ImGui::EndTable();
		}
		ImGui::End();
	}
}

void CEditorApplication::DrawMainMenuBar( void )
{
	ImGui::BeginMainMenuBar();
	ImGui::SetWindowFontScale( ImGui::GetFont()->Scale * 1.20f );
	if ( ImGui::BeginMenu( "File" ) ) {
		
		ImGui::SeparatorText( "Open" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_FOLDER_PLUS "New Scene", "CTRL+N" ) ) {
		}
		if ( ImGui::MenuItem( ICON_FA_FOLDER_OPEN "Open Scene" ) ) {
		}
		ImGui::Unindent( 0.5f );


		ImGui::SeparatorText( "Save" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_SAVE "Save Scene", "CTRL+S" ) ) {
		}
		if ( ImGui::MenuItem( ICON_FA_SAVE "Save Scene As...", "CTRL+SHIFT+S" ) ) {
		}
		if ( ImGui::MenuItem( "Save All", "CTRL+ALT+S" ) ) {
		}
		ImGui::Unindent( 0.5f );

		ImGui::SeparatorText( "Project" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_FOLDER_PLUS "New Project" ) ) {
		}
		if ( ImGui::MenuItem( ICON_FA_FOLDER_OPEN "Open Project" ) ) {
		}
		if ( ImGui::BeginMenu( "Recent Projects" ) ) {
			if ( m_RecentProjects.empty() ) {
				ImGui::MenuItem( "No Recent Projects" );
			} else {
				for ( auto& it : m_RecentProjects ) {
					if ( ImGui::MenuItem( it.c_str() ) ) {

					}
				}
			}
			ImGui::EndMenu();
		}
		ImGui::Unindent( 0.5f );

		ImGui::SeparatorText( "Import/Export" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_FILE_IMPORT "Import Into Scene..." ) ) {
		}
		if ( ImGui::MenuItem( ICON_FA_FILE_EXPORT "Export All..." ) ) {
		}
		ImGui::Unindent( 0.5f );

		ImGui::SeparatorText( "Exit" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_CROSS "Exit" ) ) {
			Application::Get()->Shutdown();
		}
		ImGui::Unindent( 0.5f );

		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Edit" ) ) {
		ImGui::SeparatorText( "Edit" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_CUT "Cut", "CTRL+X" ) ) {
		}
		if ( ImGui::MenuItem( ICON_FA_COPY "Copy", "CTRL+C" ) ) {
		}
		if ( ImGui::MenuItem( ICON_FA_PASTE "Paste", "CTRL+V" ) ) {
		}
		ImGui::Unindent( 0.5f );

		ImGui::SeparatorText( "Configuration" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON__GEAR "Editor Settings" ) ) {
			m_bEditorSettingsActive = true;
		}
		if ( ImGui::MenuItem( ICON__GEAR "Project Settings" ) ) {
			m_bProjectSettingsActive = true;
		}
		if ( ImGui::MenuItem( "Plugins" ) ) {
		}
		ImGui::Unindent( 0.5f );

		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Project" ) ) {
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "View" ) ) {
		if ( ImGui::MenuItem( "Statistics" ) ) {
			CStatsWindow::Get().SetActive();
		}
		ImGui::EndMenu();
	}
	if ( ImGui::BeginMenu( "Help" ) ) {
		ImGui::EndMenu();
	}
	ImGui::SetWindowFontScale( 1.0f );
	ImGui::EndMainMenuBar();
}

void CEditorApplication::DrawViewport( void )
{
	ImGui::Begin( "##ValdenMainViewportTopBar", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove );
	if ( ImGui::BeginMenuBar() ) {
		if ( ImGui::MenuItem( ICON_FA_SAVE, "CTRL+S" ) ) {

		}
		if ( ImGui::BeginMenu( ICON_FA_PLUS_SQUARE ) ) {
			if ( ImGui::MenuItem( "Add Script Class" ) ) {
				CScriptCompiler::Get().AddClass();
			}
			ImGui::EndMenu();
		}
		if ( ImGui::MenuItem( ICON_FA_PLAY ) ) {

		}
		if ( ImGui::MenuItem( ICON_FA_ANGLE_DOUBLE_RIGHT ) ) {

		}
		if ( ImGui::MenuItem( ICON_FA_PAUSE ) ) {

		}
		ImGui::EndMenuBar();
	}
	ImGui::End();

	ImGui::Begin( "##Valden" );
	ImGui::End();
}

void CEditorApplication::DrawBottomMenu( void )
{
	ImGui::Begin( "##BottomMenuWidget", NULL, ImGuiWindowFlags_NoCollapse );

	if ( ImGui::BeginTable( "##BottomMenuWidgetTable", 3 ) ) {
		ImGui::TableNextColumn();
		if ( ImGui::MenuItem( "Command Console" ) ) {

		}

		ImGui::TableNextColumn();
		if ( ImGui::MenuItem( "Log Output" ) ) {

		}

		ImGui::TableNextColumn();
		if ( ImGui::MenuItem( "Content Browser" ) ) {
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void CEditorApplication::DockWindowLeft( const char *pWindowLabel )
{
	ImGuiID nDockID = ImGui::DockSpaceOverViewport( ImGui::GetWindowDockID(), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode );

	ImGui::DockBuilderRemoveNode( nDockID );
	ImGui::DockBuilderAddNode( nDockID );
	ImGui::DockBuilderSetNodeSize( nDockID, ImGui::GetMainViewport()->Size );
	
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Left, 0.5f, &m_nLeftDockID, &m_nRightDockID );
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Up, 0.5f, &m_nTopDockID, &m_nBottomDockID );
	ImGui::DockBuilderDockWindow( pWindowLabel, m_nLeftDockID );
}

void CEditorApplication::DockWindowRight( const char *pWindowLabel )
{
	ImGuiID nDockID = ImGui::DockSpaceOverViewport( ImGui::GetWindowDockID(), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode );

	ImGui::DockBuilderRemoveNode( nDockID );
	ImGui::DockBuilderAddNode( nDockID );
	ImGui::DockBuilderSetNodeSize( nDockID, ImGui::GetMainViewport()->Size );
	
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Left, 0.5f, &m_nLeftDockID, &m_nRightDockID );
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Up, 0.5f, &m_nTopDockID, &m_nBottomDockID );
	ImGui::DockBuilderDockWindow( pWindowLabel, m_nRightDockID );
}

void CEditorApplication::DockWindowTop( const char *pWindowLabel )
{
	ImGuiID nDockID = ImGui::DockSpaceOverViewport( ImGui::GetWindowDockID(), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode );

	ImGui::DockBuilderRemoveNode( nDockID );
	ImGui::DockBuilderAddNode( nDockID );
	ImGui::DockBuilderSetNodeSize( nDockID, ImGui::GetMainViewport()->Size );
	
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Left, 0.5f, &m_nLeftDockID, &m_nRightDockID );
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Up, 0.5f, &m_nTopDockID, &m_nBottomDockID );
	ImGui::DockBuilderDockWindow( pWindowLabel, m_nTopDockID );
}

void CEditorApplication::DockWindowBottom( const char *pWindowLabel )
{
	ImGuiID nDockID = ImGui::DockSpaceOverViewport( ImGui::GetWindowDockID(), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode );

	ImGui::DockBuilderRemoveNode( nDockID );
	ImGui::DockBuilderAddNode( nDockID );
	ImGui::DockBuilderSetNodeSize( nDockID, ImGui::GetMainViewport()->Size );
	
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Left, 0.5f, &m_nLeftDockID, &m_nRightDockID );
	ImGui::DockBuilderSplitNode( nDockID, ImGuiDir_Up, 0.5f, &m_nTopDockID, &m_nBottomDockID );
	ImGui::DockBuilderDockWindow( pWindowLabel, m_nBottomDockID );
}

void CEditorApplication::SaveProject( void )
{
	if ( !CProjectManager::Get()->GetProject()->IsModified() ) {
		return;
	}

	CProjectManager::Get()->GetProject()->SetModified( false );

	SIRENGINE_LOG( "Saving Project \"%s\" Disk...", CProjectManager::Get()->GetProject()->GetName().c_str() );

	CProjectManager::Get()->GetProject()->Save();

	SIRENGINE_LOG( "Writing %lu ScriptClasses...", CScriptCompiler::GetObjects().size() );
	for ( auto& it : CScriptCompiler::GetObjects() ) {
		it.Save();
	}
	CTextEditorManager::Get().Update();

	SIRENGINE_LOG( "Done." );
}

void CEditorApplication::Shutdown( void )
{
	m_Widgets.clear();

	CProjectManager::SaveCache();

	ImPlot::DestroyContext();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

};