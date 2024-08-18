#ifndef __GUILIB_FONT_H__
#define __GUILIB_FONT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/ResourceDef.h>
#include <Engine/Core/FileSystem/FileSystem.h>
#include <Engine/Core/FileSystem/MemoryFile.h>
#include <imgui/imgui.h>

namespace SIREngine::GUILib {
	class CFont : public CResourceDef
	{
	public:
		CFont( const FileSystem::CFilePath& filePath );
		virtual ~CFont() override;

		virtual const char *GetName( void ) const override;
	    virtual bool IsValid( void ) const override;
	    virtual void Reload( void ) override;
	    virtual void Release( void ) override;
	private:
		FileSystem::CMemoryFile m_FontData;
		ImFont *m_pFont;
	};
};

#endif