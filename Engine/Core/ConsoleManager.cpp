#include "ConsoleManager.h"
#include <locale.h>

CConsoleManager::CConsoleManager( void )
{
    m_SavedLocale = setlocale( LC_NUMERIC, NULL );
    setlocale( LC_NUMERIC, "C" );
}

CConsoleManager::~CConsoleManager()
{
    setlocale( LC_NUMERIC, m_SavedLocale.c_str() );
}