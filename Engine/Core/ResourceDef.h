#ifndef __SIRENGINE_RESOURCE_DEF_H__
#define __SIRENGINE_RESOURCE_DEF_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <stdint.h>
#include <stddef.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "Config.h"
#include "FileSystem/FilePath.h"
#include <Engine/RenderLib/RenderLib.h>
#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/Util/CUniquePtr.h>
#include <Engine/RenderLib/Backend/RenderTexture.h>
#include <Engine/Core/FileSystem/MemoryFile.h>

namespace SIREngine {
	typedef enum {
		RES_SHADER,
		RES_SOUND,
		RES_MAP,
		RES_FONT,
		RES_MATERIAL,

		RES_INVALID
	} ResourceType_t;

	typedef enum {
		RS_INVALID,
		RS_DEFAULTED,
		RS_LOADED
	} ResourceState_t;

	class IResourceDef
	{
	public:
		IResourceDef( void )
			: m_nType( RES_INVALID ), m_nState( RS_INVALID )
		{ }
		IResourceDef( const char *pszName, ResourceType_t nType, ResourceState_t nState )
			: m_nType( nType ), m_nState( nState )
		{ SIREngine_strncpyz( m_szName, pszName, sizeof( m_szName ) ); }
		virtual ~IResourceDef()
		{ }

		inline virtual const char *GetName( void ) const
		{ return m_szName; }
		virtual bool IsValid( void ) const
		{ return false; }
		virtual void Reload( const FileSystem::CFilePath& filePath )
		{ }
		virtual void Release( void )
		{ }

		inline ResourceState_t GetState( void ) const
		{ return m_nState; }
		inline ResourceType_t GetType( void ) const
		{ return m_nType; }
	protected:
		char m_szName[MAX_RESOURCE_PATH];
		ResourceType_t m_nType;
		ResourceState_t m_nState;
	};

	class CResourceDef : public IResourceDef
	{
	public:
		CResourceDef( const char *pszName, ResourceType_t nType, ResourceState_t nState )
			: IResourceDef( pszName, nType, nState )
		{ }
		CResourceDef( ResourceType_t nType, ResourceState_t nState )
			: IResourceDef( "", nType, nState )
		{ }
		CResourceDef( const FileSystem::CFilePath& filePath )
		{ Reload( filePath ); }
		virtual ~CResourceDef() override
		{ }
	};

	class CResourceTextDef : public IResourceDef
	{
	public:
		CResourceTextDef( void )
			: m_pBuffer( NULL ), m_nBufLength( 0 )
		{ }
		virtual ~CResourceTextDef() override
		{ }

		virtual uint64_t GetTextLength( void ) const = 0;
		virtual const char *GetText( void ) const = 0;
		virtual void SetText( const char *pText ) = 0;
	protected:
		char *m_pBuffer;
		uint64_t m_nBufLength;
	};

	class CMaterial : public CResourceDef
	{
	public:
		CMaterial( const FileSystem::CFilePath& filePath )
			: CResourceDef( filePath.c_str(), RES_SHADER, RS_INVALID )
		{ Reload( filePath ); }
		CMaterial( void )
			: CResourceDef( "", RES_SHADER, RS_INVALID )
		{ }
		virtual ~CMaterial() override
		{ }

		inline virtual bool IsValid( void ) const override
		{ return m_pTexture != NULL; }
		virtual void Reload( const FileSystem::CFilePath& filePath ) override;
		virtual void Release( void ) override;
	private:
		RenderLib::Backend::IRenderTexture *m_pTexture;
	};

	class CFont : public CResourceTextDef
	{
	public:
		CFont( const FileSystem::CFilePath& fontPath );
		virtual ~CFont() override;


	private:
	};

	class CSoundShader : public CResourceDef
	{
	public:
		
	private:
	};
};

#endif