#ifndef __GUILIB_H__
#define __GUILIB_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <EASTL/unique_ptr.h>
#include <Engine/Memory/Allocators/HunkAllocator.h>

namespace SIREngine::GUILib {
	class CInstance
	{
	public:
		CInstance( void )
		{ }
		~CInstance()
		{ }

		SIRENGINE_FORCEINLINE static eastl::unique_ptr<CInstance>& Get( void )
		{ return g_pInstance; }
	private:
		static eastl::unique_ptr<CInstance> g_pInstance;
	};
};

#endif