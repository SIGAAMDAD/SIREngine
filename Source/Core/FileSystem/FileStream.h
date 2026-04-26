#ifndef __SIRENGINE_FILESTREAM_H__
#define __SIRENGINE_FILESTREAM_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Core::FileSystem
{
	class FileStream
	{
	public:
		FileStream( const eastl::string& filePath );
		virtual ~FileStream() = 0;

		virtual void Open( const eastl::string& filePath ) = 0;
	protected:
		eastl::string m_Path;
	};
};

#endif