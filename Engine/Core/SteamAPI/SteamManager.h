#ifndef __SIRENGINE_STEAM_MANAGER_H__
#define __SIRENGINE_STEAM_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/EngineApp.h>
#include <Engine/Core/ThreadSystem/Thread.h>
#include <steam/steam_api.h>

typedef struct {
    char szPath[ MAX_RESOURCE_PATH ];
    bool32 bAvailable;
} DlcInfo_t;

typedef struct {
    int64_t m_eAchievementID;
    const char *m_pchAchievementID;
    char m_szName[128];
    char m_szDescription[256];
    bool32 m_bAchieved;
    CResourceDef *m_hIconShader;
} AchievementData_t;

class CSteamManager : public IEngineApp
{
public:
    CSteamManager( void );
    virtual ~CSteamManager() override;

    virtual const char *GetName( void ) const override;
    virtual bool IsActive( void ) const override;
    virtual uint32_t GetState( void ) const override;

    virtual void Init( void ) override;
    virtual void Shutdown( void ) override;
    virtual void Frame( int64_t msec ) override;

    virtual void SaveGame( void ) override;
    virtual void LoadGame( void ) override;

    SIRENGINE_FORCEINLINE AppId_t GetAppId( void ) const {
        return m_nAppId;
    }

    STEAM_CALLBACK( CSteamManager, OnRemoteStorageFileReadAsyncComplete, RemoteStorageFileReadAsyncComplete_t,
        m_CallbackRemoteStorageFileReadAsyncComplete );
    STEAM_CALLBACK( CSteamManager, OnRemoteStorageFileWriteAsyncComplete, RemoteStorageFileWriteAsyncComplete_t,
        m_CallbackRemoteStorageFileWriteAsyncComplete );
    
    STEAM_CALLBACK( CSteamManager, OnLowBatteryPower, LowBatteryPower_t, m_CallbackLowBatteryPower );
    STEAM_CALLBACK( CSteamManager, OnTimedTrialStatus, TimedTrialStatus_t,  m_CallbackTimedTrialStatus );

    static void Message_f( void );
    static void AppInfo_f( void );
private:
    void LoadDLC( void );
    void LoadUserInfo( void );
    void LoadAppInfo( void );
    void LoadInput( void );

    void WriteFile( const char *npath );
    void LoadEngineFiles( void );

    ISteamUser *m_pSteamUser;
    HSteamUser m_nSteamUser;
    uint64_t m_nSteamUserId;

    DlcInfo_t *m_pDlcList;
    int m_nDlcCount;

    char m_szUserDataFolder[ SIRENGINE_MAX_OSPATH ];

    char m_szInstallDir[ SIRENGINE_MAX_OSPATH ];
    AppId_t m_nAppId;
    AppId_t m_nAppBuildId;

    bool32 m_bVipAccount;

    CThreadMutex m_hReadAsyncLock;
    void *m_pReadAsyncBuffer;

    CThreadMutex m_hWriteAsyncLock;
    const char *m_pWriteAsyncFileName;
    void *m_pWriteAsyncBuffer;
    uint64_t m_nWriteAsyncLength;
};

extern CSteamManager *g_pSteamManager;

SIRENGINE_EXPORT_DEMANGLE SIRENGINE_DLL_EXPORT void SteamApp_Init( void );
SIRENGINE_EXPORT_DEMANGLE SIRENGINE_DLL_EXPORT void SteamApp_CloudSave( void );
SIRENGINE_EXPORT_DEMANGLE SIRENGINE_DLL_EXPORT void SteamApp_Frame( void );
SIRENGINE_EXPORT_DEMANGLE SIRENGINE_DLL_EXPORT void SteamApp_Shutdown( void );

#endif