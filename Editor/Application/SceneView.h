#ifndef __VALDEN_SCENE_VIEW_H__
#define __VALDEN_SCENE_VIEW_H__

#pragma once

#include "Application.h"
#include "AssetLib/TextureAtlas.h"
#include "Util/FileTreeView.h"

namespace Valden {
	namespace ESceneObjectType {
		enum : uint32_t {
			None,
			TextureAtlas,
			TextureImage,
			TextureNoise,
			Animation,
			Text,
			Shape,
			Actor,
			Item,
			SyncPoint,
			Light,

			ScriptNameSpace,
			ScriptClass,

			NumTypes
		};
	};

	class ISceneObject
	{
	public:
		ISceneObject( const CString& name )
			: m_Name( name )
		{ }
		virtual ~ISceneObject()
		{ }

		virtual bool Load( void )
		{ return false; }

		virtual inline uint32_t GetType( void ) const
		{ return ESceneObjectType::None; }
		inline const CString& GetName( void ) const
		{ return m_Name; }
		inline CString& GetName( void )
		{ return m_Name; }
		inline CResourceDef *GetResource( void )
		{ return m_pData; }
		inline const CResourceDef *GetResource( void ) const
		{ return m_pData; }
	protected:
		CString m_Name;
		CResourceDef *m_pData;
	};

	class CScriptObject;
	class CScriptNameSpace : public ISceneObject
	{
	public:
		CScriptNameSpace( const CString& name )
			: ISceneObject( name )
		{ }
		virtual ~CScriptNameSpace() override
		{ }

		virtual bool Load( void ) override
		{ return true; }

		inline void RemoveClass( CScriptObject *pClass )
		{ eastl::erase( m_Children, (ISceneObject *)pClass ); }
		inline void AddChildNameSpace( CScriptNameSpace *pChild )
		{ m_Children.emplace_back( pChild ); }
		inline void AddClass( CScriptObject *pClass )
		{ m_Children.emplace_back( pClass ); }
		inline CScriptNameSpace *GetChildNameSpace( const CString& name )
		{
			for ( auto& it : m_Children ) {
				if ( it->GetName() == name && it->GetType() == ESceneObjectType::ScriptNameSpace ) {
					return Cast<CScriptNameSpace>( it );
				}
			}
			return NULL;
		}
		virtual inline uint32_t GetType( void ) const override
		{ return ESceneObjectType::ScriptNameSpace; }
		inline CVector<ISceneObject *>& GetClasses( void )
		{ return m_Children; }
	private:
		CVector<ISceneObject *> m_Children;
	};

	class CSceneView : public IEditorWidget
	{
	public:
		CSceneView( void )
			: IEditorWidget( "Scene" )
		{ }
		virtual ~CSceneView()
		{ }

		virtual void Draw( void ) override;
		virtual void Dock( void ) override;

		bool LoadScene( const FileSystem::CFilePath& directory );
		void Create( const CString& name );
		void Save( void );

		void RemoveObject( ISceneObject *pObject );
		void AddObject( const CString& name, uint32_t nType );
		inline void PushObject( ISceneObject *pObject )
		{ m_SceneObjects[ pObject->GetType() ].emplace_back( pObject ); }

		static void Init( void );

		SIRENGINE_FORCEINLINE static CUniquePtr<CSceneView>& Get( void )
		{ return g_pSceneView; }
	private:
		void DrawObject( ISceneObject *pObject );

		CStaticArray<CVector<ISceneObject *>, ESceneObjectType::NumTypes> m_SceneObjects;

		bool32 m_bRenaming;
		CString *m_pRenameTarget;
		ISceneObject *m_pEditTarget;

		static CUniquePtr<CSceneView> g_pSceneView;
	};
};

#endif