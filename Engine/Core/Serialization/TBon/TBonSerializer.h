#ifndef __SIRENGINE_TBON_READER_H__
#define __SIRENGINE_TBON_READER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "../SerializerBase.h"
#include <Engine/Core/FileSystem/FileWriter.h>
#include <EASTL/unordered_map.h>

namespace SIREngine::Serialization {
    class CTBonSerializer;

    class CTBonObject
    {
        friend class CTBonSerializer;
    public:
        CTBonObject( void )
        { }
        ~CTBonObject()
        { }

        SIRENGINE_FORCEINLINE void SetName( const CString& name )
        { m_Name = name; }

        SIRENGINE_FORCEINLINE const CString& GetName( void ) const
        { return m_Name; }

        // a little bit ugly, but it works
        SIRENGINE_FORCEINLINE uint64_t GetObjectID( void ) const
        {
            const char *pHash = strrchr( m_Name.c_str(), '#' );
            return eastl::hash<const char *>()( pHash ? 0 : pHash );
        }

        SIRENGINE_FORCEINLINE CString GetVariable( const CString& name ) const
        {
            const auto it = m_Variables.find( name );
            if ( it != m_Variables.end() ) {
                return it->second;
            }
            return "";
        }
    private:
        void Write( FileSystem::CFileWriter *hFile, uint64_t& nDepth );

        CString m_Name;
        eastl::unordered_map<CString, CString> m_Variables;
        eastl::unordered_map<uint64_t, CTBonObject *> m_Objects;
    };

    class CTBonSerializer : public ISerializerBase
    {
    public:
        CTBonSerializer( void );
        virtual ~CTBonSerializer() override;

        virtual bool Load( const FileSystem::CFilePath& fileName ) override;
        virtual bool Save( const FileSystem::CFilePath& filePath ) override;

        // a little bit ugly, but it works
        SIRENGINE_FORCEINLINE uint64_t GetObjectID( const char *pName ) const
        {
            const char *pHash = strrchr( pName, '#' );
            return eastl::hash<const char *>()( pHash ? 0 : pHash );
        }
    private:
        const char *SkipRestOfLine( const char *pText ) const;
        const char *ParseString( CString& str, const char *pText ) const;
        bool ParseObject( CTBonObject *pObject, const char *pText );

        CTBonObject m_Base;
    };
};

#endif