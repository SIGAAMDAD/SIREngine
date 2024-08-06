#include "IniSerializer.h"
#include "ini.h"

bool CIniSerializer::Load( const FileSystem::CFilePath& filePath )
{
    FileSystem::CFileReader *hFile = g_pFileSystem->OpenFileReader( filePath );

    if ( hFile ) {
        m_nError = ini_parse( hFile->GetPath().c_str(), ValueHandler, this );
        delete hFile;
        return true;
    } else {
        SIRENGINE_WARNING( "Error loading .ini file \"%s\"", filePath.c_str() );
    }

    return false;
}

int CIniSerializer::ParseError( void ) const
{
    return m_nError;
}

CString CIniSerializer::Get( const CString& section, const CString& name ) const
{
    if ( m_Values.find( section ) != m_Values.end() ) {
        return m_Values.at( section ).at( name );
    }
    return "";
}

CString CIniSerializer::GetString( const CString& section, const CString& name ) const
{
    const CString str = Get( section, name );
    return str.empty() ? "" : str;
}

int CIniSerializer::GetInt( const CString& section, const CString& name ) const
{
    CString valstr = Get( section, name );
    if ( !valstr.size() ) {
        return 0;
    }
    return atoi( valstr.c_str() );
}

int64_t CIniSerializer::GetInt64( const CString& section, const CString& name ) const
{
    CString valstr = Get( section, name );
    if ( !valstr.size() ) {
        return 0;
    }
    return atol( valstr.c_str() );
}

unsigned CIniSerializer::GetUInt( const CString& section, const CString& name ) const
{
    CString valstr = Get( section, name );
    if ( !valstr.size() ) {
        return 0;
    }
    return (unsigned)atol( valstr.c_str() );
}

uint64_t CIniSerializer::GetUInt64( const CString& section, const CString& name ) const
{
    CString valstr = Get( section, name );
    if ( !valstr.size() ) {
        return 0;
    }
    return (uint64_t)atoll( valstr.c_str() );
}

float CIniSerializer::GetFloat( const CString& section, const CString& name ) const
{
    CString valstr = Get( section, name );
    if ( !valstr.size() ) {
        return 0.0f;
    }
    return atof( valstr.c_str() );
}

bool CIniSerializer::GetBool( const CString& section, const CString& name ) const
{
    CString valstr = Get( section, name );

    valstr.ToLower();

    if ( valstr == "true" || valstr == "yes" || valstr == "1" || valstr == "on" ) {
        return true;
    } else if ( valstr == "false" || valstr == "no" || valstr == "0" || valstr == "off" ) {
        return false;
    } else {
        return false;
    }
}

bool CIniSerializer::HasSection( const CString& section ) const
{
    eastl::unordered_map<CString, eastl::unordered_map<CString, CString>>::const_iterator pos = m_Values.find( section );
    
    if ( pos == m_Values.end() ) {
        return false;
    }
    return pos->first.compare( 0, section.size(), section ) == 0;
}

bool CIniSerializer::HasValue( const CString& section, const CString& name ) const
{
    if ( !HasSection( section ) ) {
        return false;
    }
    return m_Values.at( section ).find( name ) != m_Values.at( section ).cend();
}

int CIniSerializer::ValueHandler( void *user, const char *section, const char *name,
                            const char *value )
{
    if ( !name ) { // Happens when INI_CALL_HANDLER_ON_NEW_SECTION enabled
        return 1;
    }

    CIniSerializer *reader = (CIniSerializer *)user;
    
    reader->m_Values[ section ].try_emplace( name, value );
    return 1;
}
