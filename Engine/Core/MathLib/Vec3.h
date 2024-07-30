#ifndef __SIRENGINE_MATHLIB_VEC3_H__
#define __SIRENGINE_MATHLIB_VEC3_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include "VecBase.h"

template<typename T>
class CVec3Base
{
public:
    CVec3Base( T _x, T _y, T _z )
        : x( _x ), y( _y ), z( _z )
    { }

    VECBASE( CVec3Base, T )

    SIRENGINE_FORCEINLINE const CVec3Base& operator+=( const CVec3Base& other )
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec3Base& operator-=( const CVec3Base& other )
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec3Base& operator/=( const CVec3Base& other )
    {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        return *this;
    }
    SIRENGINE_FORCEINLINE const CVec3Base& operator*=( const CVec3Base& other )
    {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        return *this;
    }

    SIRENGINE_FORCEINLINE CVec3Base operator+( const CVec3Base& other ) const
    { return CVec3Base<T>( x + other.x, y + other.y, z + other.z ); }
    SIRENGINE_FORCEINLINE CVec3Base operator-( const CVec3Base& other ) const
    { return CVec3Base<T>( x - other.x, y - other.y, z - other.z ); }
    SIRENGINE_FORCEINLINE CVec3Base operator*( const CVec3Base& other ) const
    { return CVec3Base<T>( x * other.x, y * other.y, z * other.z ); }
    SIRENGINE_FORCEINLINE CVec3Base operator/( const CVec3Base& other ) const
    { return CVec3Base<T>( x / other.x, y / other.y, z / other.z ); }
private:
    union {
        struct {
            T x;
            T y;
            T z;
        };

        T xyz[3];
    };
};

using CVec3 = CVec3Base<float>;
using CUVec3 = CVec3Base<uint32_t>;
using CIVec3 = CVec3Base<int32_t>;

#endif