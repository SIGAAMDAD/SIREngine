#ifndef __SIRENGINE_MATHLIB_VECBASE_H__
#define __SIRENGINE_MATHLIB_VECBASE_H__

#pragma once

#define VECBASE( className, primitiveType ) \
public: \
    SIRENGINE_FORCEINLINE className ( void ) = default; \
    SIRENGINE_FORCEINLINE className ( const className& ) = default; \
    SIRENGINE_FORCEINLINE className ( className&& ) = default; \
    SIRENGINE_FORCEINLINE ~className () = default; \
    \
    SIRENGINE_FORCEINLINE const className& operator=( const className& ) = default; \
    SIRENGINE_FORCEINLINE const className& operator=( className&& ) = default; \
    \
    SIRENGINE_FORCEINLINE bool operator==( const className& other ) const \
    { return ( memcmp( this, eastl::addressof( other ), sizeof( *this ) ) == 0 ); } \
    SIRENGINE_FORCEINLINE bool operator!=( const className& other ) const \
    { return !operator==( other ); } \
    \
    SIRENGINE_FORCEINLINE primitiveType& operator[]( int nIndex ) \
    { return &( this )[ nIndex ]; } \
    SIRENGINE_FORCEINLINE const primitiveType& operator[]( int nIndex ) const \
    { return &( this )[ nIndex ]; } \

#endif