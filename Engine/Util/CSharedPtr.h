#ifndef __CSHARED_PTR_H__
#define __CSHARED_PTR_H__

#include <Engine/Core/SIREngine.h>
#include "CRefCount.h"

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

template<typename T>
class CSharedPtr
{
public:
private:
    T *m_pData;
    CRefCount m_hRefCount;
};

#endif