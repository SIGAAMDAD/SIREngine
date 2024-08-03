#ifndef __SCRIPTLIB_JIT_H__
#define __SCRIPTLIB_JIT_H___

#include <Engine/Core/SIREngine.h>
#include <angelscript/angelscript.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

class CScriptJIT : public asIJITCompiler
{
public:
    CScriptJIT( void );
    virtual ~CScriptJIT();

    virtual int CompileFunction( asIScriptFunction *pFunction, asJITFunction *pOutput ) override;
	virtual void ReleaseJITFunction( asJITFunction func ) override;
};

#endif