#include "Font.h"
#include <Engine/Core/Util.h>
#include "../ResourceManager.h"

namespace SIREngine::GUILib {

CFont::CFont( const FileSystem::CFilePath& filePath )
	: m_FontData( filePath )
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
	return m_pFont != NULL && m_FontData.GetSize() != 0;
}

void CFont::Reload( void )
{
	if ( !m_FontData.GetSize() ) {
		SIRENGINE_LOG_LEVEL( SyncedResourceLoader, ELogLevel::Warning, "CFont::Reload: Error loading font file!" );
		return;
	}

	ImFontConfig config;
	memset( &config, 0, sizeof( config ) );
	config.FontDataOwnedByAtlas = false;

	m_pFont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF( const_cast<uint8_t *>( m_FontData.GetBuffer() ), m_FontData.GetSize(),
		16.0f, &config );
	if ( !m_pFont ) {
		SIRENGINE_LOG_LEVEL( SyncedResourceLoader, ELogLevel::Warning, "Error creating ImGui Font object!" );
		m_nState = RS_DEFAULTED;
		return;
	}

	m_nState = RS_LOADED;
}

void CFont::Release( void )
{
	if ( m_FontData.GetSize() ) {
		m_FontData.Release();
	}
	ImGui::GetIO().Fonts->Fonts.erase( &m_pFont );
	m_pFont = NULL;
}

};