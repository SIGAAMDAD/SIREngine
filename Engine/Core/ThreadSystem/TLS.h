#ifndef __SIRENGINE_TLS_H__
#define __SIRENGINE_TLS_H__

#pragma once

#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
	#include <Engine/Core/Application/Posix/PosixTypes.h>
#endif

class CThreadLocalData
{
public:
private:
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
	pthread_key_t m_nTLSIdKey;
#endif
};

#endif