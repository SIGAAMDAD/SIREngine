#include <Editor/Host/SIREditorSceneJSON.h>
#include <Editor/Host/SIREditorSceneWorkspace.h>

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <assert.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdio.h>
#include <string>

int main( void )
{
	constexpr uint64_t N_MIB = UINT64_C( 1024 ) * 1024;
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 128 * N_MIB, 64 * N_MIB, 2 * N_MIB, 2 };
	assert( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "EditorJSONFile", 96 * N_MIB, 56 * N_MIB, SIR_MEMORY_DOMAIN_EDITOR );
	assert( pSegment != nullptr );
	SIREditorSceneWorkspace workspace;
	assert( workspace.initialize( pSegment ) );

	const std::string sSource = SIR_EDITOR_TEST_UI_SCENE;
	const std::string sNonce = std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() );
	const std::filesystem::path sTarget = std::filesystem::temp_directory_path() / ( "sir-editor-snapshot-" + sNonce + ".json" );
	const std::string sOutput = sTarget.string();
	const std::filesystem::path sStaging = sOutput + ".tmp";
	std::filesystem::remove( sTarget );
	std::filesystem::remove( sStaging );

	assert( !sirEditorWriteSceneJSONFile( workspace, sSource.c_str(), sOutput.c_str() ) );
	assert( !std::filesystem::exists( sTarget ) );
	assert( workspace.openTextFile( sSource.c_str() ) == SIR_EDITOR_WORKSPACE_OK );
	assert( !sirEditorWriteSceneJSONFile( workspace, sSource.c_str(), sSource.c_str() ) );
	assert( !sirEditorWriteSceneJSONFile( workspace, sSource.c_str(), "" ) );
	assert( !sirEditorWriteSceneJSONFile( workspace, sSource.c_str(), nullptr ) );

	const std::string sBadTarget =
	    ( std::filesystem::temp_directory_path() / ( "sir-no-such-parent-" + sNonce ) / "snapshot.json" ).string();
	assert( !sirEditorWriteSceneJSONFile( workspace, sSource.c_str(), sBadTarget.c_str() ) );
	assert( !std::filesystem::exists( sBadTarget ) );
	assert( sirEditorWriteSceneJSONFile( workspace, sSource.c_str(), sOutput.c_str() ) );
	assert( std::filesystem::exists( sTarget ) );
	assert( !std::filesystem::exists( sStaging ) );

	std::ifstream file( sTarget, std::ios::binary );
	assert( file.good() );
	std::string sContent( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
	assert( sContent.find( "\"schema\":1" ) != std::string::npos );
	assert( sContent.find( "\"nodes\":[" ) != std::string::npos );
	assert( sContent.find( "\"resources\":[" ) != std::string::npos );
	file.close();
	assert( std::filesystem::remove( sTarget ) );
	assert( !std::filesystem::exists( sStaging ) );

	workspace.close();
	assert( workspace.shutdown() );
	memory.shutdown();
	puts( "PASS: atomic native editor JSON file publication" );
	return 0;
}
