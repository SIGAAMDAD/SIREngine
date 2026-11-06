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

#ifndef __SIRENGINE_SIREDITORSCENEWORKSPACE_H__
#define __SIRENGINE_SIREDITORSCENEWORKSPACE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRLinearAllocator.h>
#include <Editor/Scene/SIREditorTextScene.h>
#include <Editor/Scene/SIREditorTextProperties.h>
#include <Editor/Scene/SIREditorResourceReferences.h>
#include <Editor/Widgets/SIREditorHierarchyModel.h>

class SIRMemorySegment;

typedef enum sirEditorWorkspaceResult_e : uint8_t
{
	SIR_EDITOR_WORKSPACE_OK = 0,
	SIR_EDITOR_WORKSPACE_NOT_INITIALIZED = 1,
	SIR_EDITOR_WORKSPACE_INVALID_ARGUMENT = 2,
	SIR_EDITOR_WORKSPACE_ALREADY_OPEN = 3,
	SIR_EDITOR_WORKSPACE_OUT_OF_MEMORY = 4,
	SIR_EDITOR_WORKSPACE_INVALID_SCENE = 5,
	SIR_EDITOR_WORKSPACE_INVALID_RESOURCES = 6,
	SIR_EDITOR_WORKSPACE_DOCUMENT_FAILED = 7,
	SIR_EDITOR_WORKSPACE_HIERARCHY_FAILED = 8,
	SIR_EDITOR_WORKSPACE_FILE_IO = 9,
	SIR_EDITOR_WORKSPACE_INVALID_PROPERTIES = 10
} sirEditorWorkspaceResult_t;

// Editor-main-thread source owner. The Godot scene reader remains authoritative
// for serialization: this workspace indexes source without rewriting it.
// Its buffers are exclusively owned by the supplied SIR_MEMORY_DOMAIN_EDITOR
// segment; the native view model can later be consumed by Godot Control/Tree.
class SIREditorSceneWorkspace
{
public:
	SIREditorSceneWorkspace( void ) noexcept;
	SIREditorSceneWorkspace( const SIREditorSceneWorkspace& ) = delete;
	~SIREditorSceneWorkspace();

	SIREditorSceneWorkspace& operator=( const SIREditorSceneWorkspace& ) = delete;

	bool isOpen( void ) const noexcept;
	uint32_t getErrorLine( void ) const noexcept;
	const char *getSource( void ) const noexcept;
	size_t getSourceBytes( void ) const noexcept;
	const sirEditorTextSceneIndex_t& getSceneIndex( void ) const noexcept;
	const sirEditorTextResourceIndex_t& getResourceIndex( void ) const noexcept;
	const sirEditorResourceReference_t *getResourceReferences( void ) const noexcept;
	uint32_t getResourceReferenceCount( void ) const noexcept;
	const sirEditorTextPropertyIndex_t& getPropertyIndex( void ) const noexcept;
	SIREditorSceneDocument& getDocument( void ) noexcept;
	const SIREditorSceneDocument& getDocument( void ) const noexcept;
	SIREditorHierarchyModel& getHierarchy( void ) noexcept;

	bool initialize( SIRMemorySegment *pEditorSegment ) noexcept;
	sirEditorWorkspaceResult_t openTextSource( const char *pszSource, size_t nSourceBytes ) noexcept;
	sirEditorWorkspaceResult_t openTextFile( const char *pszPath ) noexcept;
	void close( void ) noexcept;
	bool shutdown( void ) noexcept;

protected:
private:
	char *allocateSource( size_t nSourceBytes ) noexcept;
	sirEditorWorkspaceResult_t finishOpen( size_t nSourceBytes ) noexcept;
	void rollbackOpen( void ) noexcept;

	SIRLinearAllocator m_Arena;
	SIREditorSceneDocument m_Document;
	SIREditorHierarchyModel m_Hierarchy;
	sirEditorTextSceneIndex_t m_Scene;
	sirEditorTextResourceIndex_t m_Resources;
	sirEditorTextPropertyIndex_t m_Properties;
	sirEditorResourceReference_t *m_pResourceReferences = nullptr;
	uint32_t m_nResourceReferenceCount = 0;
	const char *m_pszSource;
	size_t m_nSourceBytes;
	uint32_t m_nErrorLine;
	bool m_bInitialized;
	bool m_bOpen;
};

#endif // __SIRENGINE_SIREDITORSCENEWORKSPACE_H__
