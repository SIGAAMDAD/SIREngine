#ifndef __SIRENGINE_FILEWRITER_H__
#define __SIRENGINE_FILEWRITER_H__

#pragma once

#include "FileStream.h"

namespace FileSystem {
    class CFileWriter : public IFileStream
    {
    public:
        CFileWriter( void );
        virtual ~CFileWriter() override;

        size_t Write( const void *pBuffer, size_t nBytes );
        void Flush( void );
    private:
        FILE *m_hFile;
    };
};

#endif