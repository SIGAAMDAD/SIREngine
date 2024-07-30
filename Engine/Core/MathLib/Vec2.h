#ifndef __SIRENGINE_MATHLIB_VEC2_H__
#define __SIRENGINE_MATHLIB_VEC2_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include "VecBase.h"

template<typename T>
class CVec2Base
{
public:
    CVec2Base( T _x, T _y )
        : x( _x ), y( _y )
    { }

    VECBASE( CVec2Base, T )

    SIRENGINE_FORCEINLINE const CVec2Base& operator+=( const CVec2Base& other )
    {
        x += other.x;
        y += other.y;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec2Base& operator-=( const CVec2Base& other )
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec2Base& operator/=( const CVec2Base& other )
    {
        x /= other.x;
        y /= other.y;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec2Base& operator*=( const CVec2Base& other )
    {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    SIRENGINE_FORCEINLINE CVec2Base operator+( const CVec2Base& other ) const
    { return CVec2Base<T>( x + other.x, y + other.y ); }
    SIRENGINE_FORCEINLINE CVec2Base operator-( const CVec2Base& other ) const
    { return CVec2Base<T>( x - other.x, y - other.y ); }
    SIRENGINE_FORCEINLINE CVec2Base operator*( const CVec2Base& other ) const
    { return CVec2Base<T>( x * other.x, y * other.y ); }
    SIRENGINE_FORCEINLINE CVec2Base operator/( const CVec2Base& other ) const
    { return CVec2Base<T>( x / other.x, y / other.y ); }
private:
    union {
        struct {
            T x;
            T y;
        };

        T xy[2];
    };
};

using CVec2 = CVec2Base<float>;
using CUVec2 = CVec2Base<uint32_t>;
using CIVec2 = CVec2Base<int32_t>;

#endif