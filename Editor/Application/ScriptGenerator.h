#ifndef __VALDEN_SCRIPT_GENERATOR_H__
#define __VALDEN_SCRIPT_GENERATOR_H__

#pragma once

#include "Application.h"
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include "NodeEditor/NodeObject.h"
#include "NodeEditor/NodeVariable.h"
#include "NodeEditor/NodeBuilder.h"
#include "NodeEditor/NodeEditor.h"

namespace Valden {
	class CScriptGenerator
	{
	public:
		CScriptGenerator( void );
		~CScriptGenerator();

		void PushScriptClass( const NodeClass_t& object );
		void Write( void );
	private:
		eastl::vector<const NodeClass_t *> m_Objects;
	};
};

#endif