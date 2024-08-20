#ifndef __GL_TEXTURE_H__
#define __GL_TEXTURE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderTexture.h"
#include "../ImageLoader.h"
#include <EASTL/bonus/lru_cache.h>

namespace SIREngine::RenderLib::Backend::OpenGL {
	class GLTexture : public Backend::IRenderTexture
	{
	public:
		GLTexture( const TextureInit_t& textureInfo );
		virtual ~GLTexture();

		void EvictGLResource( void );

		virtual void StreamBuffer( void ) override;

		SIRENGINE_FORCEINLINE GLuint GetOpenGLHandle( void ) const
		{ return m_nTextureID; }

		virtual void LoadFile( const TextureInit_t& textureInfo ) override;
		virtual void Upload( void ) override;
	private:
		bool CanBeEvicted( void ) const;

		GLuint m_hBufferID[2];
		GLuint m_nTextureID;
		GLint m_nBufferIndex;

		bool32 m_bInGPUMemory;
		bool32 m_bCanCreateAsEvicted;
		uint64_t m_nFrameLastRendered;

		GLenum m_nGLTextureFormat;

		CImageLoader m_ImageData;
	};
};

#endif