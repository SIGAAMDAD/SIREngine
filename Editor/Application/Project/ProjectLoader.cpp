#define SIRENGINE_NEW_AND_DELETE_OVERRIDE
#include "ProjectManager.h"
#include "../../json/single_include/nlohmann/json.hpp"
#include <Engine/Core/Logging/Logger.h>

namespace Valden {

void CProjectManager::LoadProjectCache( const char *pBuffer, uint64_t nSize )
{
	nlohmann::json data;
		
	if ( nSize ) {
		try {
			data = nlohmann::json::parse( pBuffer, pBuffer + nSize );
		} catch ( const nlohmann::json::exception& e ) {
			SIRENGINE_WARNING( "Error parsing Valden/Projects/Cached.json (%i:%s)", e.id, e.what() );
			SIRENGINE_LOG( "JSON DUMP: %s", (const char *)pBuffer );
		}
	}

	if ( !data.empty() ) {
		if ( !data.contains( "Projects" ) ) {
			SIRENGINE_ERROR( "Invalid ProjectCache file, missing Projects array object" );
		}
		for ( const auto& it : data.at( "Projects" ) ) {
			const std::string projectName = it.get<std::string>();
			if ( !g_pProjectManager->Load( SIREngine::FileSystem::CFilePath( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s",
				projectName.c_str() ) ) ) )
			{
				SIRENGINE_WARNING( "Error loading project \"%s\"!", projectName.c_str() );
			}
		}
	}

	if ( g_pProjectManager->m_ProjectCache.empty() ) {
		g_pProjectManager->Create( "Unnamed" );
	}
}

};