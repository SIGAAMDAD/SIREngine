#ifndef __SIRENGINE_MATHLIB_VEC4_H__
#define __SIRENGINE_MATHLIB_VEC4_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include "VecBase.h"

template<typename T>
class CVec4Base
{
public:
    CVec4Base( T _x, T _y, T _z, T _w )
        : x( _x ), y( _y ), z( _z ), w( _w )
    { }

    VECBASE( CVec4Base, T )

    SIRENGINE_FORCEINLINE const CVec4Base& operator+=( const CVec4Base& other )
    {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec4Base& operator-=( const CVec4Base& other )
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec4Base& operator/=( const CVec4Base& other )
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec4Base& operator*=( const CVec4Base& other )
    {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        w *= other.w;
        return *this;
    }

    SIRENGINE_FORCEINLINE CVec4Base operator+( const CVec4Base& other ) const
    { return CVec4Base<T>( x + other.x, y + other.y, z + other.z, w + other.w ); }
    SIRENGINE_FORCEINLINE CVec4Base operator-( const CVec4Base& other ) const
    { return CVec4Base<T>( x - other.x, y - other.y, z - other.z, w - other.w ); }
    SIRENGINE_FORCEINLINE CVec4Base operator*( const CVec4Base& other ) const
    { return CVec4Base<T>( x * other.x, y * other.y, z * other.z, w * other.w ); }
    SIRENGINE_FORCEINLINE CVec4Base operator/( const CVec4Base& other ) const
    { return CVec4Base<T>( x / other.x, y / other.y, z / other.z, w / other.w ); }
private:
    union {
        struct {
            T x;
            T y;
            T z;
            T w;
        };

        T xyzw[4];
    };
};

using CVec4 = CVec4Base<float>;
using CUVec4 = CVec4Base<uint32_t>;
using CIVec4 = CVec4Base<int32_t>;

#endif