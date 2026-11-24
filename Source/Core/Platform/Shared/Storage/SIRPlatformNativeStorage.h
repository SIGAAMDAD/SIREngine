#ifndef __SIR_PLATFORMNATIVESTORAGE_H__
#define __SIR_PLATFORMNATIVESTORAGE_H__
#include <Core/SIRPCH.h>
typedef struct alignas( 16 ) sirPlatformNativeStorage_s
{
    uint8_t bytes[64];
} sirPlatformNativeStorage_t;
#endif // __SIR_PLATFORMNATIVESTORAGE_H__
