#include "EditorPreferences.h"

namespace Valden {

CEditorPreferences CEditorPreferences::g_Preferences;

CEditorPreferences::CEditorPreferences( void )
{
}

CEditorPreferences::~CEditorPreferences()
{
}

bool CEditorPreferences::Load( void )
{
	Serialization::CIniSerializer iniReader( "Valden/Config/EditorPreferences.ini" );

	m_bShowFramerate = iniReader.GetBool( "ValdenConfig", "ShowFramerate" );
	m_bUseLessCPUWhenUnfocused = iniReader.GetBool( "ValdenConfig", "UseLessCPUWhenUnfocused" );

	return true;
}

void CEditorPreferences::Save( void )
{
}
	
void CEditorPreferences::Draw( void )
{
}

};