#ifndef __VALDEN_PROJECT_DATA_H__
#define __VALDEN_PROJECT_DATA_H__

#pragma once

#include "../Application.h"

namespace Valden {
	typedef struct {

	} AudioConfig_t;

	typedef struct {
		bool bAllowIntegerDivision;
		bool bAllowUnsafeReferences;
		bool bOptimizeByteCode;
		bool bAllowCompilerWarnings;
		uint32_t nMaxStackSize;
	} AngelScriptConfig_t;

	typedef struct {
		CString ProjectName;
		CString Description;

		FileSystem::CFilePath Directory;

		bool bDisableStdOut;
		bool bDisableStdErr;

		AngelScriptConfig_t ScriptingConfig;
	} ProjectInfo_t;

	class CProjectData
	{
	public:
		CProjectData( void );
		~CProjectData();

		inline const FileSystem::CFilePath& GetPath( void ) const
		{ return m_Info.Directory; }

		inline void SetName( const CString& name )
		{ m_Info.ProjectName = name; }
		inline const CString& GetName( void ) const
		{ return m_Info.ProjectName; }

		void InitDirectoryStructure( void );
		void Save( void );
		bool Load( const CString& projectName );
	private:
		ProjectInfo_t m_Info;
		Serialization::CIniSerializer *m_pIniData;
	};
};

#endif