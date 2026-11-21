---
name: SIR resources and cooking
description: Apply cooked-data, streaming-asset and compatibility rules.
applyTo: 'Source/Resources/**,Source/Core/VFS/**,Tools/ShaderCooker/**,Tools/RegistrationCooker/**,Tools/StringCatalogCooker/**'
---
# Resources and cooking

- Cooked formats and package schemas are compatibility contracts: inspect source readers/writers, version tags, endianness, offsets and alignment before changing.
- Keep editor-time authoring/cook dependencies separate from standalone runtime loading and streaming.
- Test missing, corrupt, short, oversized and version-skewed asset cases where relevant.
- Avoid rebuilding, recompressing or copying whole assets when a bounded incremental path exists. Measure changes to RAM, disk and CPU use.
