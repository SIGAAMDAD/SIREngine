#ifndef __SIRENGINE_MEMORYFILE_H__
#define __SIRENGINE_MEMORYFILE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Memory/Allocators/HunkAllocator.h>
#include <Engine/Memory/Allocators/MemoryBuffer.h>
#include "FileStream.h"
#include "FileReader.h"
#include "FileSystem.h"

namespace SIREngine::FileSystem {
	class CMemoryFile
	{
	public:
		CMemoryFile( const FileSystem::CFilePath& filePath )
			: m_FilePath( filePath )
		{ Open( m_FilePath ); }
		CMemoryFile( void )
		{ }
		~CMemoryFile()
		{ Release(); }

		SIRENGINE_FORCEINLINE const FileSystem::CFilePath& GetPath( void ) const
		{ return m_FilePath; }

		bool Open( const FileSystem::CFilePath& filePath )
		{
			m_FilePath = filePath;
			g_pFileSystem->LoadFile( m_FilePath, &m_pCachedBuffer, &m_nSize );
			if ( !m_pCachedBuffer || !m_nSize ) {
				return false;
			}
			return true;
		}

		void Release( void )
		{
			CFileCache *pCache = g_pFileSystem->GetFileCache( m_FilePath );
			if ( !pCache ) {
				return;
			}
			pCache->ReleaseCachedFile( m_FilePath );
		}

		const uint8_t *GetBuffer( void ) const
		{ return m_pCachedBuffer; }
		uint64_t GetSize( void ) const
		{ return m_nSize; }
	private:
		FileSystem::CFilePath m_FilePath;
		uint64_t m_nSize;
		uint8_t *m_pCachedBuffer;
	};
};

#endif