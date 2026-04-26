#ifndef __SIRENGINE_LINEAR_ALLOCATOR_H__
#define __SIRENGINE_LINEAR_ALLOCATOR_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Core::Memory::Allocators
{
	class LinearAllocator
	{
	public:
		LinearAllocator( size_t nSize, size_t nAlignment = 16 );
		~LinearAllocator();

		void *Alloc( size_t nSize, size_t nAlignment = 16 );
		void Clear( void );
	private:
		size_t m_nSize;
		size_t m_nUsed;
		void *m_pData;
	};
};

#endif