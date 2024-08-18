#ifndef __CSHARED_PTR_H__
#define __CSHARED_PTR_H__

#define SIRENGINE_REPLACE_NEW_AND_DELETE
#include <Engine/Core/SIREngine.h>
#include "CRefCount.h"

#define EASTL_SHARED_PTR_DEFAULT_ALLOCATOR MemoryAllocator( EASTL_SHARED_PTR_DEFAULT_NAME )
#include <EASTL/shared_ptr.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

template<typename T>
class CSharedPtr : public eastl::shared_ptr<T>
{
public:
	typedef eastl::shared_ptr<T> base_type;

	using base_type::this_type;
	using base_type::element_type; 
	using base_type::reference_type;
	using base_type::default_allocator_type;
	using base_type::default_deleter_type;
	using base_type::weak_type;
public:
	CSharedPtr( T *pMemory )
		: base_type( pMemory )
	{ }
};

#endif