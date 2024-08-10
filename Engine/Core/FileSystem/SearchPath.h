#ifndef __SIRENGINE_SEARCHPATH_H__
#define __SIRENGINE_SEARCHPATH_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "FilePath.h"

namespace FileSystem {
    typedef struct Directory {
        CFilePath path;
        CFilePath gamedir;
    } Directory_t;

    typedef struct SearchPath {
        SearchPath( void )
        { }
        ~SearchPath()
        { }

        Directory_t directory;
        SearchPath *pNext;
    } SearchPath_t;
};

#endif