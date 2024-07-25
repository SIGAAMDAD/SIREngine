#ifndef __SIRENGINE_CONFIG_HPP__
#define __SIRENGINE_CONFIG_HPP__

#pragma once

#if !defined(SIRENGINE_MALLOC_OVERRIDE)
    #define SIRENGINE_MALLOC( size ) Mem_Alloc( size )
#endif

#if !defined(SIRENGINE_FREE_OVERRIDE)
    #define SIRENGINE_FREE( ptr ) Mem_Free( ptr )
#endif

#if !defined(SIRENGINE_USE_GLM_MATH)
    #define SIRENGINE_USE_MATHLIB 1
#endif

#if !defined(SIRENGINE_NO_DEFINE_TEMPLATE_STL_SUBTYPES)
    #define SIRENGINE_DEFINE_TEMPLATE_STL_SUBTYPES
#endif

#endif