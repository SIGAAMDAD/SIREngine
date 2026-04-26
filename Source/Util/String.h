#ifndef __SIRENGINE_STRING_H__
#define __SIRENGINE_STRING_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Util
{
	class String
	{
	public:
		String( void );
		String( const String& other );
		String( String&& other );
		~String();
	private:
		size_t m_nCapacity;
		size_t m_nSize;
		char *m_pData;
	};
};

#endif