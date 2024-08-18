#ifndef __GENERIC_APPLICATION_H__
#define __GENERIC_APPLICATION_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Util/CString.h>
#include <Engine/Util/CVector.h>
#include <Engine/Core/Logging/Logger.h>
#include <Engine/Core/FileSystem/FilePath.h>
#include <Engine/Core/Events/EventBase.h>
#include <EASTL/unordered_map.h>
#include <Engine/Util/CStackVector.h>

namespace SIREngine {
	class CThread;
	class IEngineApp;
};

namespace SIREngine::Application {
	typedef enum {
		RAPI_OPENGL,
		RAPI_VULKAN,
		RAPI_OPENGLES,
		RAPI_D3D11,
		RAPI_SOFTWARE
	} RenderAPIType_t;

	typedef enum {
		FileMode_ReadOnly,
		FileMode_WriteOnly,
		FileMode_Append,
		FileMode_ReadWrite,
	} FileMode_t;

	typedef enum {
		Crash_SegmentationViolation,
		Crash_BusError,
		Crash_Assertion,
		Crash_Engine,
	} CrashType_t;

	typedef struct {
		/** The amount of physical memory currently available, in bytes. */
		uint64_t nAvailablePhysical;

		/** The amount of virtual memory currently available, in bytes. */
		uint64_t nAvailableVirtual;

		/** The amount of physical memory used by the process, in bytes. */
		uint64_t nUsedPhysical;

		/** The peak amount of physical memory used by the process, in bytes. */
		uint64_t nPeakUsedPhysical;

		/** Total amount of virtual memory used by the process. */
		uint64_t nUsedVirtual;

		/** The peak amount of virtual memory used by the process. */
		uint64_t nPeakUsedVirtual;

		uint64_t nTotalPhysical;

		/** Memory pressure states, useful for platforms in which the available memory estimate
		 	may not take in to account memory reclaimable from closing inactive processes or resorting to swap. */
		enum class EMemoryPressureStatus : uint8_t {
			Unknown,
			Nominal,
			Warning,
			Critical, // high risk of OOM conditions
		};
	} MemoryStats_t;

	typedef struct {
		uint64_t nTotalPhysical;
		uint64_t nTotalVirtual;

		uint64_t nPageSize;
		uint64_t nAddressLimit;

		uint64_t nTotalPhysicalGB;
	} MemoryConstants_t;

	typedef struct {
		const char *pszWindowName;
		const char *pszAppName;
		uint64_t nAppVersion;

		int nWindowPosX;
		int nWindowPosY;
		int nWindowWidth;
		int nWindowHeight;

		unsigned eWindowFlags;
	} ApplicationInfo_t;

	typedef struct {
		uint64_t nSize;

		uint64_t nCreatedTime;
		uint64_t nModifiedTime;
	} FileInfo_t;

	class IGenericApplication
	{
	public:
		IGenericApplication( void );
		virtual ~IGenericApplication();

		virtual void Error( const char *pError ) = 0;

		SIRENGINE_FORCEINLINE ApplicationInfo_t& GetAppInfo( void )
		{ return m_ApplicationInfo; }
		SIRENGINE_FORCEINLINE const ApplicationInfo_t& GetAppInfo( void ) const
		{ return m_ApplicationInfo; }
		SIRENGINE_FORCEINLINE CVector<CString>& GetCommandLine( void )
		{ return m_CommandLineArgs; } 
		SIRENGINE_FORCEINLINE const CVector<CString>& GetCommandLine( void ) const
		{ return m_CommandLineArgs; } 
		SIRENGINE_FORCEINLINE RenderAPIType_t GetRenderAPI( void ) const
		{ return m_nRendererType; }
		SIRENGINE_FORCEINLINE const FileSystem::CFilePath& GetGamePath( void ) const
		{ return m_GamePath; }

		SIRENGINE_FORCEINLINE void SetCommandLine( const CVector<CString>& commandLine )
		{ m_CommandLineArgs = eastl::move( commandLine ); }
		SIRENGINE_FORCEINLINE void SetApplicationArgs( const ApplicationInfo_t& appInfo )
		{ m_ApplicationInfo = appInfo; }

		void ShowErrorWindow( const char *pErrorString );
		bool CheckCommandParm( const CString& name ) const;
		CString GetCommandParmValue( const CString& name ) const;

		virtual void Init( void );
		virtual void Shutdown( void );
		virtual void Run( void );

		virtual void GetFileStats( const CString& fileName, FileInfo_t *pInfo ) = 0;

		virtual FILE *OpenFile( const CString& filePath, const char *mode ) = 0;

		virtual size_t GetOSPageSize( void ) const = 0;

		virtual void *OpenDLL( const char *pName ) = 0;
		virtual void CloseDLL( void *pDLLHandle ) = 0;
		virtual void *GetProcAddress( void *pDLLHandle, const char *pProcName ) = 0;

		virtual size_t GetAllocSize( void *pBuffer ) const = 0;
		virtual void *VirtualAlloc( size_t *nSize, size_t nAlignment ) = 0;
		virtual void VirtualFree( void *pBuffer, size_t nSize ) = 0;
		virtual void CommitMemory( void *pMemory, size_t nOffset, size_t nSize ) = 0;
		virtual void DecommitMemory( void *pMemory, size_t nOffset, size_t nSize ) = 0;
		virtual void SetMemoryReadOnly( void *pMemory, size_t nOffset, size_t nSize ) = 0;

		virtual void *MapFile( void *hFile, size_t *pSize ) = 0;
		virtual void UnmapFile( void *pMemory, size_t nSize ) = 0;

		SIRENGINE_FORCEINLINE virtual void CommitByAddress( void *pMemory, size_t nSize )
		{ CommitMemory( pMemory, (size_t)( ( (byte *)pMemory ) - ( (byte *)pMemory ) ), nSize ); }
		SIRENGINE_FORCEINLINE virtual void DecommitByAddress( void *pMemory, size_t nSize )
		{ DecommitMemory( pMemory, (size_t)( ( (byte *)pMemory ) - ( (byte *)pMemory ) ), nSize ); }

		virtual void *FileOpen( const CString& filePath, FileMode_t nMode ) = 0;
		virtual void FileClose( void *hFile ) = 0;
		virtual size_t FileWrite( const void *pBuffer, size_t nBytes, void *hFile ) = 0;
		virtual size_t FileRead( void *pBuffer, size_t nBytes, void *hFile ) = 0;
		virtual size_t FileSeek( void *hFile, size_t nOffset, int whence ) = 0;
		virtual size_t FileTell( void *hFile ) = 0;
		virtual size_t FileLength( void *hFile ) = 0;

		virtual const MemoryConstants_t& GetMemoryConstants( void ) = 0;
		virtual MemoryStats_t GetMemoryStats( void ) = 0;

		virtual CVector<FileSystem::CFilePath> ListFiles( const FileSystem::CFilePath& dir, bool bDirectoryOnly = false ) = 0;

		virtual void ThreadStart( void *pThread, SIREngine::CThread *, void (SIREngine::CThread::*pFunction)( void ) ) = 0;
		virtual void ThreadJoin( void *pThread, SIREngine::CThread *, uint64_t nTimeout = SIRENGINE_UINT64_MAX ) = 0;

		virtual bool CreateDirectory( const char *pDirectoryPath ) = 0;

		virtual double GetCPUFrequency( void ) = 0;
		virtual uint32_t GetNumberOfCores( void ) = 0;

		virtual void OnOutOfMemory( uint64_t nSize, uint64_t nAlignment ) = 0;

		static CrashType_t nEngineCrashReason;
	protected:
		static void QuitGame( const Events::IEventBase *pEventData );

		ApplicationInfo_t m_ApplicationInfo;
		RenderAPIType_t m_nRendererType;

		FileSystem::CFilePath m_GamePath;

		CVector<CString> m_CommandLineArgs;
		eastl::unordered_map<CString, IEngineApp *> m_ApplicationSystems;
	};

	extern IGenericApplication *g_pApplication;
	extern uint64_t g_nFrameNumber;

	SIRENGINE_FORCEINLINE IGenericApplication *Get( void )
	{ return g_pApplication; }
};

#if !defined(PLATFORM_HEADER)
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_LINUX)
	#include <Engine/Core/Application/Posix/PosixApplication.h>
	#include <Engine/Core/Application/Posix/PosixTypes.h>
#endif
#endif

#endif