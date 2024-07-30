#ifndef __SIRENGINE_FILESYSTEM_H__
#define __SIRENGINE_FILESYSTEM_H__

#pragma once

#include <Engine/Util/CString.h>

typedef struct SearchPath {
    
    SearchPath *pNext;
    SearchPath *pPrev;
} SearchPath_t;

class CFileSystem
{
public:
    CFileSystem( void );
    ~CFileSystem();

    const CString& GetResourcePath( void ) const;

    void LoadFile( const CString& fileName );
private:
    CString m_ResourcePath;
};

#endif