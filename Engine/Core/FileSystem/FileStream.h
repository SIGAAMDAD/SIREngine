#ifndef __SIRENGINE_FILESTREAM_H__
#define __SIRENGINE_FILESTREAM_H__

#pragma once

#include <Engine/Util/CString.h>
#include "FilePath.h"

namespace FileSystem {
    class IFileStream
    {
    public:
        IFileStream( void );
        IFileStream( const IFileStream& other );
        virtual ~IFileStream();

        const IFileStream& operator=( const IFileStream& other );

        virtual bool Open( const CFilePath& filePath ) = 0;
        virtual void Close( void ) = 0;
        virtual bool IsOpen( void ) const = 0;

        virtual size_t GetPosition( void ) const = 0;
        virtual size_t GetLength( void ) const = 0;
    private:
        IFileStream( IFileStream&& );
    };
};

#endif