#ifndef __VALDEN_PROJECT_MANAGER_H__
#define __VALDEN_PROJECT_MANAGER_H__

#pragma once

#include "../Application.h"
#include "ProjectData.h"
#include <EASTL/shared_ptr.h>

namespace Valden {
	class CProjectManager
	{
	public:
		CProjectManager( void );
		~CProjectManager();

		static void Init( void );
		static void SaveCache( void );

		void Create( const CString& projectName );
		bool Load( const CString& projectName );
		bool Load( const SIREngine::FileSystem::CFilePath& projectName );

		SIRENGINE_FORCEINLINE const eastl::shared_ptr<CProjectData>& GetProject( void ) const
		{ return m_pCurrentProject; }

		SIRENGINE_FORCEINLINE static eastl::unique_ptr<CProjectManager>& Get( void )
		{ return g_pProjectManager; }
	private:
		void LoadProjectCache( const char *pBuffer, uint64_t nSize );
		bool IsProjectLoaded( const CString& projectName ) const;

		CVector<eastl::shared_ptr<CProjectData>> m_ProjectCache;
		eastl::shared_ptr<CProjectData> m_pCurrentProject;

		static eastl::unique_ptr<CProjectManager> g_pProjectManager;
	};
	SIRENGINE_DECLARE_LOG_CATEGORY( ProjectManager, ELogLevel::Type::Info );
};

#endif