#include "GenericApplication.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <Engine/Core/ThreadSystem/Thread.h>
#include <Engine/Core/Serialization/JSon/JsonCache.h>
#include <Engine/Core/FileSystem/FileSystem.h>
#include <Engine/Core/Events/EventManager.h>
#include <Engine/Core/Input/InputManager.h>

#if defined(SIRENGINE_BUILD_EDITOR)
#include <Editor/Application/Application.h>
#endif

using namespace SIREngine;
using namespace SIREngine::Application;
using namespace SIREngine::Events;
using namespace SIREngine::Input;
using namespace SIREngine::RenderLib;

SIREngine::FileSystem::CFileSystem *g_pFileSystem;

uint32_t g_nMaxFPS = 60;

namespace SIREngine::RenderLib::Backend {
	CVar<bool32> e_Fullscreen(
		"e.Fullscreen",
		false,
		Cvar_Save,
		"Sets the engine's window to fullscreen.",
		CVG_RENDERER
	);
	CVar<uint32_t> e_RenderAPI(
		"e.RenderAPI",
		0,
		Cvar_Save,
		"Sets the Rendering API used by the engine."
		"  0: OpenGL\n"
		"  1: Vulkan\n"
		"  2: OpenGL ES\n"
		"  3: DirectX 11\n"
		"  4: Software",
		CVG_RENDERER
	);
};

namespace SIREngine::Application {

CThreadAtomic<bool> g_bExitApp( false );
IGenericApplication *g_pApplication;

CVarRef<uint32_t> e_MaxFPS(
	"e.MaxFPS",
	g_nMaxFPS,
	Cvar_Save,
	"Sets the engine's maximum screen refresh rate (frames per second).",
	CVG_SYSTEMINFO
);

uint64_t g_nFrameNumber = 0;
CVarRef<uint64_t> e_FrameNumber(
	"e.FrameNumber",
	g_nFrameNumber,
	Cvar_Default,
	"The current frame index.",
	CVG_NONE
);

#include <gtk/gtk.h>
#if GTK_MAJOR_VERSION >= 3
#include <gdk/gdk.h>
#else
#include <gdk/gdkkeysyms.h>
typedef enum
{
	GTK_ALIGN_FULL,
	GTK_ALIGN_START,
	GTK_ALIGN_END,
	GTK_ALIGN_CENTER,
	GTK_ALIGN_BASELINE
} GtkAlign;
#endif

// GtkTreeViews eats return keys. I want this to be like a Windows listbox
// where pressing Return can still activate the default button.
static gint AllowDefault(GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_KEY_Return)
	{
		gtk_window_activate_default (GTK_WINDOW(func_data));
	}
	return FALSE;
}

// Double-clicking an entry in the list is the same as pressing OK.
static gint DoubleClickChecker(GtkWidget *widget, GdkEventButton *event, gpointer func_data)
{
	if (event->type == GDK_2BUTTON_PRESS)
	{
		*(int *)func_data = 1;
		gtk_main_quit();
	}
	return FALSE;
}

// When the user presses escape, that should be the same as canceling the dialog.
static gint CheckEscape (GtkWidget *widget, GdkEventKey *event, gpointer func_data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_KEY_Escape)
	{
		gtk_main_quit();
	}
	return FALSE;
}

static void ClickedOK(GtkButton *button, gpointer func_data)
{
	*(int *)func_data = 1;
	gtk_main_quit();
}

class ZUIWidget
{
public:
	virtual ~ZUIWidget() = default;

	GtkWidget *widget = nullptr;
};

class ZUIWindow : public ZUIWidget
{
public:
	ZUIWindow(const char* title)
	{
		widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);

		gtk_window_set_title (GTK_WINDOW(widget), title);
		gtk_window_set_position (GTK_WINDOW(widget), GTK_WIN_POS_CENTER);
		gtk_window_set_gravity (GTK_WINDOW(widget), GDK_GRAVITY_CENTER);

		gtk_container_set_border_width (GTK_CONTAINER(widget), 15);

		g_signal_connect (widget, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
		g_signal_connect (widget, "key_press_event", G_CALLBACK(CheckEscape), NULL);
	}

	~ZUIWindow()
	{
		if (GTK_IS_WINDOW(widget))
		{
			gtk_widget_destroy (widget);
			// If we don't do this, then the X window might not actually disappear.
			while (g_main_context_iteration (NULL, FALSE)) {}
		}
	}

	void AddWidget(ZUIWidget* child)
	{
		gtk_container_add (GTK_CONTAINER(widget), child->widget);
	}

	void RunModal()
	{
		gtk_widget_show_all (widget);
		gtk_main ();
	}
};

class ZUIVBox : public ZUIWidget
{
public:
	ZUIVBox( void )
	{
		widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	}

	void PackStart(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_start (GTK_BOX(widget), child->widget, expand, fill, padding);
	}

	void PackEnd(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_end (GTK_BOX(widget), child->widget, expand, fill, padding);
	}
};

class ZUILabel : public ZUIWidget
{
public:
	ZUILabel(const char* text)
	{
		widget = gtk_label_new (text);
		gtk_widget_set_halign (widget, GTK_ALIGN_START);
		gtk_widget_set_valign (widget, GTK_ALIGN_START);
	}
};

/*
class ZUIListView : public ZUIWidget
{
public:
	ZUIListView( const CVector<CString>& options, int defaultOption )
	{
		// Create a list store with all the found IWADs.
		store = gtk_list_store_new( 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT );
		for ( int i = 0; i < options.size(); ++i ) {
			const char *filepart = strrchr( options[i].c_str(), '/' );

			if ( filepart == NULL ) {
				filepart = options[i].c_str();
			} else {
				filepart++;
			}
			gtk_list_store_append( store, &iter );
			gtk_list_store_set( store, &iter,
				0, filepart,
				1, options[i].c_str(),
				2, i,
				-1 );
			if ( i == defaultOption ) {
				defiter = iter;
			}
		}

		// Create the tree view control to show the list.
		widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
		renderer = gtk_cell_renderer_text_new ();
		column = gtk_tree_view_column_new_with_attributes ("IWAD", renderer, "text", 0, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(widget), column);
		renderer = gtk_cell_renderer_text_new ();
		column = gtk_tree_view_column_new_with_attributes ("Game", renderer, "text", 1, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(widget), column);

		// Select the default IWAD.
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(widget));
		gtk_tree_selection_select_iter (selection, &defiter);
	}

	int GetSelectedIndex()
	{
		GtkTreeModel *model;
		GValue value = { 0, { {0} } };

		// Find out which IWAD was selected.
		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_tree_model_get_value (GTK_TREE_MODEL(model), &iter, 2, &value);
		int i = g_value_get_int (&value);
		g_value_unset (&value);
		return i;
	}

	void ConnectButtonPress(int *close_style)
	{
		g_signal_connect(G_OBJECT(widget), "button_press_event", G_CALLBACK(DoubleClickChecker), close_style);
	}

	void ConnectKeyPress(ZUIWindow* window)
	{
		g_signal_connect(G_OBJECT(widget), "key_press_event", G_CALLBACK(AllowDefault), window->widget);
	}

	GtkListStore *store = nullptr;
	GtkCellRenderer *renderer = nullptr;
	GtkTreeViewColumn *column = nullptr;
	GtkTreeSelection *selection = nullptr;
	GtkTreeIter iter, defiter;
};
*/

class ZUIScrolledWindow : public ZUIWidget
{
public:
	ZUIScrolledWindow(ZUIWidget* child)
	{
		widget = gtk_scrolled_window_new(NULL, NULL);
		if(gtk_scrolled_window_set_min_content_height) gtk_scrolled_window_set_min_content_height((GtkScrolledWindow*)widget,150);
		gtk_container_add(GTK_CONTAINER(widget), child->widget);
	}
};

class ZUIHBox : public ZUIWidget
{
public:
	ZUIHBox()
	{
		// Create the hbox for the bottom row.
		widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	}

	void PackStart(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_start (GTK_BOX(widget), child->widget, expand, fill, padding);
	}

	void PackEnd(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_end (GTK_BOX(widget), child->widget, expand, fill, padding);
	}
};

class ZUICheckButton : public ZUIWidget
{
public:
	ZUICheckButton(const char* text)
	{
		widget = gtk_check_button_new_with_label (text);
	}

	void SetChecked(bool value)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), value);
	}

	int GetChecked()
	{
		return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
	}
};

class ZUIRadioButton : public ZUIWidget
{
public:
	ZUIRadioButton(const char* text)
	{
		widget = gtk_radio_button_new_with_label (nullptr, text);
	}

	ZUIRadioButton(ZUIRadioButton* group, const char* text)
	{
		widget = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(group->widget), text);
	}

	void SetChecked(bool value)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), value);
	}

	int GetChecked()
	{
		return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
	}
};

class ZUIButtonBox : public ZUIWidget
{
public:
	ZUIButtonBox( void )
	{
		widget = gtk_button_box_new( GTK_ORIENTATION_HORIZONTAL );
		gtk_button_box_set_layout( GTK_BUTTON_BOX( widget ), GTK_BUTTONBOX_END );
		gtk_box_set_spacing( GTK_BOX( widget ), 10 );
	}

	void PackStart(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_start (GTK_BOX(widget), child->widget, expand, fill, padding);
	}

	void PackEnd(ZUIWidget* child, bool expand, bool fill, int padding)
	{
		gtk_box_pack_end (GTK_BOX(widget), child->widget, expand, fill, padding);
	}
};

class ZUIButton : public ZUIWidget
{
public:
	ZUIButton(const char* text, bool defaultButton)
	{
		widget = gtk_button_new_with_label (text);

		if (defaultButton) {
			gtk_widget_set_can_default (widget, true);
		}
	}

	void GrabDefault()
	{
		gtk_widget_grab_default (widget);
	}

	void ConnectClickedOK(int *close_style)
	{
		g_signal_connect (widget, "clicked", G_CALLBACK(ClickedOK), close_style);
		g_signal_connect (widget, "activate", G_CALLBACK(ClickedOK), close_style);
	}

	void ConnectClickedExit(ZUIWindow* window)
	{
		g_signal_connect (widget, "clicked", G_CALLBACK(gtk_main_quit), &window->widget);
	}
};

IGenericApplication::IGenericApplication( void )
{
}

IGenericApplication::~IGenericApplication()
{
}

void IGenericApplication::Shutdown( void )
{
	
	g_bExitApp.store( true );

	CConsoleManager::Get().SaveConfig( "Config/EngineData.ini" );

	for ( auto& it : m_ApplicationSystems ) {
		it.second->Shutdown();
	}

	delete g_pRenderLib;
	delete g_pFileSystem;

	CLogManager::ShutdownLogger();
	Mem_Shutdown();

	_Exit( EXIT_SUCCESS );
}

static void SetCommandLineArg( const CString& arg )
{
	auto it = eastl::find( Application::Get()->GetCommandLine().begin(), Application::Get()->GetCommandLine().end(), arg );
	if ( it != Application::Get()->GetCommandLine().end() ) {
		// already set at the command line, replace it
		*it = arg;
	} else {
		Application::Get()->GetCommandLine().emplace_back( arg );
	}
}

static int LauncherWindow( void )
{
	char **argv;
	int argc;
	int close_style;

	argv = NULL;
	argc = 0;
	if ( gtk_init_check( &argc, &argv ) == 0 )  {
		SIRENGINE_ERROR( "Failed initializing GTK+ 3.0!" );
	}

#if defined(SIRENGINE_BUILD_ENGINE)
	ZUIWindow window( "SIREngine Launcher" );
	ZUIVBox vbox;

	ZUIHBox hboxOptions;

	ZUIVBox vboxVideo;
	ZUILabel videoSettings( "Video Settings" );
	ZUIRadioButton opengl( "OpenGL" );
	ZUIRadioButton vulkan( &opengl, "Vulkan" );
	ZUIRadioButton software( &opengl, "Software" );
#if defined(SIRENGINE_PLATFORM_WINDOWS)
	ZUIRadioButton directx( &opengl, "DirectX 11" );
#endif
	ZUIRadioButton openglES( &opengl, "OpenGL ES" );
	ZUICheckButton fullscreen( "Fullscreen" );

	ZUIHBox hboxButtons;

	ZUIButtonBox bbox;
	ZUIButton playButton( "Launch", true );
	ZUIButton exitButton( "Cancel", false );

	window.AddWidget( &vbox );
	vbox.PackEnd( &hboxButtons, false, false, 0 );
	vbox.PackEnd( &hboxOptions, false, false, 0 );
	hboxOptions.PackStart( &vboxVideo, false, false, 15 );
	vboxVideo.PackStart( &videoSettings, false, false, 0 );
	vboxVideo.PackStart( &opengl, false, false, 0 );
	vboxVideo.PackStart( &vulkan, false, false, 0 );
	vboxVideo.PackStart( &software, false, false, 0 );
#if defined(SIRENGINE_PLATFORM_WINDOWS)
	vboxVideo.PackStart( &directx, false, false, 0 );
#endif
	vboxVideo.PackStart( &openglES, false, false, 0 );
	vboxVideo.PackStart( &fullscreen, false, false, 15 );
	hboxButtons.PackStart( &bbox, true, true, 0 );
	bbox.PackStart( &playButton, false, false, 0 );
	bbox.PackEnd( &exitButton, false, false, 0 );

	switch ( RenderLib::Backend::e_RenderAPI.GetValue() ) {
	case RAPI_OPENGL:
		opengl.SetChecked( true );
		break;
	case RAPI_VULKAN:
		vulkan.SetChecked( true );
		break;
	case RAPI_OPENGLES:
		openglES.SetChecked( true );
		break;
#if defined(SIRENGINE_PLATFORM_WINDOWS)
	case RAPI_D3D11:
		directx.SetChecked( true );
		break;
#endif
	case RAPI_SOFTWARE:
		software.SetChecked( true );
		break;
	};

	fullscreen.SetChecked( RenderLib::Backend::e_Fullscreen.GetValue() );

	playButton.GrabDefault();

	close_style = 0;
	playButton.ConnectClickedOK( &close_style );
	exitButton.ConnectClickedExit( &window );

	playButton.GrabDefault();

	window.RunModal();

	if ( close_style == 1 ) {
		if ( opengl.GetChecked() ) {
			RenderLib::Backend::e_RenderAPI.SetValue( RAPI_OPENGL );
		} else if ( vulkan.GetChecked() ) {
			RenderLib::Backend::e_RenderAPI.SetValue( RAPI_VULKAN );
		} else if ( openglES.GetChecked() ) {
			RenderLib::Backend::e_RenderAPI.SetValue( RAPI_OPENGLES );
		} else if ( software.GetChecked() ) {
			RenderLib::Backend::e_RenderAPI.SetValue( RAPI_SOFTWARE );
		}

		RenderLib::Backend::e_Fullscreen.SetValue( fullscreen.GetChecked() );

		return 1;
	}

	return -1;
#else
	return 1;
#endif
}

bool IGenericApplication::CheckCommandParm( const CString& name ) const
{
	return eastl::find( m_CommandLineArgs.cbegin(), m_CommandLineArgs.cend(), name ) != m_CommandLineArgs.cend();
}

CString IGenericApplication::GetCommandParmValue( const CString& name ) const
{
	auto it = m_CommandLineArgs.cbegin();
	for ( ; it != m_CommandLineArgs.cend(); ++it ) {
		if ( !strncmp( it->c_str(), name.c_str(), name.size() ) ) {
			break;
		}
	}
	if ( it == m_CommandLineArgs.cend() ) {
		return ""; // empty value simply means defaulted
	}

	const size_t pos = it->find_last_of( '=' ) + 1;
	return it->substr( pos, it->size() - pos );
}

void IGenericApplication::QuitGame( const IEventBase *pEventData )
{
	assert( pEventData->GetType() == EventType_Quit );
	Application::Get()->Shutdown();
}

void IGenericApplication::Init( void )
{
	//
	// initialize the engine
	//
	g_pFileSystem = new FileSystem::CFileSystem();

	CConsoleManager::Get().LoadConfig( "EngineData.ini" );
	CLogManager::LaunchLoggingThread();
	if ( LauncherWindow() == -1 ) {
		Shutdown();
		return;
	}

#if defined(SIRENGINE_BUILD_EDITOR)
	m_ApplicationInfo.pszAppName = "Valden";
	m_ApplicationInfo.pszWindowName = "Valden";
#endif

	e_MaxFPS.Register();
	e_FrameNumber.Register();
	RenderLib::Backend::e_RenderAPI.Register();

	g_pRenderLib = new CRenderer();

	m_ApplicationSystems.reserve( 4 );
	m_ApplicationSystems.try_emplace( CEventManager::Get().GetName(), eastl::addressof( CEventManager::Get() ) );
	m_ApplicationSystems.try_emplace( CInputManager::Get().GetName(), eastl::addressof( CInputManager::Get() ) );
	m_ApplicationSystems.try_emplace( g_pRenderLib->GetName(), g_pRenderLib );
#if defined(SIRENGINE_BUILD_EDITOR)
	m_ApplicationSystems.try_emplace( Valden::CEditorApplication::Get().GetName(), eastl::addressof( Valden::CEditorApplication::Get() ) );
#endif

	for ( auto& it : m_ApplicationSystems ) {
		it.second->Init();
	}

	CEventManager::Get().AddEventListener( eastl::make_shared<CEventListener>(
		"ApplicationListener", EventType_Quit, IGenericApplication::QuitGame
	) );

	SIRENGINE_LOG( "SIREngine MetaData:" );
	SIRENGINE_LOG( "  Version: %s", SIRENGINE_VERSION_STRING );
#if defined(SIRENGINE_BUILD_ENGINE)
	SIRENGINE_LOG( "  SIRENGINE_BUILD_TYPE: Engine" );
#elif defined(SIRENGINE_BUILD_EDITOR)
	SIRENGINE_LOG( "  SIRENGINE_BUILD_TYPE: Editor" );
#endif
	MemoryStats_t stats = Application::Get()->GetMemoryStats();
	SIRENGINE_LOG( "[OS Memory Statistics]" );
	SIRENGINE_LOG( "  Total Physical RAM: %s", SIREngine_GetMemoryString( stats.nTotalPhysical ) );
	SIRENGINE_LOG( "  Available Physical RAM: %s", SIREngine_GetMemoryString( stats.nAvailablePhysical ) );
	SIRENGINE_LOG( "  Available Virtual RAM: %s", SIREngine_GetMemoryString( stats.nAvailableVirtual ) );
	SIRENGINE_LOG( "  Peak Used Physical RAM: %s", SIREngine_GetMemoryString( stats.nPeakUsedPhysical ) );
	SIRENGINE_LOG( "  Peak Used Virtual RAM: %s", SIREngine_GetMemoryString( stats.nPeakUsedVirtual ) );
	SIRENGINE_LOG( "  Used Physical RAM: %s", SIREngine_GetMemoryString( stats.nUsedPhysical ) );
	SIRENGINE_LOG( "  Used Virtual RAM: %s", SIREngine_GetMemoryString( stats.nUsedVirtual ) );

}

void IGenericApplication::ShowErrorWindow( const char *pErrorString )
{
	ZUIWindow window( "Fatal Error" );
	ZUIVBox vbox;
	ZUILabel label( pErrorString );
	ZUIButtonBox bbox;
	ZUIButton exitButton( "Exit", true );

	window.AddWidget( &vbox );
	vbox.PackStart( &label, true, true, 0 );
	vbox.PackEnd( &bbox, false, false, 0 );
	bbox.PackEnd( &exitButton, false, false, 0 );

	exitButton.ConnectClickedExit( &window );

	exitButton.GrabDefault();
	window.RunModal();
}

void IGenericApplication::Run( void )
{
	while ( 1 ) {
		g_pRenderLib->BeginFrame();
		for ( auto& it : m_ApplicationSystems ) {
			it.second->Frame( 0 );
		}
		g_pRenderLib->EndFrame();
	}
}

void IGenericApplication::OnOutOfMemory( uint64_t nSize, uint64_t nAlignment )
{
}

};