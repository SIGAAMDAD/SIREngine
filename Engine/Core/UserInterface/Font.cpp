#include "Font.h"
#include <Engine/Core/Util.h>
#include <Engine/Core/FileSystem/MemoryFile.h>

namespace GUILib {

CFont::CFont( const SIREngine::FileSystem::CFilePath& filePath )
{
	m_nType = RES_FONT;
	m_nState = RS_INVALID;

	SIREngine_strncpyz( m_szName, filePath.c_str(), sizeof( m_szName ) );
	Reload();
}

CFont::~CFont()
{
}

const char *CFont::GetName( void ) const
{
	return m_szName;
}

bool CFont::IsValid( void ) const
{
	return m_pFont != NULL;
}

void CFont::Reload( void )
{
	SIREngine::FileSystem::CMemoryFile file( m_szName );

	SIRENGINE_LOG( "Loading font \"%s\"...", m_szName );

	if ( !file.GetSize() ) {
		SIRENGINE_WARNING( "CFont::Reload: Error loading font file!" );
		return;
	}

	ImFontConfig config;
	memset( &config, 0, sizeof( config ) );
	config.FontDataOwnedByAtlas = false;

	m_pFont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF( const_cast<uint8_t *>( file.GetBuffer() ), file.GetSize(), 16.0f, &config );
	if ( !m_pFont ) {
		SIRENGINE_WARNING( "Error creating ImGui Font object!" );
		m_nState = RS_DEFAULTED;
		return;
	}

	m_nState = RS_LOADED;
}

void CFont::Release( void )
{
}

};