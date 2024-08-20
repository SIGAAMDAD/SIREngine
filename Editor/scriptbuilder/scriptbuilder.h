#ifndef SCRIPTBUILDER_H
#define SCRIPTBUILDER_H

//---------------------------
// Compilation settings
//

// Set this flag to turn on/off metadata processing
//  0 = off
//  1 = on
#ifndef AS_PROCESS_METADATA
#define AS_PROCESS_METADATA 1
#endif

// TODO: Implement flags for turning on/off include directives and conditional programming



//---------------------------
// Declaration
//

#include <Engine/Util/CString.h>
#include <Engine/Util/CHashMap.h>
#include <Engine/Util/CVector.h>
#include <angelscript/angelscript.h>
#include <EASTL/set.h>

#if defined(_MSC_VER) && _MSC_VER <= 1200
// disable the annoying warnings on MSVC 6
#pragma warning (disable:4786)
#endif

class CScriptBuilder;

// This callback will be called for each #include directive encountered by the
// builder. The callback should call the AddSectionFromFile or AddSectionFromMemory
// to add the included section to the script. If the include cannot be resolved
// then the function should return a negative value to abort the compilation.
typedef int (*INCLUDECALLBACK_t)(const char *include, const char *from, CScriptBuilder *builder, void *userParam);

// This callback will be called for each #pragma directive encountered by the builder.
// The application can interpret the pragmaText and decide what do to based on that.
// If the callback returns a negative value the builder will report an error and abort the compilation.
typedef int(*PRAGMACALLBACK_t)(const CString& pragmaText, CScriptBuilder& builder, void *userParam);

// Helper class for loading and pre-processing script files to
// support include directives and metadata declarations
class CScriptBuilder
{
public:
	CScriptBuilder();

	// Start a new module
	int StartNewModule(asIScriptEngine *engine, const char *moduleName);

	// Load a script section from a file on disk
	// Returns  1 if the file was included
	//          0 if the file had already been included before
	//         <0 on error
	int AddSectionFromFile(const char *filename);

	// Load a script section from memory
	// Returns  1 if the section was included
	//          0 if a section with the same name had already been included before
	//         <0 on error
	int AddSectionFromMemory(const char *sectionName,
							 const char *scriptCode,
							 unsigned int scriptLength = 0,
							 int lineOffset = 0);

	// Build the added script sections
	int BuildModule();

	// Returns the engine
	asIScriptEngine *GetEngine();

	// Returns the current module
	asIScriptModule *GetModule();

	// Register the callback for resolving include directive
	void SetIncludeCallback(INCLUDECALLBACK_t callback, void *userParam);

	// Register the callback for resolving pragma directive
	void SetPragmaCallback(PRAGMACALLBACK_t callback, void *userParam);

	// Add a pre-processor define for conditional compilation
	void DefineWord(const char *word);

	// Enumerate included script sections
	unsigned int GetSectionCount() const;
	const CString&  GetSectionName(unsigned int idx) const;

	// Get metadata declared for classes, interfaces, and enums
	const CVector<CString>& GetMetadataForType(int typeId);

	// Get metadata declared for functions
	const CVector<CString>& GetMetadataForFunc(asIScriptFunction *func);

	// Get metadata declared for global variables
	const CVector<CString>& GetMetadataForVar(int varIdx);

	// Get metadata declared for class variables
	const CVector<CString>& GetMetadataForTypeProperty(int typeId, int varIdx);

	// Get metadata declared for class methods
	const CVector<CString>& GetMetadataForTypeMethod(int typeId, asIScriptFunction *method);

	inline unsigned int GetRow( void ) const {
		return row;
	}
	inline unsigned int GetColumn( void ) const {
		return col;
	}
protected:
	void ClearAll();
	int  Build();
	int  ProcessScriptSection(const char *script, unsigned int length, const char *sectionname, int lineOffset);
	int  LoadScriptSection(const char *filename);
	bool IncludeIfNotAlreadyIncluded(const char *filename);

	int  SkipStatement(int pos);

	int  ExcludeCode(int start);
	void OverwriteCode(int start, int len);

	asIScriptEngine           *engine;
	asIScriptModule           *module;
	CString                modifiedScript;

	INCLUDECALLBACK_t  includeCallback;
	void              *includeParam;

	PRAGMACALLBACK_t  pragmaCallback;
	void             *pragmaParam;

	int  ExtractMetadata(int pos, CVector<CString> &outMetadata);
	int  ExtractDeclaration(int pos, CString &outName, CString &outDeclaration, int &outType);

	enum METADATATYPE {
		MDT_TYPE = 1,
		MDT_FUNC = 2,
		MDT_VAR = 3,
		MDT_VIRTPROP = 4,
		MDT_FUNC_OR_VAR = 5
	};

	// Temporary structure for storing metadata and declaration
	struct SMetadataDecl {
		SMetadataDecl( const CVector<CString>& m, const CString& n, const CString& d, int t, const CString& c, const CString& ns )
			: metadata(m), name(n), declaration(d), type(t), parentClass(c), nameSpace(ns) {}
		CVector<CString> metadata;
		CString              name;
		CString              declaration;
		int                      type;
		CString              parentClass;
		CString              nameSpace;
	};
	CVector<SMetadataDecl> foundDeclarations;
	CString currentClass;
	CString currentNamespace;

	unsigned int row, col;

	// Storage of metadata for global declarations
	CHashMap<int, CVector<CString> > typeMetadataMap;
	CHashMap<int, CVector<CString> > funcMetadataMap;
	CHashMap<int, CVector<CString> > varMetadataMap;

	// Storage of metadata for class member declarations
	struct SClassMetadata {
		SClassMetadata(const CString& aName) : className(aName) {}
		CString className;
		CHashMap<int, CVector<CString> > funcMetadataMap;
		CHashMap<int, CVector<CString> > varMetadataMap;
	};
	CHashMap<int, SClassMetadata> classMetadataMap;

#ifdef _WIN32
	// On Windows the filenames are case insensitive so the comparisons to
	// avoid duplicate includes must also be case insensitive. True case insensitive
	// is not easy as it must be language aware, but a simple implementation such
	// as strcmpi should suffice in almost all cases.
	//
	// ref: http://www.gotw.ca/gotw/029.htm
	// ref: https://msdn.microsoft.com/en-us/library/windows/desktop/dd317761(v=vs.85).aspx
	// ref: http://site.icu-project.org/

	// TODO: Strings by default are treated as UTF8 encoded. If the application choses to
	//       use a different encoding, the comparison algorithm should be adjusted as well

	struct ci_less
	{
		bool operator()(const CString &a, const CString &b) const
		{
			return _stricmp(a.c_str(), b.c_str()) < 0;
		}
	};
	eastl::set<CString, ci_less> includedScripts;
#else
	eastl::set<CString>      includedScripts;
#endif

	eastl::set<CString>      definedWords;
};

END_AS_NAMESPACE

#endif
