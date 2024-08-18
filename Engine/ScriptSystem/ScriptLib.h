#ifndef __SIRENGINE_SCRIPTLIB_HPP__
#define __SIRENGINE_SCRIPTLIB_HPP__

#include <Engine/Core/EngineApp.h>
#include <angelscript/angelscript.h>
#include <Engine/Util/CUniquePtr.h>
#include <Engine/Util/CHashMap.h>

#if defined(SIR_ENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

namespace SIREngine::ScriptLib {
	typedef void (*ScriptRegistryFunc_t)( asIScriptEngine *pEngine );

	SIRENGINE_DECLARE_LOG_CATEGORY( ScriptLib, ELogLevel::Info );

	class CScriptModule;

	class CScriptManager : public IEngineApp
	{
	public:
		CScriptManager( void );
		virtual ~CScriptManager() override;

		virtual const char *GetName( void ) const override;
		virtual bool IsActive( void ) const override;
		virtual uint32_t GetState( void ) const override;

		virtual void Init( void ) override;
		virtual void Shutdown( void ) override;
		virtual void Frame( int64_t msec ) override;

		virtual void SaveGame( void ) override;
		virtual void LoadGame( void ) override;

		SIRENGINE_FORCEINLINE void AddRegistryFunc( const char *pClassName, ScriptRegistryFunc_t Func )
		{
			m_FuncAPIRegisters.emplace_back(
				eastl::make_pair<const char *, ScriptRegistryFunc_t>(
					eastl::move( pClassName ), eastl::move( Func )
				)
			);
		}

		SIRENGINE_FORCEINLINE static CUniquePtr<CScriptManager>& Get( void )
		{ return g_pScriptLib; }
	private:
		CVector<eastl::pair<const char *, ScriptRegistryFunc_t>> m_FuncAPIRegisters;
		CHashMap<CString, CScriptModule *> m_Modules;
		asIScriptEngine *m_pEngine;

		static CUniquePtr<CScriptManager> g_pScriptLib;
	};
};

#endif