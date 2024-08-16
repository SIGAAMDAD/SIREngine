#ifndef __SIRENGINE_IMAGE_LOADER_H__
#define __SIRENGINE_IMAGE_LOADER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "../RenderCommon.h"
#include <Engine/Core/FileSystem/FilePath.h>
#include <Engine/Core/FileSystem/MemoryFile.h>

namespace SIREngine::RenderLib::Backend {
	class CImageLoader
	{
	public:
		CImageLoader( void )
			: m_nWidth( 0 ), m_nHeight( 0 ), m_nSamples( 0 )
		{ }
		CImageLoader( CImageLoader&& other )
			: m_ImageBuffer( eastl::move( other.m_ImageBuffer ) ),
			m_nWidth( other.m_nWidth ), m_nHeight( other.m_nHeight ),
			m_nSamples( 0 )
		{ }
		CImageLoader( const FileSystem::CFilePath& filePath );
		~CImageLoader();

		inline const CImageLoader& operator=( const CImageLoader& other )
		{
			m_ImageBuffer = other.m_ImageBuffer;
			m_nWidth = other.m_nWidth;
			m_nHeight = other.m_nHeight;
			m_nSamples = other.m_nSamples;
			return *this;
		}
		inline const CImageLoader& operator=( CImageLoader&& other )
		{
			m_ImageBuffer = eastl::move( other.m_ImageBuffer );
			m_nWidth = other.m_nWidth;
			m_nHeight = other.m_nHeight;
			m_nSamples = other.m_nSamples;
			return *this;
		}

		SIRENGINE_FORCEINLINE void *GetBuffer( void )
		{ return m_ImageBuffer.data(); }
		SIRENGINE_FORCEINLINE const void *GetBuffer( void ) const
		{ return m_ImageBuffer.data(); }
		SIRENGINE_FORCEINLINE uint32_t GetWidth( void ) const
		{ return m_nWidth; }
		SIRENGINE_FORCEINLINE uint32_t GetHeight( void ) const
		{ return m_nHeight; }
		SIRENGINE_FORCEINLINE uint32_t GetChannels( void ) const
		{ return m_nSamples; }

		SIRENGINE_FORCEINLINE size_t GetSize( void ) const
		{ return m_ImageBuffer.size(); }

		bool Load( const FileSystem::CFilePath& filePath );
	private:
		static bool LoadJpeg( const FileSystem::CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
			uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
		static bool LoadPNG( const FileSystem::CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
			uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
		static bool LoadTGA( const FileSystem::CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
			uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
		static bool LoadPCX( const FileSystem::CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
			uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
		static bool LoadBMP( const FileSystem::CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
			uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );
		static bool LoadSVG( const FileSystem::CMemoryFile& fileBuffer, CVector<uint8_t>& outBuffer,
			uint32_t& nWidth, uint32_t& nHeight, uint32_t& nSamples );

		CVector<uint8_t> m_ImageBuffer;
		uint32_t m_nWidth;
		uint32_t m_nHeight;
		uint32_t m_nSamples;
	};
};

#endif