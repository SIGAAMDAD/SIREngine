#ifndef __SIR_RENDER_RESOURCE_H__
#define __SIR_RENDER_RESOURCE_H__

#include "Core/Memory/Pointers/SIRSharedPtr.h"
#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/SIRGraphicsTypes.h>

class SIRRenderResource
{
public:
	SIRWeakPtr<SIRRenderingDevice> getDevice( void );
};

#endif // __SIR_RENDER_RESOURCE_H__
