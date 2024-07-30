#ifndef __WINDOWS_APPLICATION_H__
#define __WINDOWS_APPLICATION_H__

#pragma once

#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

class CWindowsApplication : public IGenericApplication
{
public:
    CWindowsApplication( const ApplicationInfo_t& appInfo, const CVector<CString>& cmdLine );
    virtual ~CWindowsApplication() override;

    virtual size_t GetOSPageSize( void ) const override;

    virtual void Init( void ) override;
    virtual void Run( void ) override;

    virtual size_t GetAllocSize( void *pBuffer ) const override;
    virtual void *VirtualAlloc( size_t nSize, size_t nAlignment ) override;
    virtual void VirtualFree( void *pBuffer ) override;
};

namespace PlatformTypes {
    typedef SRWLOCK mutex_t;
    typedef CONDITION_VARIABLE condition_variable_t;
    typedef HANDLE thread_t;
    typedef HANDLE file_t;
};

#endif