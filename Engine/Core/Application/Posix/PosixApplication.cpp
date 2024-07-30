#include "PosixApplication.h"
#include <sys/mman.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>

CPosixApplication *g_pApplication;

CPosixApplication::CPosixApplication( const ApplicationInfo_t& appInfo, const CVector<CString>& cmdLine )
    : IGenericApplication( appInfo, cmdLine )
{
}

CPosixApplication::~CPosixApplication()
{
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

void *CPosixApplication::VirtualAlloc( size_t nSize, size_t nAlignment )
{
    void *pMemory;

    pMemory = mmap( NULL, nSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0 );
}

void CPosixApplication::VirtualFree( void *pBuffer )
{
    if ( munmap( pBuffer, GetAllocSize( pBuffer ) ) == -1 ) {

    }
}

void CPosixApplication::MutexLock( void *pMutex )
{
    if ( pthread_mutex_lock( (pthread_mutex_t *)pMutex ) == -1 ) {
        Error( "" );
    }
}

void CPosixApplication::MutexUnlock( void *pMutex )
{
    if ( pthread_mutex_unlock( (pthread_mutex_t *)pMutex ) == -1 ) {
        Error( "" );
    }
}

FILE *CPosixApplication::OpenFile( const CString& filePath, const char *mode )
{
    return fopen( filePath.c_str(), mode );
}

void *CPosixApplication::FileOpen( const CString& filePath )
{
}

void CPosixApplication::FileClose( void *hFile )
{
}

size_t CPosixApplication::FileWrite( const void *pBuffer, size_t nBytes, void *hFile )
{
}

size_t CPosixApplication::FileRead( void *pBuffer, size_t nBytes, void *hFile )
{
}

size_t CPosixApplication::FileTell( void *hFile )
{
}

size_t CPosixApplication::FileLength( void *hFile )
{
}

int main( int argc, char **argv )
{
    int i;
    CVector<CString> commandLine;
    ApplicationInfo_t appInfo{};

    commandLine.Reserve( argc );
    for ( i = 0; i < argc; i++ ) {
        commandLine.Push( argv[i] );
    }

    g_pApplication = new CPosixApplication( appInfo, commandLine );

    g_pApplication->Init();
    g_pApplication->Run();
    
    delete g_pApplication;

    return EXIT_SUCCESS;
}