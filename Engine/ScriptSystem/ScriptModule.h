#ifndef __SIRENGINE_SCRIPT_MODULE_HPP__
#define __SIRENGINE_SCRIPT_MODULE_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/ResourceDef.h>
#include "ScriptLib.h"

namespace SIREngine::ScriptLib {	
	class CScriptModule : public CResourceDef
	{
	public:
	private:
		CUniquePtr<asIScriptModule> m_pScriptModule;
		asIScriptContext *m_pScriptContext;
	};
};

#endif