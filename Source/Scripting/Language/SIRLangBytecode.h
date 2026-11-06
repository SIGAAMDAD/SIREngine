/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRLANGBYTECODE_H__
#define __SIRENGINE_SIRLANGBYTECODE_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Scripting/Language/SIRLangLowering.h>

constexpr uint32_t SIR_LANG_BYTECODE_HEADER_BYTES = 96;
constexpr uint32_t SIR_LANG_BYTECODE_FUNCTION_BYTES = 32;
constexpr uint32_t SIR_LANG_BYTECODE_INSTRUCTION_BYTES = 32;
constexpr uint32_t SIR_LANG_BYTECODE_CAPABILITY_BYTES = 40;

// Stable, little-endian, alignment-independent program representation.
// SHA-256 is an integrity digest; authenticity remains a loader policy.
sirLangVMError_t SIRLangEncodeGraphVM( const sirLangCIRProgram_t *pProgram, uint8_t *pBytes, uint32_t nCapacity, uint32_t *pUsed ) noexcept;
sirLangVMError_t SIRLangDecodeGraphVM( const uint8_t *pBytes, uint32_t nBytes, sirLangCIRStorage_t *pStorage ) noexcept;

#endif // __SIRENGINE_SIRLANGBYTECODE_H__
