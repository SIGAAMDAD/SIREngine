#include "PosixApplication.h"
#include <sys/mman.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <backtrace.h>
#include <malloc.h>
#include <errno.h>
#include <Engine/Memory/Backend/TagArenaAllocator.h>
#include <Engine/Core/ThreadSystem/Thread.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_events.h>
#include <sys/wait.h>
#include <sys/time.h>

char **myargv;
int myargc;

void *operator new[]( size_t nBytes, char const *, int, unsigned int, char const *, int )
{
    return ::operator new[]( nBytes );
}

void *operator new[]( size_t nBytes, unsigned long, unsigned long, char const*, int, unsigned int, char const*, int )
{
    return ::operator new[]( nBytes );
}

extern "C" void InitCrashHandler( void );
extern "C" void DumpStacktrace( void );

CPosixApplication::CPosixApplication()
{
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
    ShowErrorWindow( msg );
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

void *CPosixApplication::MapFile( void *hFile, size_t *pSize )
{
    void *pMemory;

    const size_t pos = FileTell( (void *)hFile );
    FileSeek( (void *)hFile, 0, SEEK_END );
    *pSize = FileTell( (void *)hFile );
    FileSeek( (void *)hFile, pos, SEEK_SET );

    pMemory = mmap( NULL, *pSize, PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANON, (PlatformTypes::file_t)(uintptr_t)hFile, 0 );
    if ( pMemory == MAP_FAILED || pMemory == NULL ) {
        SIRENGINE_WARNING( "Error mapping file into memory: %s", strerror( errno ) );
        return NULL;
    }

    return pMemory;
}

void CPosixApplication::UnmapFile( void *pMemory, size_t nSize )
{
    if ( munmap( pMemory, nSize ) == -1 ) {

    }
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

size_t CPosixApplication::FileSeek( void *hFile, size_t nOffset, int whence )
{
    return lseek( (PlatformTypes::file_t)(uintptr_t)hFile, nOffset, whence );
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

    hFile = open( filePath.c_str(), flags );
    if ( hFile == -1 ) {
        SIRENGINE_WARNING( "Error opening file \"%s\" in mode 0x%x", filePath.c_str(), (unsigned)flags );
    }
    if ( chmod( filePath.c_str(), 0777 ) == -1 ) {
        SIRENGINE_WARNING( "Error changing permissions for file \"%s\": %s", filePath.c_str(), strerror( errno ) );
    }

    return (void *)(uintptr_t)hFile;
}

void CPosixApplication::FileClose( void *hFile )
{
    assert( hFile != (void *)-1 );

    const int fd = (uintptr_t)hFile;

    if ( fd != -1 ) {
        if ( close( fd ) != 0 ) {
            SIRENGINE_ERROR( "Failure closing file %i", fd );
        }
    }
}

size_t CPosixApplication::FileWrite( const void *pBuffer, size_t nBytes, void *hFile )
{
    assert( hFile != (void *)-1 );

    const ssize_t nBytesWritten = write( (int)(uintptr_t)hFile, pBuffer, nBytes );
    if ( nBytesWritten == -1 ) {
        SIRENGINE_ERROR( "Error writing %lu bytes from source 0x%lx to file", nBytes, (uintptr_t)pBuffer );
    }
    return (size_t)nBytesWritten;
}

size_t CPosixApplication::FileRead( void *pBuffer, size_t nBytes, void *hFile )
{
    assert( hFile != (void *)-1 );

    const ssize_t nBytesRead = read( (int)(uintptr_t)hFile, pBuffer, nBytes );
    if ( nBytesRead == -1 ) {
        SIRENGINE_ERROR( "Error reading %lu bytes to source 0x%lx from file", nBytes, (uintptr_t)pBuffer );
    }
    return nBytesRead;
}

size_t CPosixApplication::FileTell( void *hFile )
{
    assert( hFile != (void *)-1 );
    return lseek( (PlatformTypes::file_t)(uintptr_t)hFile, 0, SEEK_CUR );
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

#define rdtsc( x ) \
    __asm__ __volatile__ ( "rdtsc" : "=A" ( x ) )

class CTimeVal
{
public:
    CTimeVal( void ) = default;
    CTimeVal& operator=( const CTimeVal& val ) { m_TimeVal = val.m_TimeVal; return *this; }
    inline double operator-( const CTimeVal& left ) {
        uint64_t left_us = (uint64_t)left.m_TimeVal.tv_sec * 1000000 + left.m_TimeVal.tv_usec;
	    uint64_t right_us = (uint64_t)m_TimeVal.tv_sec * 1000000 + m_TimeVal.tv_usec;
	    uint64_t diff_us = left_us - right_us;
	    return diff_us / 1000000.0f;
    }

    timeval m_TimeVal;
};

// Compute the positive difference between two 64 bit numbers.
static inline double diff( double v1, double v2 ) {
    double d = v1 - v2;
    return d >= 0 ? d : -d;
}


#if defined(SIRENGINE_PLATFORM_APPLE)
double GetCPUFreqFromPROC( void )
{
    int mib[2] = { CTL_HW, HW_CPU_FREQ };
    uint64_t frequency = 0;
    size_t len = sizeof( frequency );

    if ( sysctl( mib, 2, &frequency, &len, NULL, 0 ) == -1 ) {
        return 0;
    }
    return (double)frequency;
}
#else
double GetCPUFreqFromPROC( void )
{
    double mhz = 0;
    char line[1024], *s, search_str[] = "cpu MHz";
    FILE *fp; 
    
    // open proc/cpuinfo
    if ( ( fp = fopen( "/proc/cpuinfo", "r" ) ) == NULL ) {
	    return 0;
    }

    // ignore all lines until we reach MHz information
    while ( fgets( line, 1024, fp ) != NULL ) { 
    	if ( strstr( line, search_str ) != NULL ) {
	        // ignore all characters in line up to :
	        for ( s = line; *s && ( *s != ':' ); ++s )
                ;
	        // get MHz number
	        if ( *s && ( sscanf( s+1, "%lf", &mhz ) == 1 ) ) {
		        break;
            }
	    }
    }

    if ( fp ) {
        fclose( fp );
    }

    return mhz * 1000000.0f;
}
#endif


double CPosixApplication::GetCPUFrequency( void )
{
#if defined(SIRENGINE_PLATFORM_LINUX)
	const char *pFreq = getenv( "CPU_MHZ" );
	if ( pFreq ) {
		double retVal = 1000000.0f;
		return retVal * (double)atoll( pFreq );
	}
#endif

    // Compute the period. Loop until we get 3 consecutive periods that
    // are the same to within a small error. The error is chosen
    // to be +/- 0.02% on a P-200.
    const double error = 40000.0f;
    const int max_iterations = 600;
    int count;
    double period, period1 = error * 2, period2 = 0,  period3 = 0;

    for ( count = 0; count < max_iterations; count++ ) {
        CTimeVal start_time, end_time;
        uint64_t start_tsc, end_tsc;

        gettimeofday( &start_time.m_TimeVal, 0 );
        rdtsc( start_tsc );
        usleep( 5000 ); // sleep for 5 msec
        gettimeofday( &end_time.m_TimeVal, 0 );
        rdtsc( end_tsc );

        period3 = ( end_tsc - start_tsc) / (end_time - start_time );
        if ( diff( period1, period2 ) <= error && diff( period2, period3 ) <= error && diff( period1, period3 ) <= error ) {
 	        break;
        }
        period1 = period2;
        period2 = period3;
    }

    if ( count == max_iterations ) {
	    return GetCPUFreqFromPROC(); // fall back to /proc
    }

    // Set the period to the average period measured.
    period = ( period1 + period2 + period3 ) / 3;

    // Some Pentiums have broken TSCs that increment very
    // slowly or unevenly. 
    if ( period < 10000000.0f ) {
	    return GetCPUFreqFromPROC(); // fall back to /proc
    }

    return period;
}

bool CPosixApplication::IsRunningOnChromiumOS( void ) const
{
    bool bRetValue = false;
    int nSystemVendorFile = open( "/sys/class/dmi/id/sys_vendor", O_RDONLY );

    if ( nSystemVendorFile >= 0 ) {
        char szLineBuffer[128];
        ssize_t nLength = read( nSystemVendorFile, szLineBuffer, sizeof( szLineBuffer ) );

        if ( nLength > 0 ) {
            bRetValue = !SIREngine_stricmp( szLineBuffer, "ChromiumOS" );
        }

        close( nSystemVendorFile );
    }
    return bRetValue;
}

uint32_t CPosixApplication::GetNumberOfCoresIncludingHyperThreading( void )
{
    static uint32_t nCoreIds = 0;
    if ( nCoreIds == 0 ) {
        cpu_set_t availableCPUsMask;
        CPU_ZERO( &availableCPUsMask );

        if ( sched_getaffinity( 0, sizeof( availableCPUsMask ), &availableCPUsMask ) != 0 ) {
            nCoreIds = 0; // we are running on something, right?
        } else {
            nCoreIds = CPU_COUNT( &availableCPUsMask );
        }

        uint32_t nLimitCount = 32768;
        if ( eastl::find( m_CommandLineArgs.cbegin(), m_CommandLineArgs.cend(), "-corelimit=" ) ) {

        }
    }
}

uint32_t CPosixApplication::GetNumberOfCores( void )
{
    static uint32_t nCoreCount = 0;

    if ( nCoreCount == 0 ) {
        if ( eastl::find( m_CommandLineArgs.cbegin(), m_CommandLineArgs.cend(), "usehyperthreading" ) != m_CommandLineArgs.cend() ) {
            nCoreCount = GetNumberOfCoresIncludingHyperThreading();
        }
    }
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

    myargc = argc;
    myargv = argv;

    seteuid( getuid() );
    setenv( "LC_NUMERIC", "C", 1 );

    g_pApplication = new ( malloc( sizeof( CPosixApplication ) ) ) CPosixApplication();
    Mem_Init();

    commandLine.reserve( argc );
    for ( i = 0; i < argc; i++ ) {
        commandLine.emplace_back( argv[i] );
    }

    InitCrashHandler();

    g_pApplication->SetApplicationArgs( appInfo );
    g_pApplication->SetCommandLine( commandLine );

    g_pApplication->Init();
    g_pApplication->Run();
    g_pApplication->Shutdown();

    free( CPosixApplication::pOOMBackup );

    _Exit( EXIT_SUCCESS );

    // never reached
    return EXIT_SUCCESS;
}