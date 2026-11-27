/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_INPUT_BINDING_MAP_H__
#define __SIR_INPUT_BINDING_MAP_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRFlatHashMap.h>
#include <Core/Strings/SIRStringPool.h>

enum class SIRInputBindType : uint8_t
{
	Button,
	Axis
};

struct SIRInputBinding
{
	SIRStringAtom bindName;
	SIRInputBindType eType;
};

class SIRInputBindingMap final
{
public:
	explicit SIRInputBindingMap( void ) noexcept;
	~SIRInputBindingMap() noexcept;

	bool addBind( const SIRInputBinding& binding ) noexcept;
	bool removeBind( const SIRStringAtom& bindName ) noexcept;

	const SIRInputBinding *getBind( const SIRStringAtom& bindName ) const noexcept;

private:
	SIRFlatHashMap<uint32_t, SIRInputBinding> m_Binds;
};

#endif // __SIR_INPUT_BINDING_MAP_H__
