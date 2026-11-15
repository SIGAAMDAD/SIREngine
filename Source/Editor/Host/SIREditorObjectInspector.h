/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIREDITOROBJECTINSPECTOR_H__
#define __SIRENGINE_SIREDITOROBJECTINSPECTOR_H__
#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on
#include <Editor/Scene/SIREditorSceneDocument.h>

typedef struct sirEditorInspectorEntry_s
{
	uint32_t nPropertyId;
	uint64_t nNameAtom;
	sirPropertyKind_t eKind;
	bool bHasValue;
	sirPropertyValue_t value;
} sirEditorInspectorEntry_t;

// Generates inspector descriptors from the canonical inherited object registry
// and the authoritative document. No parallel runtime object/type authority.
class SIREditorObjectInspector
{
public:
	SIREditorObjectInspector( void ) noexcept;
	sirEditorResult_t initialize( const SIREditorSceneDocument *pDocument, const SIRObjectRegistry *pRegistry ) noexcept;
	void shutdown( void ) noexcept;
	sirEditorResult_t inspect( uint64_t nNodeId, sirEditorInspectorEntry_t *pOutput, uint32_t nCapacity,
	                           uint32_t *pOutCount ) const noexcept;

protected:
private:
	const SIREditorSceneDocument *m_pDocument;
	const SIRObjectRegistry *m_pRegistry;
};
#endif // __SIRENGINE_SIREDITOROBJECTINSPECTOR_H__
