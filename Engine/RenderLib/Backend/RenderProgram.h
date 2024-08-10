#ifndef __RENDER_PROGRAM_H__
#define __RENDER_PROGRAM_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "RenderShader.h"

class IRenderProgram
{
public:
    IRenderProgram( void )
        : m_pVertexShader( NULL ), m_pFragmentShader( NULL )
    { }
    virtual ~IRenderProgram()
    { }

    SIRENGINE_FORCEINLINE const CString& GetName( void ) const
    { return m_Name; }

    static IRenderProgram *Create( const RenderProgramInit_t& programInfo );

    SIRENGINE_FORCEINLINE virtual IRenderShader *GetVertexShader( void )
    { return m_pVertexShader; }
    SIRENGINE_FORCEINLINE virtual IRenderShader *GetPixelShader( void )
    { return m_pFragmentShader; }

    SIRENGINE_FORCEINLINE virtual const IRenderShader *GetVertexShader( void ) const
    { return m_pVertexShader; }
    SIRENGINE_FORCEINLINE virtual const IRenderShader *GetPixelShader( void ) const
    { return m_pFragmentShader; }

    virtual bool Load( void ) = 0;
protected:
    CString m_Name;

    IRenderShader *m_pVertexShader;
    IRenderShader *m_pFragmentShader;
};

#endif