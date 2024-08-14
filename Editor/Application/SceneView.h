#ifndef __VALDEN_SCENE_VIEW_H__
#define __VALDEN_SCENE_VIEW_H__

#pragma once

#include "Application.h"
#include "AssetLib/TextureAtlas.h"
#include <EASTL/shared_ptr.h>

namespace Valden {
	enum class ESceneObjectType {
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
	};

	class CSceneObject
	{
	public:
		CSceneObject( void )
		{ }
		~CSceneObject()
		{ }

		bool Load( void );

		inline ESceneObjectType GetType( void ) const
		{ return m_nType; }
		inline const CString& GetName( void ) const
		{ return m_Name; }
		inline eastl::shared_ptr<CResourceDef>& GetResource( void )
		{ return m_Data; }
		inline const eastl::shared_ptr<CResourceDef>& GetResource( void ) const
		{ return m_Data; }
	private:
		CString m_Name;
		ESceneObjectType m_nType;
		eastl::shared_ptr<CResourceDef> m_Data;
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

		bool LoadScene( const SIREngine::FileSystem::CFilePath& directory );
		void Create( const CString& name );

		static void Init( void );

		SIRENGINE_FORCEINLINE static eastl::unique_ptr<CSceneView>& Get( void )
		{ return g_pSceneView; }
	private:
		void DrawObject( CSceneObject& object );

		CVector<CSceneObject> m_Resources;
		CVector<CSceneObject> m_SceneData;

		static eastl::unique_ptr<CSceneView> g_pSceneView;
	};
};

#endif