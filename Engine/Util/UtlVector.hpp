#ifndef __UTL_VECTOR_HPP__
#define __UTL_VECTOR_HPP__

#pragma once

#include <Engine/Core/SIREngine.hpp>

template<typename T>
class UtlVector
{
public:
#if defined(SIRENGINE_DEFINE_TEMPLATE_STL_SUBTYPES)
public:
    typedef UtlVector<T> this_type;
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef pointer iterator;
    typedef const_pointer const_iterator;
    typedef uint64_t size_type;
    typedef ptrdiff_t difference_type;
#endif
private:
    T *m_pBuffer;
    uint64_t m_nAllocated;
    uint64_t m_nSize;
};

#endif