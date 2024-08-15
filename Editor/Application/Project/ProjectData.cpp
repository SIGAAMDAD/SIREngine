#include <Engine/Core/Serialization/Ini/IniSerializer.h>
#include "ProjectData.h"
#include "ProjectManager.h"

namespace Valden {

CProjectData::CProjectData( void )
{
}

CProjectData::~CProjectData()
{
}

void CProjectData::InitDirectoryStructure( void )
{
	Application::Get()->CreateDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Assets", m_Info.ProjectName.c_str() ) );
	Application::Get()->CreateDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Assets/Maps", m_Info.ProjectName.c_str() ) );
	Application::Get()->CreateDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Assets/Textures", m_Info.ProjectName.c_str() ) );
	Application::Get()->CreateDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Assets/Actors", m_Info.ProjectName.c_str() ) );
	Application::Get()->CreateDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Assets/Audio", m_Info.ProjectName.c_str() ) );

	Application::Get()->CreateDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Scripts", m_Info.ProjectName.c_str() ) );
	Application::Get()->CreateDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Scripts/Classes", m_Info.ProjectName.c_str() ) );
}

bool CProjectData::Load( const CString& projectName )
{
	g_pFileSystem->AddCacheDirectory( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s", projectName.c_str() ) );

	m_Info.Directory = SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s", projectName.c_str() );
	m_pIniData = new Serialization::CIniSerializer( SIRENGINE_TEMP_VSTRING( "%s/%s.proj",
		m_Info.Directory.c_str(), projectName.c_str() ) );
	
	m_Info.ProjectName = m_pIniData->GetString( "ProjectConfig", "Name" );
	m_Info.bDisableStdErr = m_pIniData->GetBool( "ProjectConfig", "DisableStdErr" );
	m_Info.bDisableStdOut = m_pIniData->GetBool( "ProjectConfig", "DisableStdOut" );

	return true;
}

void CProjectData::Save( void )
{
	if ( !m_pIniData ) {
		m_pIniData = new Serialization::CIniSerializer();
	}

	SIRENGINE_LOG_LEVEL( ProjectManager, ELogLevel::Info, "Saving Project \"%s\"...", m_Info.ProjectName.c_str() );

	m_pIniData->SetValue( "ProjectConfig", "Name", m_Info.ProjectName );
	m_pIniData->SetValue( "ProjectConfig", "DisableStdErr", SIREngine_BoolToString( m_Info.bDisableStdErr ) );
	m_pIniData->SetValue( "ProjectConfig", "DisableStdOut", SIREngine_BoolToString( m_Info.bDisableStdOut ) );

	m_pIniData->SetValue( "ScriptConfig", "AllowIntegerDivision", SIREngine_BoolToString( m_Info.ScriptingConfig.bAllowIntegerDivision ) );
	m_pIniData->SetValue( "ScriptConfig", "AllowCompilerWarnings", SIREngine_BoolToString( m_Info.ScriptingConfig.bAllowCompilerWarnings ) );
	m_pIniData->SetValue( "ScriptConfig", "AllowUnsafeReferences", SIREngine_BoolToString( m_Info.ScriptingConfig.bAllowUnsafeReferences ) );
	m_pIniData->SetValue( "ScriptConfig", "OptimizeByteCode", SIREngine_BoolToString( m_Info.ScriptingConfig.bOptimizeByteCode ) );
	m_pIniData->SetValue( "ScriptConfig", "MaxStackSize", SIRENGINE_TEMP_VSTRING( "%u", m_Info.ScriptingConfig.nMaxStackSize ) );

	m_pIniData->Save( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/%s.proj",
		m_Info.ProjectName.c_str(), m_Info.ProjectName.c_str() ) );
}

};