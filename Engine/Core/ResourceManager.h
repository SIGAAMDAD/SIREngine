#ifndef __SIRENGINE_RESOURCE_MANAGER_H__
#define __SIRENGINE_RESOURCE_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "EngineApp.h"
#include <EASTL/shared_ptr.h>
#include "ResourceDef.h"

namespace SIREngine {
	class CMaterial;
	class CSoundShader;
	class CResourceDef;

	typedef struct ResourceLoader {
		ResourceLoader( void )
		{ }
		~ResourceLoader()
		{ }

		FileSystem::CFilePath FilePath;
//		CUniquePtr<CResourceDef> *pResourcePointer;
		CResourceDef *ResourceData;
		ResourceType_t nType;
	} ResourceLoader_t;

	class CResourceManager : public IEngineApp
	{
	public:
		CResourceManager( void );
		virtual ~CResourceManager() override;

		virtual const char *GetName( void ) const override
		{ return "ResourceManager"; }
        virtual bool IsActive( void ) const override
		{ return true; }
        virtual uint32_t GetState( void ) const override
		{ return 0; }

		virtual void SaveGame( void ) override
		{ }
		virtual void LoadGame( void ) override
		{ }

		virtual void Init( void ) override
		{ }
		virtual void Shutdown( void ) override
		{ }

		virtual void Frame( int64_t msec ) override
		{ }

		void RegisterResourceType( const char *pTypeName, uint32_t nType );
		void RegisterResourceFolder( const char *pDirectory, const char *pExtension );

		const char *GetResourceNameFromType( uint32_t nType ) const;
		uint32_t GetResourceTypeFromName( const char *pTypeName ) const;

		void AddResource( ResourceLoader_t& resourceData );
		void BeginLoad( void );
		void EndLoad( void );

		const CMaterial *FindMaterial( const CString& name, bool bMakeDefault = true );
		const CSoundShader *FindSound( const CString& name, bool bMakeDefault = true );
		const CResourceDef *FindResource( const CString& name );

		SIRENGINE_FORCEINLINE static CResourceManager& Get( void )
		{ return g_ResourceManager; }
	private:
		static void LoadThread( void );

//		typedef struct {
//			FileSystem::CFilePath m_Path;
//			CString m_DirectoryName;
//		} ResourceFolder_t;
		CVector<ResourceLoader_t> m_Loaders;
//		CVector<ResourceFolder_t> m_ResourcePaths;

		static CResourceManager g_ResourceManager;
	};

	SIRENGINE_DECLARE_LOG_CATEGORY( SyncedResourceLoader, ELogLevel::Info );
};

#endif