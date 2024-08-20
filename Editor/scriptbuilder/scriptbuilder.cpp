#include "scriptbuilder.h"
#include <Engine/Core/FileSystem/FileSystem.h>
#include <Engine/Core/FileSystem/MemoryFile.h>

#if defined(_MSC_VER) && !defined(_WIN32_WCE) && !defined(__S3E__)
#include <direct.h>
#endif
#ifdef _WIN32_WCE
#include <windows.h> // For GetModuleFileName()
#endif

#if defined(__S3E__) || defined(__APPLE__) || defined(__GNUC__)
#include <unistd.h> // For getcwd()
#endif

namespace Valden {};
using namespace Valden;

namespace Valden {
	SIRENGINE_DECLARE_LOG_CATEGORY( ScriptCompiler, ELogLevel::Info );
};

CScriptBuilder::CScriptBuilder()
{
	engine = 0;
	module = 0;

	includeCallback = 0;
	includeParam = 0;

	pragmaCallback = 0;
	pragmaParam = 0;
}

void CScriptBuilder::SetIncludeCallback(INCLUDECALLBACK_t callback, void *userParam)
{
	includeCallback = callback;
	includeParam   = userParam;
}

void CScriptBuilder::SetPragmaCallback(PRAGMACALLBACK_t callback, void *userParam)
{
	pragmaCallback = callback;
	pragmaParam = userParam;
}

int CScriptBuilder::StartNewModule(asIScriptEngine *inEngine, const char *moduleName)
{
	if(inEngine == 0 ) return -1;

	engine = inEngine;
	module = inEngine->GetModule(moduleName, asGM_ALWAYS_CREATE);
	if( module == 0 )
		return -1;

	ClearAll();

	return 0;
}

asIScriptEngine *CScriptBuilder::GetEngine()
{
	return engine;
}

asIScriptModule *CScriptBuilder::GetModule()
{
	return module;
}

unsigned int CScriptBuilder::GetSectionCount() const
{
	return (unsigned int)(includedScripts.size());
}

const CString& CScriptBuilder::GetSectionName(unsigned int idx) const
{
	if( idx >= includedScripts.size() ) return "";

#ifdef _WIN32
	eastl::set<CString, ci_less>::const_iterator it = includedScripts.begin();
#else
	eastl::set<CString>::const_iterator it = includedScripts.begin();
#endif
	while( idx-- > 0 ) it++;
	return *it;
}

// Returns 1 if the section was included
// Returns 0 if the section was not included because it had already been included before
// Returns <0 if there was an error
int CScriptBuilder::AddSectionFromFile(const char *filename)
{
	// The file name stored in the set should be the fully resolved name because
	// it is possible to name the same file in multiple ways using relative paths.
	const char *fullpath = SIREngine::FileSystem::CFilePath( const_cast<char *>( filename ) ).GetFileName().c_str();

	if( IncludeIfNotAlreadyIncluded( fullpath ) ) {
		int r = LoadScriptSection( fullpath );
		if( r < 0 )
			return r;
		else
			return 1;
	}

	return 0;
}

// Returns 1 if the section was included
// Returns 0 if the section was not included because it had already been included before
// Returns <0 if there was an error
int CScriptBuilder::AddSectionFromMemory(const char *sectionName, const char *scriptCode, unsigned int scriptLength, int lineOffset)
{
	if( IncludeIfNotAlreadyIncluded(sectionName) )
	{
		int r = ProcessScriptSection(scriptCode, scriptLength, sectionName, lineOffset);
		if( r < 0 )
			return r;
		else
			return 1;
	}

	return 0;
}

int CScriptBuilder::BuildModule()
{
	return Build();
}

void CScriptBuilder::DefineWord(const char *word)
{
	CString sword = word;

	SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "Added preprocessor #define \"%s\"\n", word );
	if( definedWords.find( sword ) == definedWords.end() ) {
		definedWords.insert( sword );
	}
}

void CScriptBuilder::ClearAll()
{
	includedScripts.clear();

	currentClass = "";
	currentNamespace = "";

	foundDeclarations.clear();
	typeMetadataMap.clear();
	funcMetadataMap.clear();
	varMetadataMap.clear();
}

bool CScriptBuilder::IncludeIfNotAlreadyIncluded(const char *filename)
{
	CString scriptFile = filename;
	if ( includedScripts.find(scriptFile) != includedScripts.end() ) {
		// Already included
		return false;
	}

	// Add the file to the set of included sections
	includedScripts.insert(scriptFile);

	return true;
}

int CScriptBuilder::LoadScriptSection(const char *filename)
{
	union {
		char *b;
		void *v;
	} f;
	uint64_t nLength;
	int ret;

	SIREngine::FileSystem::CMemoryFile file( filename );
	if ( !file.GetSize() ) {
		engine->WriteMessage( filename, 0, 0, asMSGTYPE_ERROR, SIRENGINE_TEMP_VSTRING( "Error to loading script file '%s'",
			SIREngine::FileSystem::CFilePath( const_cast<char *>( filename ) ).GetFileName().c_str() ) );
		return -1;
	}

	/*
	// Open the script file
	CString scriptFile = filename;
#if _MSC_VER >= 1500 && !defined(__S3E__)
	FILE *f = 0;
	fopen_s(&f, scriptFile.c_str(), "rb");
#else
	FILE *f = fopen(scriptFile.c_str(), "rb");
#endif
	if( f == 0 )
	{
		// Write a message to the engine's message callback
		CString msg = "Failed to open script file '" + GetAbsolutePath(scriptFile) + "'";
		engine->WriteMessage(filename, 0, 0, asMSGTYPE_ERROR, msg.c_str());

		// TODO: Write the file where this one was included from

		return -1;
	}

	// Determine size of the file
	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	fseek(f, 0, SEEK_SET);

	// On Win32 it is possible to do the following instead
	// int len = _filelength(_fileno(f));

	// Read the entire file
	CString code;
	size_t c = 0;
	if( len > 0 )
	{
		code.resize(len);
		c = fread(&code[0], len, 1, f);
	}

	fclose(f);

	if( c == 0 && len > 0 )
	{
		// Write a message to the engine's message callback
		CString msg = "Failed to load script file '" + GetAbsolutePath(scriptFile) + "'";
		engine->WriteMessage(filename, 0, 0, asMSGTYPE_ERROR, msg.c_str());
		return -1;
	}
	*/

	// Process the script section even if it is zero length so that the name is registered
	ret = ProcessScriptSection( (const char *)file.GetBuffer(), file.GetSize(), filename, 0 );

	return ret;
}

int CScriptBuilder::ProcessScriptSection(const char *script, unsigned int length, const char *sectionname, int lineOffset)
{
	CVector<CString> includes;

	row = col = 0;

	// Perform a superficial parsing of the script first to store the metadata
	if( length )
		modifiedScript.assign(script, length);
	else
		modifiedScript = script;

	// First perform the checks for preprocessor directives to exclude code that shouldn't be compiled
	unsigned int pos = 0;
	int nested = 0;
	while( pos < modifiedScript.size() ) {
		asUINT len = 0;
		asETokenClass t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);

		if( t == asTC_UNKNOWN && modifiedScript[pos] == '#' && (pos + 1 < modifiedScript.size()) )
		{
			int start = pos++;

			// Is this an #if directive?
			t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);

			CString token;
			token.assign(&modifiedScript[pos], len);

			pos += len;

			if( token == "if" )
			{
				t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
				if( t == asTC_WHITESPACE )
				{
					pos += len;
					t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
				}

				if( t == asTC_IDENTIFIER )
				{
					CString word;
					word.assign(&modifiedScript[pos], len);

					// Overwrite the #if directive with space characters to avoid compiler error
					pos += len;
					OverwriteCode(start, pos-start);

					// Has this identifier been defined by the application or not?
					if( definedWords.find(word) == definedWords.end() )
					{
						// Exclude all the code until and including the #endif
						pos = ExcludeCode(pos);
					}
					else
					{
						nested++;
					}
				} else {
					engine->WriteMessage( sectionname, 0, 0, asMSGTYPE_ERROR, "invalid #if directive, no condition" );
				}
			}
			else if( token == "endif" )
			{
				// Only remove the #endif if there was a matching #if
				if( nested > 0 )
				{
					OverwriteCode(start, pos-start);
					nested--;
				} else {
					engine->WriteMessage( sectionname, 0, 0, asMSGTYPE_ERROR, "#endif directive without a #if or #ifdef" );
				}
			}
		}
		else
			pos += len;
	}

	// Preallocate memory
	CString name, declaration;
	CVector<CString> metadata;

	// Then check for meta data and pre-processor directives
	pos = 0;
	while( pos < modifiedScript.size() ) {
		asUINT len = 0;
		asETokenClass t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
		if( t == asTC_COMMENT || t == asTC_WHITESPACE ) {
			pos += len;
			continue;
		}
		CString token;
		token.assign(&modifiedScript[pos], len);

		// Skip possible decorators before class and interface declarations
		if (token == "shared" || token == "abstract" || token == "mixin" || token == "external") {
			pos += len;
			continue;
		}

		// Check if class or interface so the metadata for members can be gathered
		if( currentClass == "" && (token == "class" || token == "interface") )
		{
			// Get the identifier after "class"
			do
			{
				pos += len;
				if( pos >= modifiedScript.size() )
				{
					t = asTC_UNKNOWN;
					break;
				}
				t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
			} while(t == asTC_COMMENT || t == asTC_WHITESPACE);

			if( t == asTC_IDENTIFIER )
			{
				currentClass = modifiedScript.substr(pos,len);

				// Search until first { or ; is encountered
				while( pos < modifiedScript.length() )
				{
					engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);

					// If start of class section encountered stop
					if( modifiedScript[pos] == '{' )
					{
						pos += len;
						break;
					}
					else if (modifiedScript[pos] == ';')
					{
						// The class declaration has ended and there are no children
						currentClass = "";
						pos += len;
						break;
					}

					// Check next symbol
					pos += len;
				}
			}

			continue;
		}

		// Check if end of class
		if( currentClass != "" && token == "}" )
		{
			currentClass = "";
			pos += len;
			continue;
		}

		// Check if namespace so the metadata for members can be gathered
		if( token == "namespace" )
		{
			// Get the identifier after "namespace"
			do
			{
				pos += len;
				t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
			} while(t == asTC_COMMENT || t == asTC_WHITESPACE);

			if( currentNamespace != "" )
				currentNamespace += "::";
			currentNamespace += modifiedScript.substr(pos,len);

			// Search until first { is encountered
			while( pos < modifiedScript.length() )
			{
				engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);

				// If start of namespace section encountered stop
				if( modifiedScript[pos] == '{' )
				{
					pos += len;
					break;
				}

				// Check next symbol
				pos += len;
			}

			continue;
		}

		// Check if end of namespace
		if( currentNamespace != "" && token == "}" )
		{
			size_t found = currentNamespace.rfind( "::" );
			if( found != CString::npos )
			{
				currentNamespace.erase( found );
			}
			else
			{
				currentNamespace = "";
			}
			pos += len;
			continue;
		}

		// Is this the start of metadata?
		if( token == "[" )
		{
			// Get the metadata CString
			pos = ExtractMetadata(pos, metadata);

			// Determine what this metadata is for
			int type;
			ExtractDeclaration(pos, name, declaration, type);

			// Store away the declaration in a map for lookup after the build has completed
			if( type > 0 )
			{
				SMetadataDecl decl(metadata, name, declaration, type, currentClass, currentNamespace);
				foundDeclarations.push_back(decl);
			}
		}
		// Is this a preprocessor directive?
		else if( token == "#" && (pos + 1 < modifiedScript.size()) )
		{
			int start = pos++;

			t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
			if (t == asTC_IDENTIFIER)
			{
				token.assign(&modifiedScript[pos], len);
				if (token == "include")
				{
					pos += len;
					t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
					if (t == asTC_WHITESPACE)
					{
						pos += len;
						t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
					}

					if (t == asTC_VALUE && len > 2 && (modifiedScript[pos] == '"' || modifiedScript[pos] == '\''))
					{
						// Get the include file
						CString includefile;
						includefile.assign(&modifiedScript[pos + 1], len - 2);
						pos += len;

						// Store it for later processing
						includes.push_back(includefile);

						// Overwrite the include directive with space characters to avoid compiler error
						OverwriteCode(start, pos - start);
					}
				}
				else if (token == "pragma")
				{
					// Read until the end of the line
					pos += len;
					for (; pos < modifiedScript.size() && modifiedScript[pos] != '\n'; pos++);

					// Call the pragma callback
					CString pragmaText(&modifiedScript[start + 7], pos - start - 7);
					int r = pragmaCallback ? pragmaCallback(pragmaText, *this, pragmaParam) : -1;
					if (r < 0)
					{
						// TODO: Report the correct line number
						engine->WriteMessage(sectionname, 0, 0, asMSGTYPE_ERROR, "Invalid #pragma directive");
						return r;
					}

					// Overwrite the pragma directive with space characters to avoid compiler error
					OverwriteCode(start, pos - start);
				}
			}
			else
			{
				// Check for lines starting with #!, e.g. shebang interpreter directive. These will be treated as comments and removed by the preprocessor
				if (modifiedScript[pos] == '!')
				{
					// Read until the end of the line
					pos += len;
					for (; pos < modifiedScript.size() && modifiedScript[pos] != '\n'; pos++);

					// Overwrite the directive with space characters to avoid compiler error
					OverwriteCode(start, pos - start);
				}
			}
		}
		// Don't search for metadata/includes within statement blocks or between tokens in statements
		else
		{
			pos = SkipStatement(pos);
		}
	}

	// Build the actual script
	engine->SetEngineProperty(asEP_COPY_SCRIPT_SECTIONS, true);
	module->AddScriptSection(sectionname, modifiedScript.c_str(), modifiedScript.size(), lineOffset);

	if( includes.size() > 0 )
	{
		// If the callback has been set, then call it for each included file
		if( includeCallback )
		{
			for( int n = 0; n < (int)includes.size(); n++ )
			{
				int r = includeCallback(includes[n].c_str(), sectionname, this, includeParam);
				if( r < 0 )
					return r;
			}
		}
		else
		{
			// By default we try to load the included file from the relative directory of the current file

			// Determine the path of the current script so that we can resolve relative paths for includes
			CString path = sectionname;
			size_t posOfSlash = path.find_last_of("/\\");
			if( posOfSlash != CString::npos )
				path.resize(posOfSlash+1);
			else
				path = "";

			// Load the included scripts
			for( int n = 0; n < (int)includes.size(); n++ )
			{
				// If the include is a relative path, then prepend the path of the originating script
				if( includes[n].find_first_of("/\\") != 0 &&
					includes[n].find_first_of(":") == CString::npos )
				{
					includes[n] = path + includes[n];
				}

				// Include the script section
				int r = AddSectionFromFile(includes[n].c_str());
				if( r < 0 )
					return r;
			}
		}
	}

	return 0;
}

int CScriptBuilder::Build()
{
	int r = module->Build();
	if( r < 0 )
		return r;

	// After the script has been built, the metadata CStrings should be
	// stored for later lookup by function id, type id, and variable index
	for( int n = 0; n < (int)foundDeclarations.size(); n++ )
	{
		SMetadataDecl *decl = &foundDeclarations[n];
		module->SetDefaultNamespace(decl->nameSpace.c_str());
		if( decl->type == MDT_TYPE )
		{
			// Find the type id
			int typeId = module->GetTypeIdByDecl(decl->declaration.c_str());
			assert( typeId >= 0 );
			if( typeId >= 0 )
				typeMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(typeId, decl->metadata));
		}
		else if( decl->type == MDT_FUNC )
		{
			if( decl->parentClass == "" )
			{
				// Find the function id
				asIScriptFunction *func = module->GetFunctionByDecl(decl->declaration.c_str());
				assert( func );
				if( func )
					funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
			}
			else
			{
				// Find the method id
				int typeId = module->GetTypeIdByDecl(decl->parentClass.c_str());
				assert( typeId > 0 );
				CHashMap<int, SClassMetadata>::iterator it = classMetadataMap.find(typeId);
				if( it == classMetadataMap.end() )
				{
					classMetadataMap.insert(CHashMap<int, SClassMetadata>::value_type(typeId, SClassMetadata(decl->parentClass)));
					it = classMetadataMap.find(typeId);
				}

				asITypeInfo *type = engine->GetTypeInfoById(typeId);
				asIScriptFunction *func = type->GetMethodByDecl(decl->declaration.c_str());
				assert( func );
				if( func )
					it->second.funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
			}
		}
		else if( decl->type == MDT_VIRTPROP )
		{
			if( decl->parentClass == "" )
			{
				// Find the global virtual property accessors
				asIScriptFunction *func = module->GetFunctionByName(("get_" + decl->declaration).c_str());
				if( func )
					funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
				func = module->GetFunctionByName(("set_" + decl->declaration).c_str());
				if( func )
					funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
			}
			else
			{
				// Find the method virtual property accessors
				int typeId = module->GetTypeIdByDecl(decl->parentClass.c_str());
				assert( typeId > 0 );
				CHashMap<int, SClassMetadata>::iterator it = classMetadataMap.find(typeId);
				if( it == classMetadataMap.end() )
				{
					classMetadataMap.insert(CHashMap<int, SClassMetadata>::value_type(typeId, SClassMetadata(decl->parentClass)));
					it = classMetadataMap.find(typeId);
				}

				asITypeInfo *type = engine->GetTypeInfoById(typeId);
				asIScriptFunction *func = type->GetMethodByName(("get_" + decl->declaration).c_str());
				if( func )
					it->second.funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
				func = type->GetMethodByName(("set_" + decl->declaration).c_str());
				if( func )
					it->second.funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
			}
		}
		else if( decl->type == MDT_VAR )
		{
			if( decl->parentClass == "" )
			{
				// Find the global variable index
				int varIdx = module->GetGlobalVarIndexByName(decl->declaration.c_str());
				assert( varIdx >= 0 );
				if( varIdx >= 0 )
					varMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(varIdx, decl->metadata));
			}
			else
			{
				int typeId = module->GetTypeIdByDecl(decl->parentClass.c_str());
				assert( typeId > 0 );

				// Add the classes if needed
				CHashMap<int, SClassMetadata>::iterator it = classMetadataMap.find(typeId);
				if( it == classMetadataMap.end() )
				{
					classMetadataMap.insert(CHashMap<int, SClassMetadata>::value_type(typeId, SClassMetadata(decl->parentClass)));
					it = classMetadataMap.find(typeId);
				}

				// Add the variable to class
				asITypeInfo *objectType = engine->GetTypeInfoById(typeId);
				int idx = -1;

				// Search through all properties to get proper declaration
				for( asUINT i = 0; i < (asUINT)objectType->GetPropertyCount(); ++i )
				{
					const char *name;
					objectType->GetProperty(i, &name);
					if( decl->declaration == name )
					{
						idx = i;
						break;
					}
				}

				// If found, add it
				assert( idx >= 0 );
				if( idx >= 0 ) it->second.varMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(idx, decl->metadata));
			}
		}
		else if (decl->type == MDT_FUNC_OR_VAR)
		{
			if (decl->parentClass == "")
			{
				// Find the global variable index
				int varIdx = module->GetGlobalVarIndexByName(decl->name.c_str());
				if (varIdx >= 0)
					varMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(varIdx, decl->metadata));
				else
				{
					asIScriptFunction *func = module->GetFunctionByDecl(decl->declaration.c_str());
					assert(func);
					if (func)
						funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
				}
			}
			else
			{
				int typeId = module->GetTypeIdByDecl(decl->parentClass.c_str());
				assert(typeId > 0);

				// Add the classes if needed
				CHashMap<int, SClassMetadata>::iterator it = classMetadataMap.find(typeId);
				if (it == classMetadataMap.end())
				{
					classMetadataMap.insert(CHashMap<int, SClassMetadata>::value_type(typeId, SClassMetadata(decl->parentClass)));
					it = classMetadataMap.find(typeId);
				}

				// Add the variable to class
				asITypeInfo *objectType = engine->GetTypeInfoById(typeId);
				int idx = -1;

				// Search through all properties to get proper declaration
				for (asUINT i = 0; i < (asUINT)objectType->GetPropertyCount(); ++i)
				{
					const char *name;
					objectType->GetProperty(i, &name);
					if (decl->name == name)
					{
						idx = i;
						break;
					}
				}

				// If found, add it
				if (idx >= 0) 
					it->second.varMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(idx, decl->metadata));
				else
				{
					// Look for the matching method instead
					asITypeInfo *type = engine->GetTypeInfoById(typeId);
					asIScriptFunction *func = type->GetMethodByDecl(decl->declaration.c_str());
					assert(func);
					if (func)
						it->second.funcMetadataMap.insert(CHashMap<int, CVector<CString> >::value_type(func->GetId(), decl->metadata));
				}
			}
		}
	}
	module->SetDefaultNamespace("");

	return 0;
}

int CScriptBuilder::SkipStatement(int pos)
{
	asUINT len = 0;

	// Skip until ; or { whichever comes first
	while( pos < (int)modifiedScript.length() && modifiedScript[pos] != ';' && modifiedScript[pos] != '{' )
	{
		engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
		pos += len;
	}

	// Skip entire statement block
	if( pos < (int)modifiedScript.length() && modifiedScript[pos] == '{' )
	{
		pos += 1;

		// Find the end of the statement block
		int level = 1;
		while( level > 0 && pos < (int)modifiedScript.size() )
		{
			asETokenClass t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
			if( t == asTC_KEYWORD )
			{
				if( modifiedScript[pos] == '{' )
					level++;
				else if( modifiedScript[pos] == '}' )
					level--;
			}

			pos += len;
		}
	}
	else
		pos += 1;

	return pos;
}

// Overwrite all code with blanks until the matching #endif
int CScriptBuilder::ExcludeCode(int pos)
{
	asUINT len = 0;
	int nested = 0;
	while( pos < (int)modifiedScript.size() )
	{
		engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
		if( modifiedScript[pos] == '#' )
		{
			modifiedScript[pos] = ' ';
			pos++;

			// Is it an #if or #endif directive?
			engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
			CString token;
			token.assign(&modifiedScript[pos], len);
			OverwriteCode(pos, len);

			if( token == "if" )
			{
				nested++;
			}
			else if( token == "endif" )
			{
				if( nested-- == 0 )
				{
					pos += len;
					break;
				}
			}
		}
		else if( modifiedScript[pos] != '\n' )
		{
			OverwriteCode(pos, len);
		}
		pos += len;
	}

	return pos;
}

// Overwrite all characters except line breaks with blanks
void CScriptBuilder::OverwriteCode(int start, int len)
{
	char *code = &modifiedScript[start];
	for( int n = 0; n < len; n++ )
	{
		if( *code != '\n' )
			*code = ' ';
		code++;
	}
}

int CScriptBuilder::ExtractMetadata(int pos, CVector<CString> &metadata)
{
	metadata.clear();

	// Extract all metadata. They can be separated by whitespace and comments
	for (;;)
	{
		CString metadataString = "";

		// Overwrite the metadata with space characters to allow compilation
		modifiedScript[pos] = ' ';

		// Skip opening brackets
		pos += 1;

		int level = 1;
		asUINT len = 0;
		while (level > 0 && pos < (int)modifiedScript.size())
		{
			asETokenClass t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
			if (t == asTC_KEYWORD)
			{
				if (modifiedScript[pos] == '[')
					level++;
				else if (modifiedScript[pos] == ']')
					level--;
			}

			// Copy the metadata to our buffer
			if (level > 0)
				metadataString.append(&modifiedScript[pos], len);

			// Overwrite the metadata with space characters to allow compilation
			if (t != asTC_WHITESPACE)
				OverwriteCode(pos, len);

			pos += len;
		}

		metadata.push_back(metadataString);

		// Check for more metadata. Possibly separated by comments
		asETokenClass t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
		while (t == asTC_COMMENT || t == asTC_WHITESPACE)
		{
			pos += len;
			t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
		}

		if (modifiedScript[pos] != '[')
			break;
	}

	return pos;
}

int CScriptBuilder::ExtractDeclaration(int pos, CString &name, CString &declaration, int &type)
{
	declaration = "";
	type = 0;

	int start = pos;

	CString token;
	asUINT len = 0;
	asETokenClass t = asTC_WHITESPACE;

	// Skip white spaces, comments, and leading decorators
	do
	{
		pos += len;
		t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
		token.assign(&modifiedScript[pos], len);
	} while ( t == asTC_WHITESPACE || t == asTC_COMMENT || 
	          token == "private" || token == "protected" || 
	          token == "shared" || token == "external" || 
	          token == "final" || token == "abstract" );

	// We're expecting, either a class, interface, function, or variable declaration
	if( t == asTC_KEYWORD || t == asTC_IDENTIFIER )
	{
		token.assign(&modifiedScript[pos], len);
		if( token == "interface" || token == "class" || token == "enum" )
		{
			// Skip white spaces and comments
			do
			{
				pos += len;
				t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
			} while ( t == asTC_WHITESPACE || t == asTC_COMMENT );

			if( t == asTC_IDENTIFIER )
			{
				type = MDT_TYPE;
				declaration.assign(&modifiedScript[pos], len);
				pos += len;
				return pos;
			}
		}
		else
		{
			// For function declarations, store everything up to the start of the 
			// statement block, except for succeeding decorators (final, override, etc)

			// For variable declaration store just the name as there can only be one

			// We'll only know if the declaration is a variable or function declaration
			// when we see the statement block, or absense of a statement block.
			bool hasParenthesis = false;
			int nestedParenthesis = 0;
			declaration.append(&modifiedScript[pos], len);
			pos += len;
			for(; pos < (int)modifiedScript.size();)
			{
				t = engine->ParseToken(&modifiedScript[pos], modifiedScript.size() - pos, &len);
				token.assign(&modifiedScript[pos], len);
				if (t == asTC_KEYWORD)
				{
					if (token == "{" && nestedParenthesis == 0)
					{
						if (hasParenthesis)
						{
							// We've found the end of a function signature
							type = MDT_FUNC;
						}
						else
						{
							// We've found a virtual property. Just keep the name
							declaration = name;
							type = MDT_VIRTPROP;
						}
						return pos;
					}
					if ((token == "=" && !hasParenthesis) || token == ";")
					{
						if (hasParenthesis)
						{
							// The declaration is ambigous. It can be a variable with initialization, or a function prototype
							type = MDT_FUNC_OR_VAR;
						}
						else
						{
							// Substitute the declaration with just the name
							declaration = name;
							type = MDT_VAR;
						}
						return pos;
					}
					else if (token == "(")
					{
						nestedParenthesis++;

						// This is the first parenthesis we encounter. If the parenthesis isn't followed
						// by a statement block, then this is a variable declaration, in which case we
						// should only store the type and name of the variable, not the initialization parameters.
						hasParenthesis = true;
					}
					else if (token == ")")
					{
						nestedParenthesis--;
					}
				}
				else if( t == asTC_IDENTIFIER )
				{
					name = token;
				}

				// Skip trailing decorators
				if( !hasParenthesis || nestedParenthesis > 0 || t != asTC_IDENTIFIER || (token != "final" && token != "override") )
					declaration += token;

				pos += len;
			}
		}
	}

	return start;
}

const CVector<CString>& CScriptBuilder::GetMetadataForType(int typeId)
{
	CHashMap<int,CVector<CString> >::iterator it = typeMetadataMap.find(typeId);
	if( it != typeMetadataMap.end() )
		return it->second;

	return CVector<CString>();
}

const CVector<CString>& CScriptBuilder::GetMetadataForFunc(asIScriptFunction *func)
{
	if( func )
	{
		CHashMap<int,CVector<CString> >::iterator it = funcMetadataMap.find(func->GetId());
		if( it != funcMetadataMap.end() )
			return it->second;
	}

	return CVector<CString>();
}

const CVector<CString>& CScriptBuilder::GetMetadataForVar(int varIdx)
{
	CHashMap<int,CVector<CString> >::iterator it = varMetadataMap.find(varIdx);
	if( it != varMetadataMap.end() )
		return it->second;

	return CVector<CString>();
}

const CVector<CString>& CScriptBuilder::GetMetadataForTypeProperty(int typeId, int varIdx)
{
	CHashMap<int, SClassMetadata>::iterator typeIt = classMetadataMap.find(typeId);
	if(typeIt == classMetadataMap.end()) return CVector<CString>();

	CHashMap<int, CVector<CString> >::iterator propIt = typeIt->second.varMetadataMap.find(varIdx);
	if(propIt == typeIt->second.varMetadataMap.end()) return CVector<CString>();

	return propIt->second;
}

const CVector<CString>& CScriptBuilder::GetMetadataForTypeMethod(int typeId, asIScriptFunction *method)
{
	if( method )
	{
		CHashMap<int, SClassMetadata>::iterator typeIt = classMetadataMap.find(typeId);
		if (typeIt == classMetadataMap.end()) return CVector<CString>();

		CHashMap<int, CVector<CString> >::iterator methodIt = typeIt->second.funcMetadataMap.find(method->GetId());
		if(methodIt == typeIt->second.funcMetadataMap.end()) return CVector<CString>();

		return methodIt->second;
	}

	return CVector<CString>();
}


END_AS_NAMESPACE


