#include "JsonReader.h"

typedef enum {
    JsonTok_ObjectBegin = '{',
    JsonTok_ObjectEnd = '}',
    JsonTok_ArrayBegin = '[',
    JsonTok_ArrayEnd = ']',
    JsonTok_StringBegin = '\"',
    JsonTok_StringEnd = '\"',
    JsonTok_ValueAssign = ':',
} JsonToken_t;

bool CJsonReader::Parse( const CMemoryFile& fileData )
{
    const char *str;
    uint32_t nObjectDepth;
    uint32_t nArrayDepth;
    bool bLastVar;

    str = (const char *)fileData.GetBuffer();

    nObjectDepth = 0;
    nArrayDepth = 0;
    while ( *str ) {
        switch ( *str ) {
        case JsonTok_ObjectBegin: {
            nObjectDepth++;
            break; }
        case JsonTok_ObjectEnd: {
            if ( nObjectDepth == 0 ) {
                //err
            }
            nObjectDepth--;
            break; }
        case JsonTok_ArrayBegin:
            nArrayDepth++;
            break;
        case JsonTok_ArrayEnd:
            if ( nArrayDepth == 0 ) {
                //err
            }
            nArrayDepth--;
            break;
        case JsonTok_ValueAssign:
        case ',':
            if ( !bLastVar ) {
                //err
            }
            break;
        };

        str++;
    }

    if ( nObjectDepth > 0 ) {
        //err
    }
}