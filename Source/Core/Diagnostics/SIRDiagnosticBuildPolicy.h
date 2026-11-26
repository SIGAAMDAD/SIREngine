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

#ifndef __SIR_DIAGNOSTICBUILDPOLICY_H__
#define __SIR_DIAGNOSTICBUILDPOLICY_H__

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/SIRPCH.h>

// The target must select 1 for debug/development or 0 for shipping.
// This decision is independent of the CRT assertion configuration.
#ifndef SIR_DIAGNOSTIC_ASSERT_ENABLED
#error "Define SIR_DIAGNOSTIC_ASSERT_ENABLED to 0 or 1 for this target"
#endif

#define SIR_DIAGNOSTIC_POLICY_VALUE_0 1
#define SIR_DIAGNOSTIC_POLICY_VALUE_1 1
#define SIR_DIAGNOSTIC_POLICY_VALUE_IMPL( value ) SIR_DIAGNOSTIC_POLICY_VALUE_##value
#define SIR_DIAGNOSTIC_POLICY_VALUE( value ) SIR_DIAGNOSTIC_POLICY_VALUE_IMPL( value )
#if !SIR_DIAGNOSTIC_POLICY_VALUE( SIR_DIAGNOSTIC_ASSERT_ENABLED )
#error "SIR_DIAGNOSTIC_ASSERT_ENABLED must be 0 or 1"
#endif
#undef SIR_DIAGNOSTIC_POLICY_VALUE
#undef SIR_DIAGNOSTIC_POLICY_VALUE_IMPL
#undef SIR_DIAGNOSTIC_POLICY_VALUE_1
#undef SIR_DIAGNOSTIC_POLICY_VALUE_0

#endif // __SIR_DIAGNOSTICBUILDPOLICY_H__
