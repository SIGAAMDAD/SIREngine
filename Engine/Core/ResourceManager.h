#ifndef __SIRENGINE_RESOURCE_MANAGER_H__
#define __SIRENGINE_RESOURCE_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "EngineApp.h"
#include <EASTL/shared_ptr.h>

namespace SIREngine {
	class CMaterial;
	class CSoundShader;

	class CResourceManager : public IEngineApp
	{
	public:
		CResourceManager( void );
		virtual ~CResourceManager() override;

		virtual void Init( void ) override;
		virtual void Shutdown( void ) override;

		void RegisterResourceType( const char *pTypeName, uint32_t nType );
		void RegisterResourceFolder( const char *pDirectory, const char *pExtension );

		const char *GetResourceNameFromType( uint32_t nType ) const;
		uint32_t GetResourceTypeFromName( const char *pTypeName ) const;

		const eastl::shared_ptr<CMaterial>& FindMaterial( const CString& name, bool bMakeDefault = true );
		const eastl::shared_ptr<CSoundShader>& FindSound( const CString& name, bool bMakeDefault = true );
	private:
		typedef struct {
			FileSystem::CFilePath m_Path;
			CString m_DirectoryName;
		} ResourceFolder_t;

		CVector<ResourceFolder_t> m_ResourcePaths;
	};
};

#endif