---
name: SIR editor and managed scripting
description: Preserve editor, UI and managed scripting integration contracts.
applyTo: 'Source/Editor/**,Source/UI/**,Source/Scripting/**,Build/CMake/SIRMono.cmake'
---
# Editor, UI and scripting

- Keep editor-only authoring state isolated from cooked runtime services and shipping memory budgets.
- Inspect the actual scripting runtime and managed embedding ABI before changing ownership, threading, error propagation or instance lifetimes.
- Treat reflection/registration, native-to-managed handles and exported object identity as compatibility contracts; test teardown and invalid handles.
- Do not invent binding names or assume optional Mono/runtime SDK presence. Use existing feature gates and exact source configuration.
- For UI/editor edits, preserve existing threading and rendering affinity, serialization and plugin lifecycle.
