#include "JsonReader.h"

bool CJsonReader::Parse( const CMemoryFile& fileData )
{
    const char *str;

    str = (const char *)fileData.GetBuffer();

    while ( *str ) {
        switch ( *str ) {
        case '{': {

            break; }
        case '}': {
            
            break; }
        };

        str++;
    }
}