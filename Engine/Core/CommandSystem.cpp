#include "CommandSystem.h"

bool CCommandSystem::ExecuteCommand( const char *pText )
{
    const char *pString;
    bool bInStarCommment, bInSlashComment;

    pString = pText;
    bInStarCommment = false;
    bInSlashComment = false;
    
    while ( *pString ) {
        if ( pString[0] == '/' && pString[1] == '*' ) {
            bInStarCommment = true;
        } else if ( pString[0] == '*' && pString[1] == '\\' ) {
            
        }
    }
}