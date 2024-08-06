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
#include <Engine/Core/ThreadSystem/Thread.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <SDL2/SDL_stdinc.h>

#define OOM_MEMORY_BACKUP_POOL_SIZE 4*1024

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
        SIRENGINE_ERROR( "Segmentation Violation Caught" );
        break;
    case SIGBUS:
        SIRENGINE_ERROR( "Bus Error Caught" );
        break;
    case SIGABRT:
        SIRENGINE_ERROR( "Caught assertion" );
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

void CPosixApplication::Error( const char *pError )
{
    char msg[1024];
    int length;

    length = SIREngine_snprintf( msg, sizeof( msg ) - 1, "ERROR: %s\n", pError );
    write( STDERR_FILENO, msg, length );

    DumpStacktrace();
    _Exit( EXIT_FAILURE );
}

void *CPosixApplication::OpenDLL( const char *pName )
{
    void *pDLLHandle;
    char szPath[ SIRENGINE_MAX_OSPATH ];

    if ( !pName || !*pName ) {
        SIRENGINE_ERROR( "OpenDLL: invalid name" );
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
        SIRENGINE_ERROR( "mmap failed: %s", strerror( errno ) );
    }
    *(size_t *)pMemory = *nSize;

    return (void *)( (size_t *)pMemory + 1 );
}

void CPosixApplication::VirtualFree( void *pBuffer )
{
    size_t *nSize = ( (size_t *)pBuffer - 1 );
    if ( munmap( nSize, *nSize ) == -1 ) {
        SIRENGINE_ERROR( "munmap failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::CommitMemory( void *pMemory, size_t nOffset, size_t nSize )
{
    if ( madvise( ( (uint8_t *)pMemory ) + nOffset, nSize, MADV_WILLNEED ) != 0 ) {
        SIRENGINE_ERROR( "madvise( 0x%lx, %lu, MADV_WILLNEED ) failed: %s", (uintptr_t)(( (uint8_t *)pMemory ) + nOffset ), nSize,
            strerror( errno ) );
    }
}

void CPosixApplication::DecommitMemory( void *pMemory, size_t nOffset, size_t nSize )
{
    if ( madvise( ( (uint8_t *)pMemory ) + nOffset, nSize, MADV_DONTNEED ) != 0 ) {
        SIRENGINE_ERROR( "madvise failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::SetMemoryReadOnly( void *pMemory, size_t nOffset, size_t nSize )
{
    if ( mprotect( ( (byte *)pMemory ) + nOffset, nSize, PROT_READ ) == -1 ) {
        SIRENGINE_ERROR( "mprotect failed: %s", strerror( errno ) );
    }
}

void *CPosixApplication::ThreadFunction( void *pArgument )
{
    CThread *pObject = (CThread *)pArgument;
    pObject->RunThread();
    return NULL;
}

void CPosixApplication::ThreadStart( void *pThread, CThread *pObject, void (CThread::*pFunction)( void ) )
{
    SIRENGINE_LOG( "Running thread '%s'", pObject->GetName().c_str() );
    if ( pthread_create( (pthread_t *)pThread, NULL, CPosixApplication::ThreadFunction, (void *)pObject ) == -1 ) {
        SIRENGINE_ERROR( "pthread_create failed: %s", strerror( errno ) );
    }
}

void CPosixApplication::ThreadJoin( void *pThread, CThread *pObject, uint64_t nTimeout )
{
    void *pReturn;
    struct timespec ts;

    ts.tv_nsec = nTimeout * 1000000;

    SIRENGINE_LOG( "Joining thread '%s'...", pObject->GetName().c_str() );
    pthread_timedjoin_np( *(pthread_t *)pThread, &pReturn, &ts );
    SIRENGINE_LOG( "Thread joined." );
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
            SIRENGINE_ERROR( "Failure closing file %i", fd );
        }
    }
}

size_t CPosixApplication::FileWrite( const void *pBuffer, size_t nBytes, void *hFile )
{
    assert( hFile );

    const ssize_t nBytesWritten = write( (int)(uintptr_t)hFile, pBuffer, nBytes );
    if ( nBytesWritten == -1 ) {
        SIRENGINE_ERROR( "Error writing %lu bytes from source 0x%lx to file", nBytes, (uintptr_t)pBuffer );
    }
    return (size_t)nBytesWritten;
}

size_t CPosixApplication::FileRead( void *pBuffer, size_t nBytes, void *hFile )
{
    assert( hFile );

    const ssize_t nBytesRead = read( (int)(uintptr_t)hFile, pBuffer, nBytes );
    if ( nBytesRead == -1 ) {
        SIRENGINE_ERROR( "Error reading %lu bytes to source 0x%lx from file", nBytes, (uintptr_t)pBuffer );
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

CVector<FileSystem::CFilePath> CPosixApplication::ListFiles( const FileSystem::CFilePath& directory, bool bDirectoryOnly )
{
    CVector<FileSystem::CFilePath> files;
    DIR *dir;
    char szSearchPath[ SIRENGINE_MAX_OSPATH*2+1 ];
    char szDirectory[ SIRENGINE_MAX_OSPATH*2+1 ];
    struct dirent *d;
    struct stat buf;

    if ( ( dir = opendir( directory.c_str() ) ) == NULL ) {
        SIRENGINE_ERROR( "Error opening directory \"%s\"", directory.c_str() );
        return files;
    }

    while ( ( d = readdir( dir ) ) != NULL ) {
        SIREngine_snprintf( szSearchPath, sizeof( szSearchPath ) - 1, "%s/%s", directory.c_str(), d->d_name );
        if ( stat( szSearchPath, &buf ) == -1 ) {
            continue;
        }

        if ( ( bDirectoryOnly && !( buf.st_mode & S_IFDIR ) ) || ( !bDirectoryOnly && ( buf.st_mode & S_IFDIR ) )
            || ( bDirectoryOnly && ( buf.st_mode & S_IFREG ) ) )
        {
            continue;
        }
        else if ( d->d_name[0] == '.' || ( d->d_name[0] == '.' && d->d_name[1] == '.' ) ) {
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
    SIRENGINE_ERROR( "Allocation Failed!" );
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

static void *Mem_ClearedAlloc( size_t nMembers, size_t nCount )
{
    return memset( Mem_Alloc( nMembers * nCount, 16 ), 0, nMembers * nCount );
}

static void *Mem_Realloc( void *pOriginal, size_t newSize )
{
    return g_pMemAlloc->Realloc( pOriginal, newSize );
}

int main( int argc, char **argv )
{
    int i;
    CVector<CString> commandLine;
    ApplicationInfo_t appInfo{};

    g_pApplication = new ( malloc( sizeof( CPosixApplication ) ) ) CPosixApplication();
    Mem_Init();

    commandLine.reserve( argc );
    for ( i = 0; i < argc; i++ ) {
        commandLine.emplace_back( argv[i] );
    }

    g_pApplication->SetApplicationArgs( appInfo );
    g_pApplication->SetCommandLine( commandLine );

    CPosixApplication::nOOMBackupSize = OOM_MEMORY_BACKUP_POOL_SIZE;
    CPosixApplication::pOOMBackup = malloc( CPosixApplication::nOOMBackupSize );

    g_pApplication->Init();
    g_pApplication->Run();
    g_pApplication->Shutdown();

    free( CPosixApplication::pOOMBackup );

    _Exit( EXIT_SUCCESS );

    // never reached
    return EXIT_SUCCESS;
}