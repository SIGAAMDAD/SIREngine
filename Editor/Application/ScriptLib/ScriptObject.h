#ifndef __VALDEN_SCRIPT_OBJECT_H__
#define __VALDEN_SCRIPT_OBJECT_H__

#pragma once

#include "ScriptTextEditor.h"
#include "../SceneView.h"

namespace Valden {
	class CScriptObject : public ISceneObject
	{
	public:
		CScriptObject( const FileSystem::CFilePath& scriptPath );
		CScriptObject( void );
		virtual ~CScriptObject() override;

		virtual inline uint32_t GetType( void ) const override
		{ return ESceneObjectType::ScriptClass; }
		inline const char *GetDisplayName( void ) const
		{ return m_szDisplayName; }
		inline CTextEditor *GetEditor( void )
		{ return &m_CodeFile; }
		inline CScriptNameSpace *GetNameSpace( void )
		{ return m_pNameSpace; }

		inline void SetNameSpace( CScriptNameSpace *pNameSpace )
		{ m_pNameSpace = pNameSpace; }

		void Edit( void );
		void Save( void );
	private:
		char m_szDisplayName[ SIRENGINE_MAX_OSPATH ];
//		FileSystem::CMemoryFile m_FileBuffer;
		CTextEditor m_CodeFile;
		FileSystem::CFilePath m_FilePath;

		CScriptNameSpace *m_pNameSpace;

		uint64_t m_nFlags;
	};
};

#endif