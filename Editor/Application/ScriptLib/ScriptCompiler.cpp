#include <angelscript/angelscript.h>
#include "ScriptCompiler.h"
#include "../Project/ProjectManager.h"
#include <scriptbuilder/scriptbuilder.h>
#include <future>

namespace Valden {

SIRENGINE_DEFINE_LOG_CATEGORY( ScriptCompiler, ELogLevel::Info );
static asIScriptEngine *s_pScriptEngine;
static asIScriptContext *s_pScriptContext;
static asIScriptModule *s_pScriptModule;
static CScriptBuilder *s_pScriptBuilder;
static CScriptObject *s_pCurrentObject;
static CVector<std::future<int>> s_ExecutionResults;
static CThread s_ScriptExecutionThread( "ScriptExecution" );

CScriptCompiler CScriptCompiler::g_ScriptCompiler;

CScriptCompiler::CScriptCompiler( void )
{
}

CScriptCompiler::~CScriptCompiler()
{
}

void CScriptCompiler::SetEdit( CScriptObject *pTextEditor )
{
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Spam, "Setting ScriptEditor to class \"%s\"...", pTextEditor->GetName().c_str() );
	g_ScriptCompiler.m_pEditorClass = pTextEditor;
	CTextEditorManager::Get().AddScript( pTextEditor );
}

void CScriptCompiler::AddClass( CScriptObject *pClass )
{
	m_Objects.emplace_back( pClass );
}

void CScriptCompiler::DebugLineCallback( asIScriptContext *pContext )
{

}

void CScriptCompiler::DumpExeceptionInfo( asIScriptContext *pContext, void *pUserData )
{
	CScriptObject *pObject = (CScriptObject *)pUserData;
	asIScriptFunction *pFunction;
	int nLineNumber;
	const char *pErrorString, *pExceptionType;

	pFunction = pContext->GetExceptionFunction();
	nLineNumber = pContext->GetExceptionLineNumber();
	pExceptionType = pContext->GetExceptionString();

	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Warning, "Exception throw in ScriptClass: %s", pObject->GetName().c_str() );
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Warning, "Printing Script Stacktrace..." );
	CScriptCompiler::Get().PrintCallstack( pContext );

	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "Script Execution Exception Information:" );
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "  Module ID: %s", pFunction->GetModuleName() );
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "  Section: %s", pFunction->GetScriptSectionName() );
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "  Function: %s", pFunction->GetName() );
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "  Line: %i", nLineNumber );
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "  Error Type: %s", pExceptionType );
	if ( *pErrorString ) {
		SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "  Error Message: %s", pErrorString );
	}
}

void CScriptCompiler::PrintCallstack( asIScriptContext *pContext )
{

}

void CScriptCompiler::Execute( asIScriptFunction *pFunction )
{
	CThreadMutex Lock;

	if ( s_pScriptContext->GetState() == asEXECUTION_ACTIVE ) {
		// we've current got something running at the moment,
		// lock the context, then push a new state
		s_ExecutionResults.emplace_back( std::async( std::launch::async, [&]( void ) -> int {
			int value;

			s_pScriptContext->PushState();
			s_pScriptContext->Prepare( pFunction );

			// since we're in the editor and not just running the game
			// in a release settings, always have a script debugger
			// present
			s_pScriptContext->SetLineCallback( asFUNCTION( CScriptCompiler::DebugLineCallback ), NULL, asCALL_CDECL );
			
			try {
				switch ( s_pScriptContext->Execute() ) {
				
				};
			} catch ( const std::exception& ) {
			}

			s_pScriptContext->PopState();

			return s_pScriptContext->GetReturnDWord();
		} ) );
	}
}

void CScriptCompiler::Start( void )
{
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "Starting ScriptExecution..." );

	m_bExecuting.store( true );

	s_ScriptExecutionThread.Start(
		[&]( void ) -> void {
			while ( m_bExecuting.load() ) {
				NextFrame();
			}
		}
	);
}

void CScriptCompiler::NextFrame( void )
{
	CThreadMutex Lock;

	CThreadAutoLock<CThreadMutex> _( Lock );
	s_pScriptContext->Prepare( m_pExecutionPoint );
	s_pScriptContext->Execute();
	s_pScriptContext->Unprepare();
}

void CScriptCompiler::Pause( void )
{
	CThreadMutex Lock;

	m_bExecuting.store( false );

	// validate that we're actually running anything
	Lock.Lock();
	if ( s_pScriptContext->GetState() == asEXECUTION_ACTIVE ) {
		// save the current state
		s_pScriptContext->PushState();
		s_ScriptExecutionThread.Join();
	}
	Lock.Unlock();
}

void CScriptCompiler::Stop( void )
{
	Pause();

	s_pScriptContext->PopState();
	s_pScriptContext->Unprepare();
}

static void *AS_MemAlloc( size_t nSize )
{
	return Mem_Alloc( nSize );
}

static void AS_MemFree( void *pMemory )
{
	Mem_Free( pMemory );
}

int AS_IncludeCallback_f( const char *pInclude, const char *pFrom, CScriptBuilder *pBuilder, void *unused )
{
	(void)unused; // shut up compiler

	FileSystem::CMemoryFile file( pInclude );
	if ( !file.GetSize() ) {
		s_pScriptEngine->WriteMessage( s_pCurrentObject->GetName().c_str(),
			0, 0, asMSGTYPE_WARNING, SIRENGINE_TEMP_VSTRING( "Error loading include preprocessor file \"%s\" from \"%s\"", pInclude, pFrom ) );
		return -1;
	}

	pBuilder->AddSectionFromMemory( FileSystem::CFilePath( const_cast<char *>( pInclude ) ).GetFileName().c_str(),
		(const char *)file.GetBuffer(), file.GetSize() );

	s_pScriptEngine->WriteMessage( s_pCurrentObject->GetName().c_str(),
		0, 0, asMSGTYPE_INFORMATION, SIRENGINE_TEMP_VSTRING( "Added include file \"%s\" to \"%s\"\n", pInclude, pFrom ) );
	return 1;
}

void CScriptCompiler::LoadClasses( void )
{
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Verbose, "Loading ScriptClass Objects..." );

	FileSystem::CFileList *pFiles = g_pFileSystem->ListFiles( CProjectManager::Get()->BuildProjectPath( "Scripts/Classes" ), ".as", false );
	if ( pFiles ) {
		m_Objects.reserve( pFiles->GetList().size() );
		for ( const auto& it : pFiles->GetList() ) {
			SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Verbose, "- Loaded ScriptClass file \"%s\"", it.c_str() );
			CSceneView::Get()->PushObject( new CScriptObject( it ) );
		}
		delete pFiles;
	}

	FileSystem::CFileList *pSubDirs = g_pFileSystem->ListFiles( CProjectManager::Get()->BuildProjectPath( "Scripts/Classes" ), NULL, true );
	if ( pSubDirs ) {
		LoadSubDirs( pSubDirs );
		delete pSubDirs;
	}
}

void CScriptCompiler::Init( void )
{
	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "Initializing Script Compiler..." );

	asSetGlobalMemoryFunctions( AS_MemAlloc, AS_MemFree );

	s_pScriptEngine = asCreateScriptEngine();
	if ( !s_pScriptEngine ) {
		SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Fatal, "Error creating asIScriptEngine" );
	}

	s_pScriptContext = s_pScriptEngine->CreateContext();
	if ( !s_pScriptContext ) {
		SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Fatal, "Error creating asIScriptContext" );
	}

	s_pScriptModule = s_pScriptEngine->GetModule( "SIREngineScriptCache", asGM_CREATE_IF_NOT_EXISTS );
	if ( !s_pScriptModule ) {
		SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Fatal, "Error creating asIScriptModule" );
	}

	s_pScriptBuilder = new CScriptBuilder();

	s_pScriptBuilder->SetIncludeCallback( AS_IncludeCallback_f, NULL );

	// initialize configuration
//	s_pScriptEngine->SetEngineProperty( asEP_COMPILER_WARNINGS, CProjectManager::Get()->GetProject()-> )

	//
	// initialize engine api
	//
	

	//
	// load classes
	//
	LoadClasses();
}

class CScriptWriterBinaryStream : public asIBinaryStream
{
public:
	CScriptWriterBinaryStream( FileSystem::CFileWriter *hFile )
		: m_hStream( hFile )
	{ }
	virtual ~CScriptWriterBinaryStream() override
	{ }

	virtual int Read( void *ptr, asUINT size ) override
	{ return 0; }
	virtual int Write( const void *ptr, asUINT size ) override
	{ return m_hStream->Write( ptr, size ); }
private:
	FileSystem::CFileWriter *m_hStream;
};

void CScriptCompiler::Compile( void )
{
	FileSystem::CFileWriter *hFile;

	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "Compiling ScriptObjects..." );

	for ( auto& it : m_Objects ) {
		SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "- Compiling ScriptClass \"%s\"...", it->GetName().c_str() );

		hFile = g_pFileSystem->OpenFileWriter(
			SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Scripts/Cache/%s.asb", CProjectManager::Get()->GetProject()->GetName().c_str(),
				it->GetName().c_str() )
		);

		s_pCurrentObject = it;

		const CString code = it->GetEditor()->GetText();
		s_pScriptBuilder->AddSectionFromMemory( it->GetName().c_str(), code.c_str(), code.size() );

		CScriptWriterBinaryStream writer( hFile );
		s_pScriptModule->SaveByteCode( &writer, false );

		delete hFile;
	}
	
	s_pScriptModule->BindAllImportedFunctions();
	s_pScriptModule->Build();

	hFile = g_pFileSystem->OpenFileWriter(
		SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Scripts/Cache/EntryPoint.sea", CProjectManager::Get()->GetProject()->GetName().c_str() ) );

	delete hFile;
}

};