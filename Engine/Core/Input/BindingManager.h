#ifndef __SIRENGINE_BINDING_MANAGER_H__
#define __SIRENGINE_BINDING_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/Events/KeyCodes.h>
#include <Engine/Util/CVector.h>
#include <Engine/Util/CString.h>
#include <Engine/Core/Logging/Logger.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unique_ptr.h>
#include <Engine/Core/ConsoleManager.h>

#define MAX_BIND_NAME 128

namespace SIREngine::Input {
	typedef struct KeyBind {
		KeyBind( const CString& name, const CString& downCommand, const CString& upCommand )
			: pDownCommand( CConsoleManager::Get().GetConsoleCommand( name ) ),
			pUpCommand( CConsoleManager::Get().GetConsoleCommand( name ) )
		{ }

		char szName[ MAX_BIND_NAME ];
		IConsoleCmd *pDownCommand;
		IConsoleCmd *pUpCommand;
	} KeyBind_t;

	class CBindSet
	{
	public:
		CBindSet( void )
		{ }
		~CBindSet()
		{ }

		bool Load( const Serialization::CIniSerializer& iniReader );

		SIRENGINE_FORCEINLINE const eastl::unordered_map<Events::KeyNum_t, KeyBind_t>& GetBinds( void ) const
		{ return m_Bindings; }

		SIRENGINE_FORCEINLINE const KeyBind_t *GetBind( Events::KeyNum_t nKeyID ) const
		{
			auto it = m_Bindings.find( nKeyID );
			if ( it != m_Bindings.end() ) {
				return &it->second;
			}
			return NULL;
		}
		SIRENGINE_FORCEINLINE KeyBind_t *GetBind( Events::KeyNum_t nKeyID )
		{
			auto it = m_Bindings.find( nKeyID );
			if ( it != m_Bindings.end() ) {
				return &it->second;
			}
			return NULL;
		}
	private:
		eastl::unordered_map<Events::KeyNum_t, KeyBind_t> m_Bindings;
	};

	class CBindingManager
	{
	public:
		CBindingManager( void );
		~CBindingManager();

		void LoadBindings( void );

		SIRENGINE_FORCEINLINE const eastl::unique_ptr<CBindSet>& GetSet( const CString& name ) const
		{
			auto it = m_BindMap.find( name );
			if ( it != m_BindMap.end() ) {
				return it->second;
			}
			SIRENGINE_WARNING( "No bindset for mapping \"%s\"!", name.c_str() );
			return NULL;
		}
	private:
		eastl::unordered_map<CString, eastl::unique_ptr<CBindSet>> m_BindMap;
	};
};

#endif