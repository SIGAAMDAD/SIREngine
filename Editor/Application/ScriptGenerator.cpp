#include "ScriptGenerator.h"
#include <angelscript/angelscript.h>

namespace Valden {

void CScriptGenerator::PushScriptClass( const NodeClass_t& object )
{
	m_Objects.emplace_back( eastl::addressof( object ) );
}

#define WRITE_INDENT for ( uint64_t i = 0; i < nDepth; i++ ) buffer.append_sprintf( "\t" );

static void WriteVarSet( eastl::string& buffer, uint64_t& nDepth,const Node_t& node )
{
	WRITE_INDENT
	buffer.append_sprintf( "%s = %s;\n", node.Outputs[0].Name.c_str(), node.Inputs[0].Name.c_str() );
}

static void WriteBranch( eastl::string& buffer, uint64_t& nDepth, const Node_t& node )
{
	WRITE_INDENT
	buffer.append_sprintf(
		"if ( %s ) {\n"
	, node.Inputs[1].Name.c_str() );
}

void CScriptGenerator::Write( void )
{
	asIScriptFunction *p;
	eastl::string buffer;

	buffer.reserve( 2*1024*1024 );

	for ( const auto& it : m_Objects ) {
		FileSystem::CFileWriter *hFile = g_pFileSystem->OpenFileWriter(
			SIRENGINE_TEMP_VSTRING( ".../%s.as", it->data.name.c_str() ) );

		if ( !hFile ) {
			SIRENGINE_ERROR( "" );
		}

		// handle includes
		for ( const auto& include : m_Objects ) {
			if ( include == it ) {
				continue; // don't recurse
			}

		}
	}
}

};