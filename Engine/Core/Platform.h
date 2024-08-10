#ifndef __PLATFORM_CONFIG_H__
#define __PLATFORM_CONFIG_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#if defined(_WIN32)
	#if defined(__CYGWIN__)
		#define SIRENGINE_PLATFORM_NAME "Cygwin"
		#define SIRENGINE_PLATFORM_CYGWIN 1
	#else
		#define SIRENGINE_PLATFORM_NAME "Windows"
	#endif

	#if defined(_WIN64)
		#define SIRENGINE_PLATFORM_64BIT 1
	#elif defined(_WIN32)
		#define SIRENGINE_PLATFORM_32BIT 1
	#elif defined(_WIN16)
		#error "16-Bit Architectures Aren't Supported, And Buy A New PC Buddy, 32-Bit At Least"s
	#endif

	#if defined(_M_X64) || defined(_M_AMD64)
		#define SIRENGINE_PLATFORM_X64 1
		#if !defined(__WORDSIZE)
			#define __WORDSIZE 64
		#endif
	#elif defined(_M_IX86)
		#define SIRENGINE_PLATFORM_X32 1
		#if !defined(__WORDSIZE)
			#define __WORDSIZE 32
		#endif
	#elif defined(_M_ARM64)
		#define SIRENGINE_PLATFORM_ARM64 1
		#if !defined(__WORDSIZE)
			#define __WORDSIZE 64
		#endif
	#endif

	#define IsPlatformPosix() 0
	#define IsPlatformWindows() 1

	#define SIRENGINE_PLATFORM_WINDOWS 1
	#define SIRENGINE_PLATFORM_PC 1
	#define SIRENGINE_LITTLE_ENDIAN 1

	#define SIRENGINE_STDOUT_HANDLE (void *)STDOUT_HANDLE
	#define SIRENGINE_STDERR_HANDLE (void *)STDERR_HANDLE
	#define SIRENGINE_INVALID_HANDLE INVALID_HANDLE

	#define SIRENGINE_PATH_SEPERATOR '\\'
#elif defined(__unix__) || defined(__unix)
	#if defined(__ANDROID__)
		#error "Android Isn't Supported Yet!"

		#define SIRENGINE_PLATFORM_NAME "Android"
		#define SIRENGINE_PLATFORM_ANDROID 1
		#define SIRENGINE_PLATFORM_MOBILE 1

		#include <android/api-level.h>
	#elif defined(__APPLE__) || defined(__OSX__)
		#error "Apple Isn't Supported Yet!"

		#if defined(__MACH__)
			#define SIRENGINE_PLATFORM_NAME "MacOS"
			#define SIRENGINE_PLATFORM_MACOS 1
			#define SIRENGINE_PLATFORM_PC 1
		#else
			#define SIRENGINE_PLATFORM_NAME "iOS"
			#define SIRENGINE_PLATFORM_IOS 1
			#define SIRENGINE_PLATFORM_MOBILE 1
		#endif

		#define SIRENGINE_PLATFORM_APPLE 1
	#elif defined(__linux__)
		#define SIRENGINE_PLATFORM_NAME "Linux"
		#define SIRENGINE_PLATFORM_LINUX 1
		#define SIRENGINE_PLATFORM_PC 1
	#endif

	#define SIRENGINE_PLATFORM_POSIX 1
	#define SIRENGINE_LITTLE_ENDIAN 1

	#if !defined(O_BINARY)
		#define O_BINARY 0
	#endif
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
	#if defined(__FreeBSD__)
		#define SIRENGINE_PLATFORM_NAME "FreeBSD"
	#elif defined(__NetBSD__)
		#define SIRENGINE_PLATFORM_NAME "NetBSD"
	#elif defined(__OpenBSD__)
		#define SIRENGINE_PLATFORM_NAME "OpenBSD"
	#elif defined(__bsdi__)
		#define SIRENGINE_PLATFORM_NAME "BSD OS"
	#elif defined(__DragonFly__)
		#define SIRENGINE_PLATFORM_NAME "DragonFly"
	#else
		#error "Unrecognized BSD Version"
	#endif

	#define SIRENGINE_PLATFORM_BSD 1
	#define SIRENGINE_PLATFORM_PC 1

	#if !defined(SIRENGINE_PLATFORM_POSIX)
		#define SIRENGINE_PLATFORM_POSIX 1
	#endif

	#include <sys/types.h>
	#include <machine/endian.h>

	#if BYTE_ORDER == BIG_ENDIAN
		#define SIRENGINE_BIG_ENDIAN 1
	#else
		#define SIRENGINE_LITTLE_ENDIAN 1
	#endif
#else
	#error "Unsupported Operating System! WTF R U COMPILING ON?"
#endif

// general posix definitions
#if defined(SIRENGINE_PLATFORM_POSIX)
	#include <unistd.h>
	#define SIRENGINE_STDOUT_HANDLE (void *)STDOUT_FILENO
	#define SIRENGINE_STDERR_HANDLE (void *)STDERR_FILENO
	#define SIRENGINE_INVALID_HANDLE (int)(-1)
	#define SIRENGINE_PATH_SEPERATOR '/'

	#define IsPlatformPosix() 1
	#define IsPlatformWindows() 0

	#if defined(__x86_64__)
		#define SIRENGINE_PLATFORM_X64 1
		#define SIRENGINE_PLATFORM_64BIT 1
	#elif defined(i386) || defined(__i386__) || defined(__i386)
		#define SIRENGINE_PLATFORM_I386 1
		#define SIRENGINE_PLATFORM_32BIT 1
	#elif defined(__aarch64__)
		#define SIRENGINE_PLATFORM_ARM64 1
		#define SIRENGINE_PLATFORM_64BIT 1
	#elif defined(__arm__)
		#define SIRENGINE_PLATFORM_ARM32 1
		#define SIRENGINE_PLATFORM_32BIT 1
	#endif
#endif

#if defined(SIRENGINE_PLATFORM_PC)
	#define IsPlatformPC() 1
	#define IsPlatformConsole() 0
	#define IsPlatformMobile() 0
#elif defined(SIRENGINE_PLATFORM_CONSOLE)
	#define IsPlatformPC() 0
	#define IsPlatformConsole() 1
	#define IsPlatformMobile() 0
#elif defined(SIRENGINE_PLATFORM_MOBILE)
	#define IsPlatformPC() 0
	#define IsPlatformConsole() 0
	#define IsPlatformMobile() 1
#endif

#endif