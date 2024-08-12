#ifndef __SIRENGINE_INI_SERIALIZER_H__
#define __SIRENGINE_INI_SERIALIZER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/FileSystem/FilePath.h>
#include "../SerializerBase.h"

namespace SIREngine::Serialization {
    class CIniSerializer : public ISerializerBase
    {
    public:
        CIniSerializer( void )
        { }
        CIniSerializer( const FileSystem::CFilePath& filePath )
        { Load( filePath ); }
        virtual ~CIniSerializer() override
        { }

        int ParseError( void ) const;

        CString Get( const CString& section, const CString& name ) const;
        CString GetString( const CString& section, const CString& name ) const;

        int GetInt( const CString& section, const CString& name ) const;
        int64_t GetInt64( const CString& section, const CString& name ) const;
        unsigned GetUInt( const CString& section, const CString& name ) const;
        uint64_t GetUInt64( const CString& section, const CString& name ) const;
        float GetFloat( const CString& section, const CString& name ) const;
        bool GetBool( const CString& section, const CString& name ) const;

        bool HasSection( const CString& section ) const;
        bool HasValue( const CString& section, const CString& name ) const;

        inline void SetValue( const CString& section, const CString& name, const CString& value )
        {
            if ( !HasSection( section ) ) {
                m_Values.try_emplace( section );
            }
            m_Values.at( section )[ name ] = value;
        }

        virtual bool Load( const FileSystem::CFilePath& fileName ) override;
        virtual bool Save( const FileSystem::CFilePath& filePath ) override;
    private:
        int m_nError;
        eastl::unordered_map<CString, eastl::unordered_map<CString, CString>> m_Values;

        static int ValueHandler( void *user, const char *section, const char *name,
                                const char *value );
    };
};

#endif