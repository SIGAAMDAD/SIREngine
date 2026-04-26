#ifndef __SIRENGINE_FILE_CACHE_H__
#define __SIRENGINE_FILE_CACHE_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Core::FileSystem
{
	class FileCache
	{
	public:
		FileCache( void );
		~FileCache();

		void ReadFile( const char *pszFileName, void **pData, size_t *pLength );
	private:
		struct SCacheEntry
		{
			const char *pszFileName;
			size_t nCachedSize;
			void *pMappedData;
		};

		eastl::unordered_map<eastl::fixed_string<char, SIRENGINE_MAX_OSPATH>, SCacheEntry> m_dataCache;
	};
};

#endif