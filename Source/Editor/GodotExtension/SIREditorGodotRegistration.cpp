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

#include <Editor/GodotExtension/SIREditorGodotSession.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

static void sirEditorGodotRegister( godot::ModuleInitializationLevel eLevel )
{
	if ( eLevel == godot::MODULE_INITIALIZATION_LEVEL_SCENE ) {
		godot::ClassDB::register_class<SIREditorGodotSession>();
	}
}

static void sirEditorGodotUnregister( godot::ModuleInitializationLevel eLevel )
{
	( void )eLevel;
}

extern "C" GDExtensionBool GDE_EXPORT sir_editor_gdextension_init( GDExtensionInterfaceGetProcAddress pGetProcAddress,
                                                                   GDExtensionClassLibraryPtr pLibrary,
                                                                   GDExtensionInitialization *pInitialization )
{
	godot::GDExtensionBinding::InitObject init( pGetProcAddress, pLibrary, pInitialization );
	init.register_initializer( sirEditorGodotRegister );
	init.register_terminator( sirEditorGodotUnregister );
	init.set_minimum_library_initialization_level( godot::MODULE_INITIALIZATION_LEVEL_SCENE );
	return init.init();
}
