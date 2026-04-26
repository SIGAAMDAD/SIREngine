#ifndef __OPENGL_PCH_H__
#define __OPENGL_PCH_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>
#include <SDL3/SDL_opengl.h>

#if defined(SIRENGINE_PLATFORM_WINDOWS)
	#if defined(SIRENGINE_COMPILER_MSVC)
		#pragma warning (disable: 4201)
		#pragma warning (disable: 4214)
		#pragma warning (disable: 4514)
		#pragma warning (disable: 4032)
		#pragma warning (disable: 4201)
		#pragma warning (disable: 4214)
	#endif
	#include <windows.h>
	#include <GL/gl.h>
#elif defined(SIRENGINE_PLATFORM_POSIX)
	#include <GL/gl.h>
	#include <GL/glcorearb.h>
	#include <GL/glext.h>
	#include <KHR/khrplatform.h>
	#if defined(SIRENGINE_PLATFORM_APPLE)
		#define GL_NUM_EXTENSIONS                 0x821D
		#include <OpenGL/gl.h>
		#include <OpenGL/glext.h>
	#endif
#endif

#endif