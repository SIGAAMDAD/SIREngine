#ifndef __VALDEN_SCENE_VIEW_H__
#define __VALDEN_SCENE_VIEW_H__

#pragma once

#include "Application.h"
#include "AssetLib/TextureAtlas.h"
#include <EASTL/shared_ptr.h>
#include "Util/FileTreeView.h"

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
		inline CResourceDef *GetResource( void )
		{ return m_pData; }
		inline const CResourceDef *GetResource( void ) const
		{ return m_pData; }
	private:
		CString m_Name;
		ESceneObjectType m_nType;
		CResourceDef *m_pData;
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
		void AddObject( ESceneObjectType nType );

		static void Init( void );

		SIRENGINE_FORCEINLINE static CUniquePtr<CSceneView>& Get( void )
		{ return g_pSceneView; }
	private:
		void DrawObject( CSceneObject& object );

		static CUniquePtr<CSceneView> g_pSceneView;
	};
};

#endif