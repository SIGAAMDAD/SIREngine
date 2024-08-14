#ifndef __SIRENGINE_JSON_SERIALIZER_H__
#define __SIRENGINE_JSON_SERIALIZER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/FileSystem/FilePath.h>
#include <nlohmann/json.hpp>
#include <Engine/Core/FileSystem/FileWriter.h>
#include "../SerializerBase.h"

namespace SIREngine::Serialization {
    class CJSonSerializer : public ISerializerBase
    {
    public:
        CJSonSerializer( void )
        { }
        CJSonSerializer( const FileSystem::CFilePath& filePath )
        { Load( filePath ); }
        virtual ~CJSonSerializer() override
        { }

		inline const nlohmann::json& GetJson( void ) const
		{ return m_JsonObject; }

        virtual bool Load( const FileSystem::CFilePath& fileName ) override;
        virtual bool Save( const FileSystem::CFilePath& filePath ) override;
    private:
		void WriteObject( FileSystem::CFileWriter *hFile, const nlohmann::json& data, uint64_t& nDepth );
		void WriteArray( FileSystem::CFileWriter *hFile, const nlohmann::json& data, uint64_t& nDepth );

		nlohmann::json m_JsonObject;
    };
};

#endif