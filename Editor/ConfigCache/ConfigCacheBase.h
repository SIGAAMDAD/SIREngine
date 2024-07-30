#ifndef __CONFIG_CACHE_BASE_H__
#define __CONFIG_CACHE_BASE_H__

#pragma once

#include <Engine/Core/SIREngine.h>

class IConfigCacheBase
{
public:
    IConfigCacheBase( void );
    virtual ~IConfigCacheBase();

    virtual bool Load( const CString& filePath ) = 0;
    virtual void Save( void ) const = 0;
};

#endif