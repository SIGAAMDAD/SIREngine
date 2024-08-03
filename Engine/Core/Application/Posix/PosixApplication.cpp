#include "PosixApplication.h"
#include <sys/mman.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <backtrace.h>
#include <cxxabi.h>
#include <malloc.h>
#include <errno.h>
#include <Engine/Memory/Backend/TagArenaAllocator.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>

#define OOM_MEMORY_BACKUP_POOL_SIZE 4*1024

CPosixApplication *g_pApplication;

void *CPosixApplication::pOOMBackup;
size_t CPosixApplication::nOOMBackupSize;

#define MAX_SYMBOL_LENGTH 4096

static bool32 g_bBacktraceError = false;
static FILE *g_pBacktraceOutput = NULL;
static backtrace_state *g_pBacktraceState = NULL;

void *operator new[]( size_t nBytes, char const *, int, unsigned int, char const *, int )
{
    return ::operator new[]( nBytes );
}

void *operator new[]( size_t nBytes, unsigned long, unsigned long, char const*, int, unsigned int, char const*, int )
{
    return ::operator new[]( nBytes );
}

static void bt_error_callback( void *data, const char *msg, int errnum )
{
    fprintf( stderr, "libbacktrace ERROR: %d - %s\n", errnum, msg );
    g_bBacktraceError = true;
}

static void bt_syminfo_callback( void *data, uintptr_t pc, const char *symname,
								 uintptr_t symval, uintptr_t symsize )
{
    if ( g_bBacktraceError ) {
        return;
    }

	if ( symname != NULL ) {
		int status;
		// [glnomad] 10/6/2023: fixed buffer instead of malloc'd buffer, risky however
		char name[MAX_SYMBOL_LENGTH];
		memset( name, 0, sizeof( name ) );
		size_t length = sizeof( name );
		abi::__cxa_demangle( symname, name, &length, &status );
		if ( name[0] ) {
			symname = name;
		}
		if ( g_pBacktraceOutput ) {
			fprintf( g_pBacktraceOutput, "  %-8zu %s\n", pc, symname );
		}
		fprintf( stdout, "  %-8zu %s\n", pc, symname );
	} else {
		if ( g_pBacktraceOutput ) {
			fprintf( g_pBacktraceOutput, "%-8zu (unknown symbol)\n", pc );
		}
		fprintf( stdout, "  %-8zu (unknown symbol)\n", pc );
	}
}

static int bt_pcinfo_callback( void *data, uintptr_t pc, const char *filename, int lineno, const char *function )
{
    if ( g_bBacktraceError ) {
        return 0;
    }

	if ( data != NULL ) {
		int *hadInfo = (int *)data;
		*hadInfo = (function != NULL);
	}

	if ( function != NULL ) {
		int status;
		// [glnomad] 10/6/2023: fixed buffer instead of malloc'd buffer, risky however
		char name[MAX_SYMBOL_LENGTH];
		memset( name, 0, sizeof( name ) );
		size_t length = sizeof( name );
		abi::__cxa_demangle( function, name, &length, &status );
		if ( name[0] ) {
			function = name;
		}

		const char *fileNameSrc = strstr( filename, "/src/" );
		if ( fileNameSrc != NULL ) {
			filename = fileNameSrc+1; // I want "src/bla/blub.cpp:42"
		}
		if ( g_pBacktraceOutput ) {
			fprintf( g_pBacktraceOutput, "  %-8zu %-16s:%-8d %s\n", pc, filename, lineno, function );
		}
		fprintf( stdout, "  %-8zu %-16s:%-8d %s\n", pc, filename, lineno, function );
	}

	return 0;
}

static void bt_error_dummy( void *data, const char *msg, int errnum )
{
	//CrashPrintf("ERROR-DUMMY: %d - %s\n", errnum, msg);
}

static int bt_simple_callback( void *data, uintptr_t pc )
{
	int pcInfoWorked;

    pcInfoWorked = 0;
	// if this fails, the executable doesn't have debug info, that's ok (=> use bt_error_dummy())
	backtrace_pcinfo( g_pBacktraceState, pc, bt_pcinfo_callback, bt_error_dummy, &pcInfoWorked );
	if ( !pcInfoWorked ) { // no debug info? use normal symbols instead
		// yes, it would be easier to call backtrace_syminfo() in bt_pcinfo_callback() if function == NULL,
		// but some libbacktrace versions (e.g. in Ubuntu 18.04's g++-7) don't call bt_pcinfo_callback
		// at all if no debug info was available - which is also the reason backtrace_full() can't be used..
		backtrace_syminfo( g_pBacktraceState, pc, bt_syminfo_callback, bt_error_callback, NULL );
	}

	return 0;
}

void DumpStacktrace( void )
{
    if ( g_pBacktraceState != NULL ) {
        backtrace_simple( g_pBacktraceState, 2, bt_simple_callback, bt_error_callback, NULL );
    }
}

void CPosixApplication::CatchSignal( int nSignum )
{
    switch ( nSignum ) {
    case SIGSEGV:
        g_pApplication->Error( "Segmentation Violation Caught" );
        break;
    case SIGBUS:
        g_pApplication->Error( "Bus Error Caught" );
        break;
    case SIGABRT:
        g_pApplication->Error( "Caught assertion" );
        break;
    };
}

CPosixApplication::CPosixApplication()
{
    signal( SIGSEGV, CPosixApplication::CatchSignal );
    signal( SIGABRT, CPosixApplication::CatchSignal );
    signal( SIGBUS, CPosixApplication::CatchSignal );

    g_pBacktraceState = backtrace_create_state( "SIREngine", false, bt_error_callback, NULL );

    GetPwd();
}

CPosixApplication::~CPosixApplication()
{
}

void SIRENGINE_ATTRIBUTE(format(printf, 2, 3)) CPosixApplication::Error( const char *fmt, ... )
{
    va_list argptr;
    char buf[8192];
    char msg[1024];
    int len;

    va_start( argptr, fmt );
    SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
    va_end( argptr );

    len = SIREngine_snprintf( buf, sizeof( buf ) - 1, "ERROR: %s\n", msg );

    write( STDERR_FILENO, buf, len );

    DumpStacktrace();
    _Exit( EXIT_FAILURE );
}

void SIRENGINE_ATTRIBUTE(format(printf, 2, 3)) CPosixApplication::Warning( const char *fmt, ... )
{
    va_list argptr;

    fprintf( stderr, "WARNING: " );

    va_start( argptr, fmt );
    vfprintf( stderr, fmt, argptr );
    va_end( argptr );
}

void SIRENGINE_ATTRIBUTE(format(printf, 2, 3)) CPosixApplication::Log( const char *fmt, ... )
{
    va_list argptr;

    va_start( argptr, fmt );
    vfprintf( stdout, fmt, argptr );
    va_end( argptr );
}

void *CPosixApplication::OpenDLL( const char *pName )
{
    void *pDLLHandle;
    char szPath[ SIRENGINE_MAX_OSPATH ];

    if ( !pName || !*pName ) {
        Error( "OpenDLL: invalid name" );
    }

    // dlopen requires the dll's path to start with a "./"
    if ( *pName != '.' && *( pName + 1 ) != '/' ) {
        SIREngine_snprintf( szPath, sizeof( szPath ) - 1, "./%s", pName );
    } else {
        SIREngine_strncpyz( szPath, pName, sizeof( szPath ) );
    }

    pDLLHandle = dlopen( szPath, RTLD_NOW | RTLD_LOCAL );
    if ( !pDLLHandle ) {
        SIRENGINE_WARNING( "Error opening dynamic shared object library \"%s\"", szPath );
    }

    return pDLLHandle;
}

void *CPosixApplication::GetProcAddress( void *pDLLHandle, const char *pName )
{
    void *pDLLProc;
    const char *pErrorString;
    char szBuffer[1024];
    uint64_t nlen;

    if ( !pDLLHandle || !pName || *pName == '\0' ) {
        return NULL;
    }
    
    dlerror(); // clear the old error state
    pDLLProc = dlsym( pDLLHandle, pName );
    pErrorString = dlerror();
    if ( pErrorString ) {
        nlen = strlen( pName );
        if ( nlen >= sizeof( szBuffer ) ) {
            return NULL;
        }
        
        szBuffer[0] = '_';
        strcpy( szBuffer + 1, pName );
        dlerror(); // clear the old error state
        pDLLProc = dlsym( pDLLHandle, pName );
    }

    return pDLLProc;
}

void CPosixApplication::CloseDLL( void *pDLLHandle )
{
    if ( pDLLHandle != NULL ) {
        dlclose( pDLLHandle );
    } else {
        SIRENGINE_WARNING( "CloseDLL: NULL handle" );
    }
}

size_t CPosixApplication::GetOSPageSize( void ) const
{
    return sysconf( _SC_PAGESIZE );
}

size_t CPosixApplication::GetAllocSize( void *pBuffer ) const
{
    size_t nSize;
    
    if ( pBuffer ) {
        nSize = malloc_usable_size( pBuffer );
    } else {
        return 0;
    }

    return nSize;
}

void *CPosixApplication::VirtualAlloc( size_t *nSize, size_t nAlignment )
{
    void *pMemory;

    *nSize = SIRENGINE_PAD( *nSize + sizeof( size_t ), nAlignment );
    if ( *nSize % GetOSPageSize() ) {
        // ensure that the allocation is aligned by the OS page size
        *nSize = SIRENGINE_PAD( *nSize, GetOSPageSize() );
    }

    pMemory = mmap( NULL, *nSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
    if ( pMemory == MAP_FAILED || pMemory == NULL ) {
        Error( "mmap failed: %s", strerror( errno ) );
    }
    *(size_t *)pMemory = *nSize;

    return (void *)( (size_t *)pMemory + 1 );
}

void CPosixApplication::VirtualFree( void *pBuffer )
{
    size_t *nSize = ( (size_t *)pBuffer - 1 );
    if ( munmap( nSize, *nSize ) == -1 ) {
        Error( "munmap failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::CommitMemory( void *pMemory, size_t nOffset, size_t nSize )
{
    if ( madvise( ( (uint8_t *)pMemory ) + nOffset, nSize, MADV_WILLNEED ) != 0 ) {
        Error( "madvise failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::DecommitMemory( void *pMemory, size_t nOffset, size_t nSize )
{
    if ( madvise( ( (uint8_t *)pMemory ) + nOffset, nSize, MADV_DONTNEED ) != 0 ) {
        Error( "madvise failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::SetMemoryReadOnly( void *pMemory, size_t nOffset, size_t nSize )
{
    if ( mprotect( ( (byte *)pMemory ) + nOffset, nSize, PROT_READ ) == -1 ) {
        Error( "mprotect failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::MutexLock( void *pMutex )
{
    if ( pthread_mutex_lock( (pthread_mutex_t *)pMutex ) == -1 ) {
        Error( "pthread_mutex_lock failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::MutexUnlock( void *pMutex )
{
    if ( pthread_mutex_unlock( (pthread_mutex_t *)pMutex ) == -1 ) {
        Error( "pthread_mutex_unlock failed: %s", strerror( errno ) );
    }
}

bool CPosixApplication::MutexTryLock( void *pMutex )
{
    return pthread_mutex_trylock( (pthread_mutex_t *)pMutex );
}

void CPosixApplication::MutexInit( void *pMutex )
{
    if ( pthread_mutex_init( (pthread_mutex_t *)pMutex, NULL ) == -1 ) {

    }
}

void CPosixApplication::MutexShutdown( void *pMutex )
{
    if ( pthread_mutex_destroy( (pthread_mutex_t *)pMutex ) == -1 ) {

    }
}

void CPosixApplication::MutexRWUnlock( void *pMutex )
{
    if ( pthread_rwlock_unlock( (pthread_rwlock_t *)pMutex ) == -1 ) {

    }
}

void CPosixApplication::MutexWriteLock( void *pMutex )
{
    if ( pthread_rwlock_wrlock( (pthread_rwlock_t *)pMutex ) == -1 ) {

    }
}

void CPosixApplication::MutexReadLock( void *pMutex )
{
    if ( pthread_rwlock_rdlock( (pthread_rwlock_t *)pMutex ) == -1 ) {

    }
}

bool CPosixApplication::MutexRWTryWriteLock( void *pMutex )
{
    if ( pthread_rwlock_trywrlock( (pthread_rwlock_t *)pMutex ) == -1 ) {

    }
}

bool CPosixApplication::MutexRWTryReadLock( void *pMutex )
{
    if ( pthread_rwlock_tryrdlock( (pthread_rwlock_t *)pMutex ) == -1 ) {
        
    }
}

void CPosixApplication::MutexRWInit( void *pMutex )
{
    if ( pthread_rwlock_init( (pthread_rwlock_t *)pMutex, NULL ) == -1 ) {

    }
}

void CPosixApplication::MutexRWShutdown( void *pMutex )
{
    if ( pthread_rwlock_destroy( (pthread_rwlock_t *)pMutex ) == -1 ) {

    }
}

void CPosixApplication::ConditionVarWait( void *pConditionVar, void *pMutex )
{
    pthread_cond_wait( (pthread_cond_t *)pConditionVar, (pthread_mutex_t *)pMutex );
}

void CPosixApplication::ConditionVarInit( void *pConditionVar )
{
    if ( pthread_cond_init( (pthread_cond_t *)pConditionVar, NULL ) == -1 ) {

    }
}

void CPosixApplication::ConditionVarShutdown( void *pConditionVar )
{
    if ( pthread_cond_destroy( (pthread_cond_t *)pConditionVar ) == -1 ) {

    }
}

void *CPosixApplication::ThreadFunction( void *pArgument )
{
    ThreadFunc_t pFunction = *(ThreadFunc_t *)pArgument;
    pFunction();
    return NULL;
}

void CPosixApplication::ThreadStart( void *pThread, ThreadFunc_t pFunction )
{
    if ( pthread_create( (pthread_t *)pThread, NULL, CPosixApplication::ThreadFunction, &pFunction ) == -1 ) {
    }
}

void CPosixApplication::ThreadJoin( void *pThread, uint64_t nTimeout )
{
    void *pReturn;
    struct timespec ts;

    ts.tv_nsec = nTimeout * 1000000;

    pthread_timedjoin_np( *(pthread_t *)pThread, &pReturn, &ts );
}

FILE *CPosixApplication::OpenFile( const CString& filePath, const char *mode )
{
    return fopen( filePath.c_str(), mode );
}

void *CPosixApplication::FileOpen( const CString& filePath, FileMode_t nMode )
{
    PlatformTypes::file_t hFile;
    int flags;

    flags = O_BINARY;
    switch ( nMode ) {
    case FileMode_ReadOnly:
        flags |= O_RDONLY;
        break;
    case FileMode_WriteOnly:
        flags |= O_WRONLY | O_CREAT;
        break;
    case FileMode_Append:
        flags |= O_WRONLY | O_APPEND;
        break;
    case FileMode_ReadWrite:
        flags |= O_RDWR | O_CREAT;
        break;
    };

    hFile = open64( filePath.c_str(), flags );
    if ( hFile == -1 ) {
        SIRENGINE_WARNING( "Error opening file \"%s\" in mode 0x%x", filePath.c_str(), (unsigned)flags );
    }

    return (void *)(uintptr_t)hFile;
}

void CPosixApplication::FileClose( void *hFile )
{
    assert( hFile );

    const int fd = (uintptr_t)hFile;

    if ( fd != -1 ) {
        if ( close( fd ) != 0 ) {
            Error( "Failure closing file %i", fd );
        }
    }
}

size_t CPosixApplication::FileWrite( const void *pBuffer, size_t nBytes, void *hFile )
{
    assert( hFile );

    const ssize_t nBytesWritten = write( (int)(uintptr_t)hFile, pBuffer, nBytes );
    if ( nBytesWritten == -1 ) {
        Error( "Error writing %lu bytes from source 0x%lx to file", nBytes, (uintptr_t)pBuffer );
    }
    return (size_t)nBytesWritten;
}

size_t CPosixApplication::FileRead( void *pBuffer, size_t nBytes, void *hFile )
{
    assert( hFile );

    const ssize_t nBytesRead = read( (int)(uintptr_t)hFile, pBuffer, nBytes );
    if ( nBytesRead == -1 ) {
        Error( "Error reading %lu bytes to source 0x%lx from file", nBytes, (uintptr_t)pBuffer );
    }
    return nBytesRead;
}

size_t CPosixApplication::FileTell( void *hFile )
{
    assert( hFile );
}

size_t CPosixApplication::FileLength( void *hFile )
{
}

/*
void CPosixApplication::ListFiles( CVector<FileSystem::CFilePath>& fileList, const char *pDirectory, const char *pSubDirs, const char *pExtension )
{
    DIR *d;
    struct dirent *dir;
    char szSubDirs[ SIRENGINE_MAX_OSPATH*2+1 ];
    char szNewSubDirs[ SIRENGINE_MAX_OSPATH*2+1 ];
    char szFileName[ SIRENGINE_MAX_OSPATH*2+1 ];
    char szSearchPath[ SIRENGINE_MAX_OSPATH*2+1 ];
    struct stat st;
    bool bHasPatterns;
    uint64_t nExtensionLength, nPathLength;
    const char *x;

    if ( *pSubDirs ) {
        SIREngine_snprintf( szSubDirs, sizeof( szSubDirs ) - 1, "%s/%s", pDirectory, pSubDirs );
    } else {
        SIREngine_snprintf( szSubDirs, sizeof( szSubDirs ) - 1, "%s", pDirectory );
    }

    d = opendir( szSubDirs );
    if ( !d ) {
        SIRENGINE_WARNING( "Error opening directory \"%s\": %s", szSubDirs, strerror( errno ) );
        return;
    }

    if ( !pExtension || !*pExtension ) {
        pExtension = "";
    }

    nExtensionLength = strlen( pExtension );
    bHasPatterns = String_HasPatterns( pExtension );

    fileList.reserve( 256 );
    while ( ( dir = readdir( d ) ) != NULL ) {
        SIREngine_snprintf( szSearchPath, sizeof( szSearchPath ) - 1, "%s/%s", pDirectory, dir->d_name );
        if ( stat( szSearchPath, &st ) == -1 ) {
            continue;
        }

        if ( st.st_mode & S_IFDIR ) {
            if ( dir->d_name[0] != '.' && ( dir->d_name[0] != '.' && dir->d_name[1] != '.' ) ) {
                if ( *pSubDirs ) {
                    SIREngine_snprintf( szNewSubDirs, sizeof( szNewSubDirs ) - 1, "%s/%s", pSubDirs, dir->d_name );
                } else {
                    SIREngine_snprintf( szNewSubDirs, sizeof( szNewSubDirs ) - 1, "%s", dir->d_name );
                }
                ListFiles( fileList, pDirectory, szNewSubDirs, pExtension );
            } else {
                continue; // "." and ".." are this directory
            }
        }

        if ( *pExtension ) {
            if ( bHasPatterns ) {
                x = strrchr( dir->d_name, '.' );
                if ( !x || !String_FilterExt( pExtension, x + 1 ) ) {
                    continue;
                }
            }
            else {
                nPathLength = strlen( dir->d_name );
                if ( nPathLength < nExtensionLength || SIREngine_stricmp( dir->d_name + nPathLength - nExtensionLength, pExtension ) ) {
                    continue;
                }
            }
        }
        fileList.emplace_back( dir->d_name );
    }
    closedir( d );
}
*/

const CVector<FileSystem::CFilePath>& CPosixApplication::ListFiles( const FileSystem::CFilePath& directory, bool bDirectoryOnly )
{
    static CVector<FileSystem::CFilePath> files;
    DIR *dir;
    char szSearchPath[ SIRENGINE_MAX_OSPATH*2+1 ];
    char szDirectory[ SIRENGINE_MAX_OSPATH*2+1 ];
    struct dirent *d;
    struct stat64 buf;

    if ( ( dir = opendir( directory.c_str() ) ) == NULL ) {
        SIRENGINE_WARNING( "Error opening directory \"%s\"", directory.c_str() );
        return files;
    }

    files.reserve( 256 );
    while ( ( d = readdir( dir ) ) != NULL ) {
        SIREngine_snprintf( szSearchPath, sizeof( szSearchPath ) - 1, "%s/%s", directory.c_str(), d->d_name );
        if ( stat64( szSearchPath, &buf ) == -1 ) {
            continue;
        }

        if ( ( bDirectoryOnly && !( buf.st_mode & S_IFDIR ) ) || ( !bDirectoryOnly && ( buf.st_mode & S_IFDIR ) ) ) {
            continue;
        }

        files.emplace_back( szSearchPath );
    }
    closedir( dir );

    // sort alphabetically
    eastl::sort( files.begin(), files.end(), [&]( const FileSystem::CFilePath& a, const FileSystem::CFilePath& b ) -> bool {
        return a > b;    
    } );

    return files;
}

void CPosixApplication::OnOutOfMemory( void )
{
    Error( "Allocation Failed!" );
}

void CPosixApplication::GetPwd( void )
{
    char pwd[ SIRENGINE_MAX_OSPATH ];

    // more reliable, linux-specific
    if ( readlink( "/proc/self/exe", pwd, sizeof( pwd ) - 1 ) != -1 ) {
        pwd[ sizeof( pwd ) - 1 ] = '\0';
        dirname( pwd );
        m_GamePath = pwd;
        return;
    }
    
    if ( !getcwd( pwd, sizeof( pwd ) ) ) {
        *pwd = '\0';
    }

    m_GamePath = pwd;
}

int main( int argc, char **argv )
{
    int i;
    CVector<CString> commandLine;
    ApplicationInfo_t appInfo{};

    g_pApplication = new ( malloc( sizeof( CPosixApplication ) ) ) CPosixApplication();
    g_pMemAlloc = new ( malloc( sizeof( CTagArenaAllocator ) ) ) CTagArenaAllocator( "MainArena", 256*1024*1024 );

    commandLine.reserve( argc );
    for ( i = 0; i < argc; i++ ) {
        commandLine.emplace_back( argv[i] );
    }

    const CVector<FileSystem::CFilePath>& files = g_pApplication->ListFiles( g_pApplication->GetGamePath() );
    for ( const auto& it : files ) {
        SIRENGINE_LOG( "Found file \"%s\"", it.c_str() );
    }

    g_pApplication->SetApplicationArgs( appInfo );
    g_pApplication->SetCommandLine( commandLine );

    CPosixApplication::nOOMBackupSize = OOM_MEMORY_BACKUP_POOL_SIZE;
    CPosixApplication::pOOMBackup = malloc( CPosixApplication::nOOMBackupSize );

    g_pApplication->Init();
    g_pApplication->Run();
    g_pApplication->Shutdown();

    free( CPosixApplication::pOOMBackup );

    return EXIT_SUCCESS;
}