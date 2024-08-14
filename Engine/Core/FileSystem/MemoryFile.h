#ifndef __SIRENGINE_MEMORYFILE_H__
#define __SIRENGINE_MEMORYFILE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Memory/Allocators/SlabAllocator.h>
#include <Engine/Memory/Allocators/MemoryBuffer.h>
#include "FileStream.h"
#include "FileReader.h"
#include "FileSystem.h"

namespace SIREngine::FileSystem {
	class CMemoryFile
	{
	public:
		CMemoryFile( const FileSystem::CFilePath& filePath )
		{ Open( filePath ); }
		~CMemoryFile()
		{ }

		bool Open( const FileSystem::CFilePath& filePath )
		{
			g_pFileSystem->LoadFile( filePath, &m_pCachedBuffer, &m_nSize );
			if ( !m_pCachedBuffer || !m_nSize ) {
				return false;
			}
			return true;
		}

		const uint8_t *GetBuffer( void ) const
		{ return m_pCachedBuffer; }
		uint64_t GetSize( void ) const
		{ return m_nSize; }
	private:
		uint64_t m_nSize;
		uint8_t *m_pCachedBuffer;
	};
};

#endif