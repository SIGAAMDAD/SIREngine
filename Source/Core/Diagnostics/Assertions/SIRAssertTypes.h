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

#ifndef __SIR_ASSERTTYPES_H__
#define __SIR_ASSERTTYPES_H__

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/SIRPCH.h>

#include <Core/Diagnostics/SIRDiagnosticSourceLocation.h>

typedef struct sirAssertContext_s
{
	const char *pszExpression;
	sirDiagnosticSourceLocation_t source;
} sirAssertContext_t;

#endif // __SIR_ASSERTTYPES_H__
