#ifndef __SIRENGINE_FILEREADER_H__
#define __SIRENGINE_FILEREADER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "FilePath.h"
#include "FileStream.h"

namespace SIREngine::FileSystem {
	class CFileReader : public IFileStream
	{
	public:
		CFileReader( void )
		{ }
		CFileReader( CFileReader& other )
		{
			m_FilePath = eastl::move( other.m_FilePath );
			m_hFileHandle = other.m_hFileHandle;
			other.m_hFileHandle = NULL;
		}
		CFileReader( const CFilePath& filePath )
		{ Open( filePath ); }
		virtual ~CFileReader() override
		{ Close(); }

		SIRENGINE_FORCEINLINE CFileReader& operator=( CFileReader& other )
		{
			m_FilePath = eastl::move( other.m_FilePath );
			m_hFileHandle = other.m_hFileHandle;
			return *this;
		}

		virtual bool Open( const CFilePath& filePath ) override;
		virtual void Close( void ) override;
		virtual bool IsOpen( void ) const override;
		virtual size_t GetPosition( void ) const override;
		virtual size_t GetLength( void ) const override;

		size_t Read( void *pBuffer, size_t nBytes );
	};
};

#endif