#include "Win_Local.h"

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    HANDLE hProcess;
    DWORD dwPriority;

    // should never get a previous instance in Win32
	if ( hPrevInstance ) {
		return 0;
	}

	// slightly boost process priority if it set to default
	hProcess = GetCurrentProcess();
	dwPriority = GetPriorityClass( hProcess );
	if ( dwPriority == NORMAL_PRIORITY_CLASS || dwPriority == ABOVE_NORMAL_PRIORITY_CLASS ) {
		SetPriorityClass( hProcess, HIGH_PRIORITY_CLASS );
	}



    // never gets here
    return EXIT_SUCCESS;
}