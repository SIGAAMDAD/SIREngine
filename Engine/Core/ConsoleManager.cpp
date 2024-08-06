#include "ConsoleManager.h"
#include <locale.h>

CConsoleManager g_ConsoleManager;

static const char *GetConfigSectionName( CvarGroup_t nGroup )
{
    switch ( nGroup ) {
    case CVG_FILESYSTEM:
        return "FileSystemConfig";
    case CVG_RENDERER:
        return "RendererConfig";
    case CVG_SYSTEMINFO:
        return "EngineConfig";
    case CVG_USERINFO:
        return "RuntimeConfig";
    case CVG_SOUNDSYSTEM:
        return "SoundSystemConfig";
    default:
        break;
    };
    return "MiscConfig";
}

CConsoleManager::CConsoleManager( void )
{
    m_pSavedLocale = setlocale( LC_NUMERIC, NULL );
    setlocale( LC_NUMERIC, "C" );
}

CConsoleManager::~CConsoleManager()
{
    if ( m_pConfigLoader ) {
        delete m_pConfigLoader;
    }
    setlocale( LC_NUMERIC, m_pSavedLocale );
}

void CConsoleManager::RegisterCVar( IConsoleVar *pCvar )
{
    if ( m_CvarList.find( pCvar->GetName() ) == m_CvarList.end() ) {
        m_CvarList.try_emplace( pCvar->GetName(), pCvar );
    }
}

void CConsoleManager::LoadConfig( void )
{
    const char *pSectionName;

    SIRENGINE_LOG( "Loading Engine Configuration..." );
    
    m_pConfigLoader = new CIniSerializer( "Confg/EngineData.ini" );

    for ( auto& it : m_CvarList ) {
        pSectionName = GetConfigSectionName( it.second->GetGroup() );

        switch ( it.second->GetType() ) {
        case CvarType_Bool:
            it.second->SetValue( (bool32)m_pConfigLoader->GetBool( pSectionName, it.second->GetName() ) );
            break;
        case CvarType_Int:
            it.second->SetValue( (int32_t)m_pConfigLoader->GetInt( pSectionName, it.second->GetName() ) );
            break;
        case CvarType_UInt:
            it.second->SetValue( (uint32_t)m_pConfigLoader->GetUInt( pSectionName, it.second->GetName() ) );
            break;
        case CvarType_Float:
            it.second->SetValue( m_pConfigLoader->GetFloat( pSectionName, it.second->GetName() ) );
            break;
        case CvarType_FilePath:
            it.second->SetValue( FileSystem::CFilePath( m_pConfigLoader->GetString( pSectionName, it.second->GetName() ).c_str() ) );
            break;
        case CvarType_String:
            it.second->SetValue( m_pConfigLoader->GetString( pSectionName, it.second->GetName() ) );
            break;
        };
    }

    SIRENGINE_LOG( "Done." );
}

void CConsoleManager::SaveConfig( void ) const
{
    for ( const auto& var : m_CvarList ) {
        m_pConfigLoader->SetValue( GetConfigSectionName( var.second->GetGroup() ), var.second->GetName(), var.second->GetStringValue() );
    }

    if ( !m_pConfigLoader->Save( "Config/EngineData.ini" ) ) {
        SIRENGINE_WARNING( "Error saving engine configuration file!" );
    }
}