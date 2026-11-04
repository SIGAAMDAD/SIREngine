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

#ifndef __SIRENGINE_SIREDITORGODOTSESSION_H__
#define __SIRENGINE_SIREDITORGODOTSESSION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/SIRMemoryManager.h>
#include <Editor/Bridge/SIREditorNativeBridge.h>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

class SIRMemorySegment;

// Editor-only native adapter. Godot owns its Control and Variant objects;
// SIRMemoryManager owns the separate native authoring workspace bytes.
class SIREditorGodotSession : public godot::RefCounted
{
	GDCLASS( SIREditorGodotSession, godot::RefCounted );

private:
	static constexpr size_t BRIDGE_STORAGE_CAPACITY = 4096;
	alignas( 64 ) unsigned char m_szBridgeStorage[BRIDGE_STORAGE_CAPACITY];
	SIRMemoryManager m_Memory;
	SIRMemorySegment *m_pEditorSegment = nullptr;
	sirEditorNativeBridgeHandle_t *m_pBridge = nullptr;
	bool m_bMemoryInitialized = false;
	godot::String m_sScenePath;
	godot::String m_sLastError;

	bool initializeBridge( void ) noexcept;
	godot::String copyField( uint64_t nGeneration, sirEditorBridgeStringField_t eField, uint32_t iRecord, bool *pValid ) const;
	static godot::String formatId( uint64_t nValue );

protected:
	static void _bind_methods( void );

public:
	SIREditorGodotSession( void ) = default;
	~SIREditorGodotSession() override;

	bool open_scene( const godot::String &sPath );
	void close_scene( void ) noexcept;
	godot::Dictionary get_snapshot( void ) const;
	godot::String get_last_error( void ) const;
};

#endif // __SIRENGINE_SIREDITORGODOTSESSION_H__
