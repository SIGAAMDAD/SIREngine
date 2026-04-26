#ifndef __SIRENGINE_FILESYSTEM_H__
#define __SIRENGINE_FILESYSTEM_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Core::FileSystem
{
	class FileSystem
	{
	public:
		FileSystem( void );
		~FileSystem();

		eastl::vector<eastl::string> ListFiles( const eastl::string& directory, const eastl::string& searchPattern, bool recursive );
	private:
	};
};

#endif