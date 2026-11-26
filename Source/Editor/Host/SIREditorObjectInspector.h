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

#ifndef __SIR_EDITOROBJECTINSPECTOR_H__
#define __SIR_EDITOROBJECTINSPECTOR_H__
#include <Core/SIRPCH.h>
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
	SIRPropertyKind eKind;
	bool bHasValue;
	sirPropertyValue_t value;
} sirEditorInspectorEntry_t;

// Generates inspector descriptors from the canonical inherited object registry
// and the authoritative document. No parallel runtime object/type authority.
class SIREditorObjectInspector
{
public:
	SIREditorObjectInspector( void ) noexcept;
	SIREditorResult initialize( const SIREditorSceneDocument *pDocument, const SIRObjectRegistry *pRegistry ) noexcept;
	void shutdown( void ) noexcept;
	SIREditorResult inspect( uint64_t nNodeId, sirEditorInspectorEntry_t *pOutput, uint32_t nCapacity,
	                           uint32_t *pOutCount ) const noexcept;

protected:
private:
	const SIREditorSceneDocument *m_pDocument;
	const SIRObjectRegistry *m_pRegistry;
};
#endif // __SIR_EDITOROBJECTINSPECTOR_H__
