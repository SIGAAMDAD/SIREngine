#include "JsonCache.h"
#include <Engine/Core/FileSystem/MemoryFile.h>

using namespace SIREngine::Serialization;
using namespace SIREngine::FileSystem;

namespace SIREngine::Serialization {

CJsonCache *g_pJSonCache;

void CJsonCache::InitJSonCache( void )
{
	g_pJSonCache = new CJsonCache();

	FileSystem::CFileList *pJsonList = g_pFileSystem->ListFiles( "Resources/Scripts", "json" );
	if ( !pJsonList ) {
		return;
	}

	SIRENGINE_LOG( "Initializing JSon object cache..." );

	g_pJSonCache->m_JsonCache.reserve( pJsonList->GetList().size() );
	for ( const auto& it : pJsonList->GetList() ) {
		nlohmann::json data;
		CMemoryFile file( it );

		if ( !file.GetSize() ) {
			continue;
		}

		try {
			data = eastl::move( nlohmann::json::parse( file.GetBuffer(), file.GetBuffer() + file.GetSize() ) );
		} catch ( const nlohmann::json::exception& e ) {
			SIRENGINE_WARNING( "Error parsing json file \"%s\":\n\t[id] %i\n\t[message] %s", it.c_str(), e.id, e.what() );
			continue;
		}

		g_pJSonCache->m_JsonCache.try_emplace( it, eastl::move( data ) );
	}

	delete pJsonList;
}

const nlohmann::json& CJsonCache::GetJsonObject( const FileSystem::CFilePath& filePath )
{
	const auto it = g_pJSonCache->m_JsonCache.find( filePath );
	if ( it != g_pJSonCache->m_JsonCache.cend() ) {
		return it->second;
	} else {
		SIRENGINE_WARNING( "Invalid json path \"%s\"", filePath.c_str() );
	}
	return g_pJSonCache->emptyJSon;
}

};