#ifndef __SIRENGINE_CONSOLE_MANAGER_H__
#define __SIRENGINE_CONSOLE_MANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

class CIniSerializer;

#include "SIREngine.h"
#include <Engine/Core/Serialization/Ini/IniSerializer.h>
#include <EASTL/unordered_map.h>
#include "ThreadSystem/Thread.h"
#include "Util.h"

#define MAX_COMMAND_BUFFER SIRENGINE_UINT16_MAX
#define MAX_COMMAND_ARGS 1024

typedef enum {
	Cvar_Save       = SIRENGINE_BIT( 1 ),   // save to the config file
	Cvar_ReadOnly   = SIRENGINE_BIT( 2 ),   // can't be changed at runtime
	Cvar_Developer  = SIRENGINE_BIT( 3 ),   // can only be changed in developer mode
	Cvar_Module     = SIRENGINE_BIT( 4 ),   // cvar was defined in a module

	Cvar_Default    = 0x0000
} CvarFlags_t;

typedef enum {
	CVG_RENDERER,
	CVG_SOUNDSYSTEM,
	CVG_FILESYSTEM,
	CVG_SYSTEMINFO,
	CVG_USERINFO,
	CVG_NONE, // special case -- user created

	NUMCVARGROUPS
} CvarGroup_t;

typedef enum {
	CvarType_Invalid,
	CvarType_Int,
	CvarType_UInt,
	CvarType_Float,
	CvarType_String,
	CvarType_Bool,
	CvarType_FilePath,

	CvarType_Max
} CvarType_t;


class IConsoleVar;
class IConsoleCmd;

class IConsoleObject
{
public:
	IConsoleObject( void )
	{ }
	virtual ~IConsoleObject()
	{ }

	inline virtual IConsoleVar *AsVariable( void )
	{ return NULL; }
	inline virtual IConsoleCmd *AsCommand( void )
	{ return NULL; }
private:
	CString m_Name;
};

class CConsoleManager
{
public:
	CConsoleManager( void );
	~CConsoleManager();

	//
	// these functions are mainly for the editor and the module system's usage
	//
	IConsoleVar *RegisterCVar( const char *pName, bool bValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVar( const char *pName, uint32_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVar( const char *pName, uint64_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVar( const char *pName, int32_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVar( const char *pName, int64_t nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVar( const char *pName, float fValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVar( const char *pName, const CString& value, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );

	IConsoleVar *RegisterCVarRef( const char *pName, bool& bValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVarRef( const char *pName, uint32_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVarRef( const char *pName, uint64_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVarRef( const char *pName, int32_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVarRef( const char *pName, int64_t& nValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVarRef( const char *pName, float& fValue, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );
	IConsoleVar *RegisterCVarRef( const char *pName, CString& value, uint32_t nFlags, const char *pDescription, CvarGroup_t nGroup );

	// internal engine use only
	void RegisterCVarDefaultValue( IConsoleVar *pCvar );

	void RegisterConsoleCommand( IConsoleCmd *pCommand );

	void LoadConfig( void );
	void SaveConfig( void ) const;

	void ExecuteCommand( const char *pText );
	void CommandCompletion( void );

	SIRENGINE_FORCEINLINE static CConsoleManager& Get( void )
	{ return g_ConsoleManager; }
private:
	const char *SkipWhitespace( const char *pText );

	eastl::unordered_map<CString, IConsoleObject *> m_ObjectList;
	CThreadMutex m_hObjectLock;

	CStaticArray<char, MAX_COMMAND_BUFFER> m_CommandLine;
	CStaticArray<char *, MAX_COMMAND_ARGS> m_CommandArgs;
	CThreadAtomic<uint64_t> m_CommandArgc;
	CThreadMutex m_hCommandLock;

	bool32 m_bHistoryLoaded;
	CVector<CString> m_HistoryEntries;

	const char *m_pSavedLocale;
	CIniSerializer *m_pConfigLoader;

	static CConsoleManager g_ConsoleManager;
};

class IConsoleVar : public IConsoleObject
{
public:
	IConsoleVar( void )
	{ }
	IConsoleVar( const char *pName, const char *pDescription, uint32_t iFlags, CvarGroup_t nGroup )
		: m_Name( pName ), m_Description( pDescription ), m_iFlags( iFlags ), m_nGroup( nGroup )
	{ }
	virtual ~IConsoleVar()
	{ }

	inline CvarType_t GetType( void ) const
	{ return m_nType; }
	inline uint32_t GetFlags( void ) const
	{ return m_iFlags; }
	inline CvarGroup_t GetGroup( void ) const
	{ return m_nGroup; }
	inline const CString& GetName( void ) const
	{ return m_Name; }
	inline const CString& GetDescription( void ) const
	{ return m_Description; }

	virtual const CString GetStringValue( void ) const = 0;

	virtual void Register( void ) = 0;

	virtual void SetValue( int32_t nValue ) {}
	virtual void SetValue( uint32_t nValue ) {}
	virtual void SetValue( float nValue ) {}
	virtual void SetValue( bool bValue ) {}
	virtual void SetValue( const CString& value ) {}
	virtual void SetValue( const FileSystem::CFilePath& value ) {}
protected:
	CString m_Name;
	CString m_Description;
	uint32_t m_iFlags;
	CvarGroup_t m_nGroup;
	CvarType_t m_nType;
	IConsoleObject *m_pObject;
};

template<typename T>
class CVar : public IConsoleVar
{
public:
	inline CVar( void )
	{ }
	inline CVar( const char *pName, const T defaultValue, uint32_t iFlags, const char *pDescription, CvarGroup_t nGroup )
		: IConsoleVar( pName, pDescription, iFlags, nGroup ), m_Value( defaultValue )
	{
		if constexpr ( std::is_same<T, int32_t>() ) {
			m_nType = CvarType_Int;
		}
		else if constexpr ( std::is_same<T, uint32_t>() ) {
			m_nType = CvarType_UInt;
		}
		else if constexpr ( std::is_same<T, float>() ) {
			m_nType = CvarType_Float;
		}
		else if constexpr ( std::is_same<T, bool32>() || std::is_same<T, bool>() ) {
			m_nType = CvarType_Bool;
		}
		else if constexpr ( std::is_same<T, CString>() ) {
			m_nType = CvarType_String;
		}
		else if constexpr ( std::is_same<T, FileSystem::CFilePath>() ) {
			m_nType = CvarType_FilePath;
		}
		else {
			m_nType = CvarType_Invalid;
		}
	}
	inline virtual ~CVar() override
	{ }

	inline virtual const CString GetStringValue( void ) const override
	{
		if constexpr ( std::is_same<T, int32_t>() ) {
			return SIRENGINE_TEMP_VSTRING( "%i", m_Value );
		}
		else if constexpr ( std::is_same<T, uint32_t>() ) {
			return SIRENGINE_TEMP_VSTRING( "%u", m_Value );
		}
		else if constexpr ( std::is_same<T, int64_t>() ) {
			return SIRENGINE_TEMP_VSTRING( "%li", m_Value );
		}
		else if constexpr ( std::is_same<T, uint64_t>() ) {
			return SIRENGINE_TEMP_VSTRING( "%lu", m_Value );
		}
		else if constexpr ( std::is_same<T, float>() ) {
			return SIRENGINE_TEMP_VSTRING( "%0.02f", m_Value );
		}
		else if constexpr ( std::is_same<T, bool32>() || std::is_same<T, bool>() ) {
			return SIRENGINE_TEMP_VSTRING( "%s", m_Value ? "true" : "false" );
		}
		else if constexpr ( std::is_same<T, CString>() ) {
			return m_Value;
		}
		else if constexpr ( std::is_same<T, FileSystem::CFilePath>() ) {
			return m_Value;
		}
		else {
			return "";
		}
	}

	inline void Register( void ) override
	{ CConsoleManager::Get().RegisterCVarDefaultValue( this ); }

	inline T GetValue( void ) const
	{ return m_Value; }
	inline T& GetRef( void )
	{ return m_Value; }
	inline const T& GetRef( void ) const
	{ return m_Value; }

	virtual void SetValue( int32_t nValue ) override
	{
		if constexpr ( std::is_same<T, int32_t>() ) {
			m_Value = nValue;
		}
	}
	virtual void SetValue( uint32_t nValue ) override {
		if constexpr ( std::is_same<T, uint32_t>() ) {
			m_Value = nValue;
		}
	}
	virtual void SetValue( float nValue ) override {
		if constexpr ( std::is_same<T, float>() ) {
			m_Value = nValue;
		}
	}
	virtual void SetValue( bool bValue ) override {
		if constexpr ( std::is_same<T, bool32>() || std::is_same<T, bool>() ) {
			m_Value = bValue;
		}
	}
	virtual void SetValue( const CString& value ) override {
		if constexpr ( std::is_same<T, CString>() ) {
			m_Value = value;
		}
	}
	virtual void SetValue( const FileSystem::CFilePath& value ) override {
		if constexpr ( std::is_same<T, FileSystem::CFilePath>() ) {
			m_Value = value;
		}
	}
private:
	T m_Value;
};

template<typename T>
class CVarRef : public IConsoleVar
{
public:
	inline CVarRef( void )
	{ }
	inline CVarRef( const char *pName, T& valueRef, uint32_t iFlags, const char *pDescription, CvarGroup_t nGroup )
		: IConsoleVar( pName, pDescription, iFlags, nGroup ), m_pRefValue( eastl::addressof( valueRef ) )
	{
		if constexpr ( std::is_same<T, int32_t>() || std::is_same<T, int64_t>() ) {
			m_nType = CvarType_Int;
		}
		else if constexpr ( std::is_same<T, uint32_t>() || std::is_same<T, uint64_t>() ) {
			m_nType = CvarType_UInt;
		}
		else if constexpr ( std::is_same<T, float>() ) {
			m_nType = CvarType_Float;
		}
		else if constexpr ( std::is_same<T, bool32>() || std::is_same<T, bool>() ) {
			m_nType = CvarType_Bool;
		}
		else if constexpr ( std::is_same<T, CString>() ) {
			m_nType = CvarType_String;
		}
		else if constexpr ( std::is_same<T, FileSystem::CFilePath>() ) {
			m_nType = CvarType_FilePath;
		}
		else {
			m_nType = CvarType_Invalid;
		}
	}
	inline virtual ~CVarRef() override
	{ }

	inline void Register( void ) override
	{ CConsoleManager::Get().RegisterCVarDefaultValue( this ); }

	inline virtual const CString GetStringValue( void ) const override
	{
		if constexpr ( std::is_same<T, int32_t>() ) {
			return SIRENGINE_TEMP_VSTRING( "%i", *m_pRefValue );
		}
		else if constexpr ( std::is_same<T, uint32_t>() ) {
			return SIRENGINE_TEMP_VSTRING( "%u", *m_pRefValue );
		}
		else if constexpr ( std::is_same<T, float>() ) {
			return SIRENGINE_TEMP_VSTRING( "%0.02f", *m_pRefValue );
		}
		else if constexpr ( std::is_same<T, bool32>() || std::is_same<T, bool>() ) {
			return SIRENGINE_TEMP_VSTRING( "%s", *m_pRefValue ? "true" : "false" );
		}
		else if constexpr ( std::is_same<T, CString>() ) {
			return *m_pRefValue;
		}
		else if constexpr ( std::is_same<T, FileSystem::CFilePath>() ) {
			return *m_pRefValue;
		}
		else {
			return "";
		}
	}

	inline T GetValue( void ) const
	{ return *m_pRefValue; }
	inline T& GetRef( void )
	{ return *m_pRefValue; }
	inline const T& GetRef( void ) const
	{ return *m_pRefValue; }

	virtual void SetValue( int32_t nValue ) override
	{
		if constexpr ( std::is_same<T, int32_t>() ) {
			*m_pRefValue = nValue;
		}
	}
	virtual void SetValue( uint32_t nValue ) override {
		if constexpr ( std::is_same<T, uint32_t>() ) {
			*m_pRefValue = nValue;
		}
	}
	virtual void SetValue( float nValue ) override {
		if constexpr ( std::is_same<T, float>() ) {
			*m_pRefValue = nValue;
		}
	}
	virtual void SetValue( bool bValue ) override {
		if constexpr ( std::is_same<T, bool32>() || std::is_same<T, bool>() ) {
			*m_pRefValue = bValue;
		}
	}
	virtual void SetValue( const CString& value ) override {
		if constexpr ( std::is_same<T, CString>() ) {
			*m_pRefValue = value;
		}
	}
	virtual void SetValue( const FileSystem::CFilePath& value ) override {
		if constexpr ( std::is_same<T, FileSystem::CFilePath>() ) {
			*m_pRefValue = value;
		}
	}
private:
	T *m_pRefValue;
};


class CCommandSystem
{
public:
	CCommandSystem( void );

	bool ExecuteCommand( const char *pText );
private:
	
};

typedef void (*cmdFunc_Static_t)( void );

class IConsoleCmd
{
public:
	IConsoleCmd( void )
	{ }
	IConsoleCmd( const char *pName, cmdFunc_Static_t fn )
		: m_Name( pName ), m_InternalFunction( [&]{ fn(); } )
	{ }
	virtual ~IConsoleCmd();

	virtual void operator()( void ) = 0;
private:
	CString m_Name;
	eastl::function<void( void )> m_InternalFunction;
};

class CStaticConsoleCmd : public IConsoleCmd
{
public:
	CStaticConsoleCmd( void );
	CStaticConsoleCmd( const char *pName );
	virtual ~CStaticConsoleCmd() override;
private:

};


#endif