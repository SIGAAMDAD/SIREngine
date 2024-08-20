#ifndef __VALDEN_EDITOR_PREFERENCES_H__
#define __VALDEN_EDITOR_PREFERENCES_H__

#pragma once

#include "Application.h"
#include <Engine/Core/Events/KeyCodes.h>

namespace Valden {
	class CEditorPreferences
	{
	public:
		CEditorPreferences( void );
		~CEditorPreferences();

		bool Load( void );
		void Save( void );
		
		void Draw( void );

		SIRENGINE_FORCEINLINE static CEditorPreferences& Get( void )
		{ return g_Preferences; }
	private:
		static void ShortcutCallback( const Events::IEventBase *pEventData );

		typedef struct {
			Events::KeyNum_t nKey0;
			Events::KeyNum_t nKey1;
		} ShortCut_t;

		eastl::unordered_map<CString, ShortCut_t> m_KeyboardShortcuts;

		int m_nStylePreset;
		ImGuiStyle m_Style;

		bool m_bShowFramerate;
		bool m_bUseLessCPUWhenUnfocused;

		static CEditorPreferences g_Preferences;
	};
};

#endif