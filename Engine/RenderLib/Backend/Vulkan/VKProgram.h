#ifndef __VK_PROGRAM_H__
#define __VK_PROGRAM_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderProgram.h"

class VKProgram : public IRenderProgram
{
public:
    VKProgram( const RenderProgramInit_t& programInfo );
    virtual ~VKProgram() override;

    virtual bool Load( void ) override { return false; }
};

#endif