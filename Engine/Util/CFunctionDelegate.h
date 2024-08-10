#ifndef __CFUNCTION_DELEGATE_H__
#define __CFUNCTION_DELEGATE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

template<typename ReturnType, typename... Args>
class CFunctionDelegate
{
public:
    CFunctionDelegate( void )
    { }
    CFunctionDelegate( ReturnType<Args>&& fn )
        : m_InternalDelegate( fn )
    { }
    ~CFunctionDelegate()
    { }
private:
    ReturnType m_InternalDelegate;
};

#endif