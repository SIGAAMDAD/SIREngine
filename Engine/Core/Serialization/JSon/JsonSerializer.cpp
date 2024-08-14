#include "JsonSerializer.h"
#include "Engine/Core/Logging/Logger.h"
#include <Engine/Core/FileSystem/MemoryFile.h>
#include <fstream>

namespace SIREngine::Serialization {

bool CJSonSerializer::Load( const SIREngine::FileSystem::CFilePath& fileName )
{
	FileSystem::CMemoryFile file( fileName );

	try {
		m_JsonObject = nlohmann::json::parse( file.GetBuffer(), file.GetBuffer() + file.GetSize() );
	} catch ( const nlohmann::json::exception& e ) {
		SIRENGINE_WARNING( "Error parsing json file \"%s\" (%i:%s)", fileName.c_str(), e.id, e.what() );
		return false;
	}

	SIRENGINE_LOG( "Parsed json file \"%s\".", fileName.c_str() );

	return true;
}

bool CJSonSerializer::Save( const SIREngine::FileSystem::CFilePath& filePath )
{
	FileSystem::CFileWriter *hFile = g_pFileSystem->OpenFileWriter( filePath );

	if ( !hFile ) {
		return false;
	}

	const CString data = m_JsonObject.dump( 1, '\t' );
	hFile->Write( data.c_str(), data.size() );

	delete hFile;

	return true;
}

};