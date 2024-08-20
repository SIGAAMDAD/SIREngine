#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#pragma once

#include <Engine/Core/FileSystem/FileSystem.h>
#include <Engine/Core/EngineApp.h>
#include <EASTL/unique_ptr.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include "../IconsFontAwesomeEditor.h"
#include "IconsFontAwesome5.h"
#include <Engine/Core/ConsoleManager.h>
#include "ImGuiNotify.hpp"
#include <Engine/Util/CUniquePtr.h>

#include <Engine/RenderLib/Backend/OpenGL/GLTexture.h>

#define IMGUI_TEXTURE_ID( texture ) \
	(ImTextureID)(intptr_t)( Cast<RenderLib::Backend::OpenGL::GLTexture>( Cast<CMaterial>( texture )->GetTexture() )->GetOpenGLHandle() )

#define ITEM_TOOLTIP_STRING( ... ) if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNone ) ) { ImGui::SetTooltip( __VA_ARGS__ ); }

#include <Engine/Memory/Memory.h>

using namespace SIREngine;

namespace Valden {
	class IEditorWidget
	{
	public:
		IEditorWidget( const char *pLabel )
			: m_pLabel( pLabel )
		{ }
		virtual ~IEditorWidget()
		{ }

		virtual void Draw( void ) = 0;
		virtual void Dock( void ) = 0;
	protected:
		const char *m_pLabel;
	};

	class CEditorApplication : public IEngineApp
	{
	public:
		CEditorApplication( void )
		{ }
		virtual ~CEditorApplication() override
		{ }

		virtual void Init( void ) override;
		virtual void Shutdown( void ) override;
		virtual void Frame( int64_t msec ) override;

		virtual void SaveGame( void ) override { }
		virtual void LoadGame( void ) override { }

		virtual const char *GetName( void ) const override
		{ return "ValdenApplication"; }
        virtual bool IsActive( void ) const override
		{ return true; }
        virtual uint32_t GetState( void ) const override
		{ return 0; }

		void DockWindowLeft( const char *pWindowLabel );
		void DockWindowRight( const char *pWindowLabel );
		void DockWindowTop( const char *pWindowLabel );
		void DockWindowBottom( const char *pWindowLabel );

		void SaveProject( void );

		SIRENGINE_FORCEINLINE static CEditorApplication& Get( void )
		{ return g_Application; }
		
		SIRENGINE_FORCEINLINE void AddWidget( IEditorWidget *pWidget )
		{ m_Widgets.emplace_back( pWidget ); }
	private:
		void DrawMainMenuBar( void );
		void DrawBottomMenu( void );
		void DrawProjectSettings( void );
		void DrawEditorSettings( void );
		void DrawViewport( void );

		CVector<IEditorWidget *> m_Widgets;
		CVector<CString> m_RecentProjects;

		ImGuiID m_nTopDockID;
		ImGuiID m_nBottomDockID;
		ImGuiID m_nLeftDockID;
		ImGuiID m_nRightDockID;

		bool m_bProjectSettingsActive;
		bool m_bEditorSettingsActive;

		static CEditorApplication g_Application;
	};

	extern CVar<uint32_t> AutoSaveTime;
	extern CVar<bool> AutoSaveEnabled;
};

namespace ImGui {
	bool InputText(const char* label, CString *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
	bool InputTextMultiline(const char* label, CString *str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
	bool InputTextWithHint(const char* label, const char* hint, CString *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
};

#endif