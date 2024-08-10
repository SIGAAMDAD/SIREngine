#ifndef __SIRENGINE_CONTROLLERSTATUS_EVENT_H__
#define __SIRENGINE_CONTROLLERSTATUS_EVENT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "EventBase.h"
#include <Engine/Core/Types.h>
#include <Engine/Core/Compiler.h>

class CControllerStatusEvent : public IEventBase
{
public:
	CControllerStatusEvent( bool bStatus )
		: m_bStatus( bStatus )
	{ }
	virtual ~CControllerStatusEvent() override
	{ }

	SIRENGINE_FORCEINLINE bool IsDeviceAdded( void ) const
	{ return m_bStatus; }
private:
	bool32 m_bStatus;
};

#endif