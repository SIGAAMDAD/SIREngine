#include "ConsoleManager.h"
#include <locale.h>

CConsoleManager CConsoleManager::g_ConsoleManager;

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
    for ( auto& it : m_ObjectList ) {
        if ( it.second->AsVariable() ) {
            if ( it.second->AsVariable()->GetGroup() == CVG_USERINFO || it.second->AsVariable()->GetGroup() == CVG_NONE ) {
                delete it.second; // runtime allocated cvar
            }
        } else {
            delete it.second; // command
        }
    }
    if ( m_pConfigLoader ) {
        delete m_pConfigLoader;
    }
    setlocale( LC_NUMERIC, m_pSavedLocale );
}

IConsoleVar *CConsoleManager::RegisterCVar( const char *pName, bool bValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVar<bool>( pName, bValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%s\"", pName, SIREngine_BoolToString( bValue ) );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVar( const char *pName, uint32_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVar<uint32_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%u\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVar( const char *pName, uint64_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVar<uint64_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%lu\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVar( const char *pName, int32_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVar<int32_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%i\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVar( const char *pName, int64_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVar<int64_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%li\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVar( const char *pName, float fValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVar<float>( pName, fValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%0.02f\"", pName, fValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVar( const char *pName, const CString& value, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVar<CString>( pName, value, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%s\"", pName, value.c_str() );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

void CConsoleManager::RegisterCVarDefaultValue( IConsoleVar *pCvar )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );
    
    auto it = m_ObjectList.find( pCvar->GetName() );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pCvar->GetName() );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
    }
    
    m_ObjectList.try_emplace( pCvar->GetName(), pCvar );
}

IConsoleVar *CConsoleManager::RegisterCVarRef( const char *pName, bool& bValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVarRef<bool>( pName, bValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%s\"", pName, SIREngine_BoolToString( bValue ) );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVarRef( const char *pName, uint32_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVarRef<uint32_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%u\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVarRef( const char *pName, uint64_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVarRef<uint64_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%lu\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVarRef( const char *pName, int32_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );
    
    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVarRef<int32_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%i\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVarRef( const char *pName, int64_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVarRef<int64_t>( pName, nValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%li\"", pName, nValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVarRef( const char *pName, float& fValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVarRef<float>( pName, fValue, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%0.02f\"", pName, fValue );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}

IConsoleVar *CConsoleManager::RegisterCVarRef( const char *pName, CString& value, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup )
{
    CThreadAutoLock<CThreadMutex> _( m_hObjectLock );

    auto it = m_ObjectList.find( pName );
    if ( it != m_ObjectList.end() ) {
        SIRENGINE_WARNING( "IConsoleObject \"%s\" already registered", pName );
        if ( !it->second->AsVariable() ) {
            SIRENGINE_ERROR( "CConsoleManager::RegisterCVarRef: called on IConsoleObject that isn't a valid cvar" );
        }
        return it->second->AsVariable();
    }

    IConsoleVar *pCvar = new CVarRef<CString>( pName, value, nFlags, pDescription, nGroup );
    SIRENGINE_LOG( "Registered CVarRef object \"%s\" with defaultValue \"%s\"", pName, value.c_str() );

    m_ObjectList.try_emplace( pName, pCvar );

    return pCvar;
}


void CConsoleManager::RegisterConsoleCommand( IConsoleCmd *pCommand )
{
}
/*
void CConsoleManager::RegisterCVar( IConsoleVar *pCvar )
{
    if ( m_ObjectList.find( pCvar->GetName() ) == m_ObjectList.end() ) {
        m_ObjectList.try_emplace( pCvar->GetName(), pCvar );

        if ( pCvar->GetFlags() & Cvar_Save ) {
            if ( m_pConfigLoader->HasValue( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName() ) ) {
                switch ( pCvar->GetType() ) {
                case CvarType_Bool:
                    pCvar->SetValue( m_pConfigLoader->GetBool( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName() ) );
                    break;
                case CvarType_Int:
                    pCvar->SetValue( m_pConfigLoader->GetInt( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName() ) );
                    break;
                case CvarType_UInt:
                    pCvar->SetValue( m_pConfigLoader->GetUInt( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName() ) );
                    break;
                case CvarType_Float:
                    pCvar->SetValue( m_pConfigLoader->GetFloat( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName() ) );
                    break;
                case CvarType_FilePath:
                    pCvar->SetValue( m_pConfigLoader->GetString( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName() ).c_str() );
                    break;
                case CvarType_String:
                    pCvar->SetValue( m_pConfigLoader->GetString( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName() ) );
                    break;
                };
            }
        }
    }
}
*/

void CConsoleManager::LoadConfig( void )
{
    const char *pSectionName;

    SIRENGINE_LOG( "Loading Engine Configuration..." );
    
    m_pConfigLoader = new CIniSerializer( "EngineData.ini" );

    for ( auto& it : m_ObjectList ) {
        if ( !it.second->AsVariable() ) {
            continue; // its a command
        }

        IConsoleVar *pCvar = it.second->AsVariable();
        if ( !( pCvar->GetFlags() & Cvar_Save ) ) {
            continue;
        }

        pSectionName = GetConfigSectionName( pCvar->GetGroup() );

        switch ( pCvar->GetType() ) {
        case CvarType_Bool:
            pCvar->SetValue( (bool32)m_pConfigLoader->GetBool( pSectionName, pCvar->GetName() ) );
            break;
        case CvarType_Int:
            pCvar->SetValue( (int32_t)m_pConfigLoader->GetInt( pSectionName, pCvar->GetName() ) );
            break;
        case CvarType_UInt:
            pCvar->SetValue( (uint32_t)m_pConfigLoader->GetUInt( pSectionName, pCvar->GetName() ) );
            break;
        case CvarType_Float:
            pCvar->SetValue( m_pConfigLoader->GetFloat( pSectionName, pCvar->GetName() ) );
            break;
        case CvarType_FilePath:
            pCvar->SetValue( FileSystem::CFilePath( m_pConfigLoader->GetString( pSectionName, pCvar->GetName() ).c_str() ) );
            break;
        case CvarType_String:
            pCvar->SetValue( m_pConfigLoader->GetString( pSectionName, pCvar->GetName() ) );
            break;
        };
    }

    SIRENGINE_LOG( "Done." );
}

void CConsoleManager::SaveConfig( void ) const
{
    SIRENGINE_LOG( "Saving Engine Configuration..." );
    for ( const auto& var : m_ObjectList ) {
        if ( !var.second->AsVariable() ) {
            continue; // its a command
        }

        IConsoleVar *pCvar = var.second->AsVariable();
        if ( !( pCvar->GetFlags() & Cvar_Save ) ) {
            continue;
        }
        SIRENGINE_LOG( "\"%s\" = \"%s\"", pCvar->GetName().c_str(), pCvar->GetStringValue().c_str() );
        m_pConfigLoader->SetValue( GetConfigSectionName( pCvar->GetGroup() ), pCvar->GetName(), pCvar->GetStringValue() );
    }

    if ( !m_pConfigLoader->Save( "Config/EngineData.ini" ) ) {
        SIRENGINE_WARNING( "Error saving engine configuration file!" );
    }
}

const char *CConsoleManager::SkipWhitespace( const char *pText )
{
    while ( 1 ) {
        while ( *pText && *pText <= ' ' ) {
            pText++;
        }
        if ( !*pText ) {
            return NULL;
        }
        
        // single-line comments
        if ( pText[0] == '/' && pText[1] == '/' ) {
            if ( pText < m_CommandLine.begin() + 3 ) {
                return NULL;
            }
        }

        // multiline comments
        if ( pText[0] == '/' && pText[1] == '*' ) {
            while ( *pText && ( pText[0] != '*' || pText[1] != '/' ) ) {
                pText++;
            }
            if ( !*pText ) {
                return NULL;
            }
            pText += 2;
        } else {
            break;
        }
    }
    return pText;
}

void CConsoleManager::ExecuteCommand( const char *pText )
{
    const char *pInput;
    char *pOut;
    char szCommandArgs[8192];

    CThreadAutoLock<CThreadMutex> _( m_hCommandLock );

    SIREngine_strncpyz( m_CommandLine.begin(), pText, m_CommandLine.size() );
    pInput = m_CommandLine.begin();
    pOut = szCommandArgs;

    while ( 1 ) {
        pInput = SkipWhitespace( pInput );

        if ( !pInput ) {
            return; // all tokens processed
        }

        // parse a quoted string
        if ( *pInput == '\"' ) {
            m_CommandArgs[ m_CommandArgc.load() ] = pOut;
            m_CommandArgc.fetch_add( 1 );
            pInput++;

            while ( *pInput && *pInput != '\"' ) {
                *pOut++ = *pInput++;
            }

            *pOut = '\0';

            if ( !*pInput ) {
                return;
            }

            pInput++;
            continue;
        }

        m_CommandArgs[ m_CommandArgc.load() ] = pOut;
        m_CommandArgc.fetch_add( 1 );

        // skip until whitespace
        while ( *pInput > ' ' ) {
            if ( pInput[0] == '\"' ) {
                break;
            }

            if ( pInput[0] == '/' && pInput[1] == '/' ) {
                if ( pInput < m_CommandLine.begin() + 3 ) {
                    break;
                }
            }

            if ( pInput[0] == '/' && pInput[1] == '*' ) {
                break;
            }

            *pOut++ = *pInput++;
        }

        *pOut++ = '\0';

        if ( !*pInput ) {
            return; // all tokens parsed
        }
    }
}