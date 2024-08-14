#ifndef __SHADER_CACHE_H__
#define __SHADER_CACHE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/ResourceDef.h>
#include "Backend/RenderContext.h"
#include "Backend/RenderShader.h"
#include <EASTL/shared_ptr.h>

namespace SIREngine::RenderLib {
	class CShaderData
	{
	public:
		CShaderData( const eastl::shared_ptr<CResourceDef>& pMaterial )
			: m_pMaterial( pMaterial )
		{
			RenderProgramInit_t init;
			init.pName = pMaterial->GetName();
			m_pShader = Backend::g_pRenderContext->AllocateProgram( init );
		}
		~CShaderData()
		{ delete m_pShader; }

		inline const eastl::shared_ptr<CResourceDef>& GetMaterial( void ) const
		{ return m_pMaterial; }
		inline const Backend::IRenderProgram *GetCacheData( void ) const
		{ return m_pShader; }

		inline eastl::shared_ptr<CResourceDef>& GetMaterial( void )
		{ return m_pMaterial; }
		inline Backend::IRenderProgram *GetCacheData( void )
		{ return m_pShader; }
	private:
		eastl::shared_ptr<CResourceDef> m_pMaterial;
		Backend::IRenderProgram *m_pShader;
	};

	class CShaderCache
	{
	public:
		CShaderCache( void );
		~CShaderCache();

		inline bool IsShaderCompiled( const CString& materialName ) const
		{ return m_CacheData.find( materialName ) != m_CacheData.end(); }

		// compiles the material's shader if not already loaded
		const eastl::shared_ptr<CResourceDef>& GetShader( const CString& materialName );
	private:
		eastl::unordered_map<CString, CShaderData> m_CacheData;
	};
};

#endif