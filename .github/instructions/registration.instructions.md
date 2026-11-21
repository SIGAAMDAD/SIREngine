---
name: SIR object registration and code generation
description: Preserve registration, RTTI, reflection and code generation contracts.
applyTo: 'Source/Core/Registration/**,Tools/RegistrationCooker/**,Source/Core/Objects/**'
---
# Registration and reflection

- Inspect existing registration/code-generation pipeline, native type IDs, generated consumers and migration/versioning before changing macros or RTTI.
- Preserve stable serialized names, inheritance behavior, registration order and required ABI. Basic RTTI remains available where the owning subsystem permits it.
- Keep code-generation tools separate from runtime-only dependencies; do not reintroduce runtime reflection or editor-side scanning without authorization.
- Test generated registration records and actual C++ translation-unit consumers. Distinguish build-time declarations from runtime performance claims.
