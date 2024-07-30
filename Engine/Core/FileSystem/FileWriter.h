#ifndef __SIRENGINE_FILEWRITER_H__
#define __SIRENGINE_FILEWRITER_H__

#pragma once

#include "FileStream.h"

class CFileWriter : public IFileStream
{
public:
    CFileWriter( void );
    virtual ~CFileWriter() override;
private:
};

#endif