#include "TBonSerializer.h"
#include <Engine/Core/FileSystem/MemoryFile.h>

CTBonSerializer::CTBonSerializer( void )
{
    m_Base.SetName( "BaseObject#obj_072746663334546B" );
}

CTBonSerializer::~CTBonSerializer()
{
}

bool CTBonSerializer::Load( const FileSystem::CFilePath& filePath )
{
    CMemoryFile file( filePath );
    if ( !file.GetSize() ) {
        return false;
    }
    ParseObject( &m_Base, (const char *)file.GetBuffer() );
    return true;
}

void CTBonObject::Write( FileSystem::CFileWriter *hFile, uint64_t& nDepth )
{
    uint64_t i;

    nDepth++;

    hFile->Write( "\t", nDepth );
    hFile->Printf( "\"%s\" {\n", m_Name.c_str() );

    for ( auto it = m_Variables.begin(); it != m_Variables.end(); it++ ) {
        hFile->Write( "\t", nDepth );
        hFile->Printf( "\"%s\" \"%s\"\n", it->first.c_str(), it->second.c_str() );
    }
    for ( auto& it : m_Objects ) {
        it.second->Write( hFile, nDepth );
    }

    hFile->Write( "\t", nDepth );
    hFile->Printf( "\n" );
}

bool CTBonSerializer::Save( const FileSystem::CFilePath& filePath )
{
    FileSystem::CFileWriter *hFile = g_pFileSystem->OpenFileWriter( filePath );
    uint64_t nDepth = 0;

    SIRENGINE_LOG( "Saving TBon object to \"%s\"...", filePath.c_str() );
    for ( auto& it : m_Base.m_Objects ) {
        it.second->Write( hFile, nDepth );
    }

    delete hFile;

    return true;
}

const char *CTBonSerializer::SkipRestOfLine( const char *pText ) const
{
    while ( *pText ) {
        if ( *pText == '\n' ) {
            pText++;
            break;
        }
        pText++;
    }
    return pText;
}

const char *CTBonSerializer::ParseString( CString& str, const char *pText ) const
{
    while ( *pText ) {
        if ( *pText == '\"' ) {
            return pText + 1;
        } else if ( *pText == '\n' ) {
            break;
        } else {
            str.insert( str.end(), *pText );
        }
        pText++;
    }
    return NULL;
}

bool CTBonSerializer::ParseObject( CTBonObject *pObject, const char *pText )
{
    const char *pString;
    bool bInStarComment, bInSlashComment;
    uint64_t nObjectDepth;
    CString strValue;
    CTBonObject *pNewObject;

    pString = pText;
    bInSlashComment = false;
    bInStarComment = false;
    nObjectDepth = 0;

    while ( *pString ) {
        //
        // multiline comment
        //
        if ( pString[0] == '/' && pString[1] == '*' ) {
            /*
            while ( *pString ) {
                if ( *pString == '*' && *( pString + 1 ) == '\\' ) {
                    pString += 2;
                    break;
                }
                pString++;
            }
            */
            pString = strstr( pString, "*/" );
            if ( !pString ) {
                SIRENGINE_WARNING( "Invalid TBon file, expected \"*/\" to end a multiline comment" );
                return false;
            }
        }

        //
        // single line comment
        //
        else if ( pString[0] == '/' && pString[1] == '/' ) {
            pString = SkipRestOfLine( pString );
        }

        if ( pString[0] == '\"' ) {
            CString *pValue = NULL;
            if ( strValue.size() && !nObjectDepth ) {
                return false; // no object declared, and we're defining a variable
            } else if ( strValue.size() && nObjectDepth > 0 ) {
                pValue = &pObject->m_Variables.try_emplace( strValue ).first->second;
            }
            strValue.clear();
            pString = ParseString( strValue, pString + 1 );
            if ( !pString ) {
                SIRENGINE_WARNING( "Invalid string in TBon file, multiline strings aren't supported yet" );
                return false; // invalid end of line token in the middle of a string
            }
            if ( pValue ) {
                *pValue = strValue;
            }
        }
        else if ( pString[0] == '{' ) {
            nObjectDepth++;
            if ( strValue.size() > 0 ) {
                // object name given, check if already exists
                const uint64_t nHashID = GetObjectID( strValue.c_str() );

                if ( pObject->m_Objects.find( nHashID ) != pObject->m_Objects.end() ) {
                    SIRENGINE_WARNING( "TBon object \"%s\" already defined", strValue.c_str() );
                    return false; // redefined object
                }

                pNewObject = pObject->m_Objects.try_emplace( nHashID, new CTBonObject() ).first->second;
                pNewObject->SetName( strValue );
            }
            else {
                const uint64_t nHashID = pObject->m_Objects.size();
                const char *pObjectName = SIRENGINE_TEMP_VSTRING( "#obj_%08lu", nHashID );

                if ( pObject->m_Objects.find( nHashID ) != pObject->m_Objects.end() ) {
                    SIRENGINE_WARNING( "TBon object \"%s\" already defined", pObjectName );
                    return false; // redefined object, although this probably shouldn't happen
                }

                pNewObject = pObject->m_Objects.try_emplace( nHashID, new CTBonObject() ).first->second;
                pNewObject->SetName( pObjectName );
            }
            pString++;
            if ( !ParseObject( pNewObject, pString ) ) {
                return false; // invalid object
            }
        }
        else if ( pString[0] == '}' ) {
            if ( nObjectDepth == 0 ) {
                SIRENGINE_WARNING( "Invalid TBon file, expected object definition" );
                return false; // invalid end of object definition
            }
            nObjectDepth--;
        }
    }

    return true;
}