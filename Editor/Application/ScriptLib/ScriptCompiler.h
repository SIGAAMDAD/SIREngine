#ifndef __VALDEN_SCRIPT_COMPILER_H__
#define __VALDEN_SCRIPT_COMPILER_H__

#pragma once

#include "../Application.h"
#include "ScriptObject.h"

namespace Valden {
	class CScriptCompiler
	{
	public:
		CScriptCompiler( void );
		~CScriptCompiler();

		void Compile( void );
		void Init( void );

		void AddClass( CScriptObject *pClass );

		//
		// execution
		//
		static void Execute( asIScriptFunction *pFunction );
		static void DebugLineCallback( asIScriptContext *pContext );
		static void DumpExeceptionInfo( asIScriptContext *pContext, void *pUserData );

		void Start( void );
		void Stop( void );
		void Pause( void );
		void NextFrame( void );

		static void SetEdit( CScriptObject *pTextEditor );
		SIRENGINE_FORCEINLINE static CVector<CScriptObject *>& GetObjects( void )
		{ return g_ScriptCompiler.m_Objects; }
		SIRENGINE_FORCEINLINE static CScriptObject *GetEditorClass( void )
		{ return g_ScriptCompiler.m_pEditorClass; }
		SIRENGINE_FORCEINLINE static CScriptCompiler& Get( void )
		{ return g_ScriptCompiler; }
	private:
		// debugger
		void PrintCallstack( asIScriptContext *pContext );

		void LoadClasses( void );
		void LoadSubDirs( FileSystem::CFileList *pSubDirs );

		CVector<CScriptObject *> m_Objects;
		CScriptObject *m_pEditorClass;
		
		asIScriptFunction *m_pExecutionPoint;
		CThreadAtomic<bool> m_bExecuting;

		static CScriptCompiler g_ScriptCompiler;
	};
};

#endif