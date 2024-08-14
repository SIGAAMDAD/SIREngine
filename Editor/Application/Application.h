#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#pragma once

#include <Engine/Memory/Memory.h>
#include <Engine/Core/FileSystem/FileSystem.h>
#include <Engine/Core/EngineApp.h>
#include <EASTL/unique_ptr.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>

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
	protected:
		const char *m_pLabel;
	};

	class CEditorApplication : public SIREngine::IEngineApp
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

		SIRENGINE_FORCEINLINE static CEditorApplication& Get( void )
		{ return g_Application; }
		
		SIRENGINE_FORCEINLINE void AddWidget( IEditorWidget *pWidget )
		{ m_Widgets.emplace_back( pWidget ); }
	private:
		CVector<IEditorWidget *> m_Widgets;

		static CEditorApplication g_Application;
	};
};

namespace ImGui {
	bool InputText(const char* label, CString *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
	bool InputTextMultiline(const char* label, CString *str, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
	bool InputTextWithHint(const char* label, const char* hint, CString *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
};

#endif