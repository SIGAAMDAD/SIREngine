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

#ifndef __SIR_RENDER_TYPES_H__
#define __SIR_RENDER_TYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Core/Containers/SIRSpan.h>
#include <Core/Strings/SIRFixedString.h>

/// Return status for RHI calls.
enum class SIRRenderResult : uint8_t
{
    Success, ///< Operation completed successfully.
    InvalidArgument, ///< A supplied argument is invalid.
    InvalidHandle, ///< The referenced handle is missing, stale, or of the wrong type.
    Unsupported, ///< The requested operation is not supported.
    OutOfMemory, ///< The device or host allocator cannot satisfy the allocation.
    CapacityExceeded, ///< A reported resource or device limit was exceeded.
    Busy, ///< The requested object is currently occupied or in use.
    Timeout, ///< The operation exceeded its permitted wait duration.
    NotReady, ///< The result is not available yet.
    CompileFailed, ///< Shader or pipeline compilation failed.
    ResourceHazard, ///< The operation conflicts with a resource-use dependency.
    IncompatibleFormat, ///< The requested format or format pairing is incompatible.
    SurfaceChanged, ///< The presentation surface changed and must be refreshed.
    SurfaceUnavailable, ///< No usable presentation surface is available.
    DeviceLost, ///< The native graphics device was lost.
    DriverError ///< An unrecoverable or unclassified backend error occurred.
};

/// Native graphics API or backend selection.
enum class SIRRenderBackend : uint8_t
{
    Auto, ///< Select a suitable backend automatically.
    Vulkan, ///< Use the Vulkan graphics API.
    D3D12, ///< Use Direct3D 12.
    D3D11, ///< Use Direct3D 11.
    D3D10, ///< Use Direct3D 10.
    D3D9, ///< Use Direct3D 9.
    D3D8, ///< Use Direct3D 8.
    D3D7, ///< Use Direct3D 7.
    OpenGL, ///< Use OpenGL.
    Metal, ///< Use Apple's Metal API.
    Software, ///< Use a software-rendering implementation.
    Null, ///< Use a null backend for validation or testing.
    Custom ///< Use an externally provided backend.
};

/// Variant of a backend, such as Direct3D 9Ex.
enum class SIRRenderBackendVariant : uint8_t
{
    Default, ///< Use the normal variant of the selected API.
    D3D9Ex ///< Use the Direct3D 9Ex device variant.
};

/// Rendering pipeline family used to lower frame work.
enum class SIRPipelineEra : uint8_t
{
    Advanced, ///< Use the engine's advanced programmable rendering path.
    Programmable, ///< Use the conventional programmable-shader rendering path.
    FixedFunction ///< Use the fixed-function graphics pipeline path.
};

/// How the backend records and executes GPU commands.
enum class SIRRenderExecutionModel : uint8_t
{
    ExplicitQueues, ///< Commands are explicitly recorded and submitted to queues.
    StatefulDevice, ///< A mutable API device executes submitted commands.
    ContextSerial ///< Commands execute serially on an owning graphics context.
};

/// Requested OpenGL context/profile type.
enum class SIRRenderGLProfile : uint8_t
{
    NotApplicable, ///< An OpenGL profile is irrelevant to this backend.
    Core, ///< Request an OpenGL core-profile context.
    Compatibility, ///< Request an OpenGL compatibility-profile context.
    PreProfile ///< Use OpenGL from before explicit profile selection.
};

/// Current lifecycle state of the rendering device.
enum class SIRRenderDeviceState : uint8_t
{
    Creating, ///< Device initialization is underway.
    Ready, ///< The device can accept normal operations.
    SurfaceSuspended, ///< Presentation is suspended while device resources remain available.
    Lost, ///< The native device is unusable after device loss.
    Recovering, ///< The device is rebuilding its native state.
    Shutdown ///< The device is shutting down or has shut down.
};

/// Logical purpose of a submission queue.
enum class SIRRenderQueueType : uint8_t
{
    Graphics, ///< Queue for rasterization and graphics work.
    Compute, ///< Queue for compute dispatches.
    Transfer, ///< Queue for resource transfers and copies.
    Count ///< Sentinel representing the number of queue categories.
};

/// Resource category encoded in an opaque handle.
enum class SIRRenderHandleType : uint8_t
{
    None, ///< Invalid or uninitialized handle type.
    Heap, ///< GPU memory heap allocation.
    Buffer, ///< General-purpose buffer resource.
    Texture, ///< Texture or image resource.
    TextureView, ///< View into texture subresources.
    BufferView, ///< View into a buffer byte range.
    Sampler, ///< Texture sampler state.
    Shader, ///< Compiled shader resource.
    PipelineLayout, ///< Shader-resource binding and constant layout.
    Pipeline, ///< Executable or cached pipeline state.
    BindingSet, ///< Bound resource collection compatible with a layout.
    Framebuffer, ///< Attachment group or framebuffer.
    QueryPool, ///< Collection of GPU queries.
    AccelerationStructure, ///< Ray-tracing acceleration structure.
    ShaderTable, ///< Ray-tracing shader record table.
    PipelineCache, ///< Cached pipeline compilation data.
    Surface, ///< Native presentation surface.
    Swapchain ///< Collection of presentable images.
};

/// Bit flags advertising optional device features.
enum class SIRRenderFeature : uint64_t
{
    Graphics = 1ULL << 0, ///< Device can execute graphics drawing commands.
    Compute = 1ULL << 1, ///< Device supports compute workloads.
    AsyncCompute = 1ULL << 2, ///< Compute can overlap or submit independently where supported.
    DedicatedTransfer = 1ULL << 3, ///< A distinct transfer queue is available.
    IndirectDraw = 1ULL << 4, ///< GPU reads draw arguments from a buffer.
    IndirectCount = 1ULL << 5, ///< GPU reads the number of indirect commands from a buffer.
    MultiDrawIndirect = 1ULL << 6, ///< One call can consume multiple indirect draw commands.
    Instancing = 1ULL << 7, ///< Hardware or API supports instance-based drawing.
    BaseVertex = 1ULL << 8, ///< Indexed drawing supports a base vertex offset.
    BaseInstance = 1ULL << 9, ///< Draws support a base instance offset.
    MultipleRenderTargets = 1ULL << 10, ///< Passes can write more than one color target.
    IndependentBlending = 1ULL << 11, ///< Blend settings may differ between render targets.
    DualSourceBlending = 1ULL << 12, ///< Blending can use a second shader color output.
    TextureArrays = 1ULL << 13, ///< Array textures are supported.
    CubeArrays = 1ULL << 14, ///< Arrays of cube textures are supported.
    Multisampling = 1ULL << 15, ///< Multisampled images and rendering are supported.
    StorageResources = 1ULL << 16, ///< Shaders can read or write storage resources.
    BufferTextures = 1ULL << 17, ///< Shaders can sample formatted buffer data as texels.
    Tessellation = 1ULL << 18, ///< Tessellation shader stages are available.
    GeometryShaders = 1ULL << 19, ///< Geometry shader stages are available.
    MeshShaders = 1ULL << 20, ///< Task or mesh shader pipelines are available.
    RayTracing = 1ULL << 21, ///< Hardware ray-tracing operations are available.
    AccelerationUpdate = 1ULL << 22, ///< Existing acceleration structures may be updated.
    DescriptorIndexing = 1ULL << 23, ///< Shader resource descriptors can be indexed dynamically.
    BindlessResources = 1ULL << 24, ///< Shader-visible resources can use bindless addressing.
    PushConstants = 1ULL << 25, ///< Small inline shader constants are available.
    DynamicUniformOffsets = 1ULL << 26, ///< Uniform-buffer bindings accept dynamic offsets.
    PersistentMapping = 1ULL << 27, ///< Buffers can remain mapped across operations.
    GPUDeviceAddress = 1ULL << 28, ///< Shaders can address GPU memory using device addresses.
    DeviceLocalHostVisible = 1ULL << 29, ///< Host-visible memory may also be device-local.
    SparseResources = 1ULL << 30, ///< Resources can use sparse residency.
    SparseTileBinding = 1ULL << 31, ///< Individual sparse tiles can be committed or released.
    SamplerFeedback = 1ULL << 32, ///< GPU can provide feedback about texture sampling.
    VariableRateShading = 1ULL << 33, ///< Shading rate can vary over screen regions.
    ConservativeRaster = 1ULL << 34, ///< Conservative rasterization is available.
    DepthBounds = 1ULL << 35, ///< Depth-bounds testing is available.
    DepthClamp = 1ULL << 36, ///< Depth values can be clamped instead of clipped.
    DepthResolve = 1ULL << 37, ///< Depth buffers support multisample resolve.
    LogicOperations = 1ULL << 38, ///< Boolean logic operations are supported for output merging.
    TimelineSynchronization = 1ULL << 39, ///< Synchronization supports monotonically increasing values.
    ExplicitSynchronization = 1ULL << 40, ///< API exposes explicit resource synchronization.
    NativeMemoryHeaps = 1ULL << 41, ///< Explicit native memory heaps can be allocated.
    TransientAliasing = 1ULL << 42, ///< Transient resources can reuse compatible memory regions.
    SecondaryCommandLists = 1ULL << 43, ///< Secondary or reusable command lists are supported.
    PipelineStatistics = 1ULL << 44, ///< GPU pipeline statistics can be queried.
    TimestampQueries = 1ULL << 45, ///< GPU timestamps can be recorded and queried.
    OcclusionQueries = 1ULL << 46, ///< Visibility/occlusion queries are supported.
    FixedFunction = 1ULL << 47, ///< A native fixed-function graphics path exists.
    FixedTextureCombiners = 1ULL << 48, ///< Fixed-function texture-combiner stages are available.
    Present = 1ULL << 49, ///< Rendered images can be presented to a surface.
    HDRPresent = 1ULL << 50, ///< HDR output formats/color spaces are supported.
    Tearing = 1ULL << 51, ///< Presentation may occur without vertical-sync blocking.
    DebugMarkers = 1ULL << 52, ///< Diagnostic labels and markers are supported.
    ExternalMemory = 1ULL << 53, ///< Resources may interoperate through external memory handles.
    SoftwareRasterization = 1ULL << 54 ///< Rendering can be performed using a software rasterizer.
};

/// Bit flags describing permissible buffer uses.
enum class SIRRenderBufferUsage : uint64_t
{
    None = 0, ///< No buffer usage has been selected.
    Vertex = 1ULL << 0, ///< Buffer supplies vertex attributes.
    Index = 1ULL << 1, ///< Buffer supplies index data.
    Uniform = 1ULL << 2, ///< Buffer supplies shader uniform/constant data.
    Storage = 1ULL << 3, ///< Buffer can be accessed as shader storage.
    Texel = 1ULL << 4, ///< Buffer can be viewed as formatted texels.
    Indirect = 1ULL << 5, ///< Buffer holds GPU-generated indirect command arguments.
    CopySource = 1ULL << 6, ///< Buffer is allowed as a copy source.
    CopyDestination = 1ULL << 7, ///< Buffer is allowed as a copy destination.
    AccelerationStructure = 1ULL << 8, ///< Buffer can back a ray-tracing acceleration structure.
    AccelerationScratch = 1ULL << 9, ///< Buffer serves as scratch space while building acceleration structures.
    ShaderTable = 1ULL << 10, ///< Buffer holds ray-tracing shader-table records.
    DeviceAddress = 1ULL << 11, ///< Buffer may be referenced through a GPU device address.
    QueryResults = 1ULL << 12 ///< Buffer can hold resolved GPU query data.
};

/// Bit flags describing permissible texture uses.
enum class SIRRenderTextureUsage : uint64_t
{
    Sampled = 1ULL << 0, ///< Texture can be sampled by shaders.
    Storage = 1ULL << 1, ///< Texture can be accessed as shader storage.
    ColorAttachment = 1ULL << 2, ///< Texture can serve as a color render target.
    DepthStencilAttachment = 1ULL << 3, ///< Texture can serve as a depth/stencil render target.
    CopySource = 1ULL << 4, ///< Texture can supply source data in transfers.
    CopyDestination = 1ULL << 5, ///< Texture can receive destination data in transfers.
    InputAttachment = 1ULL << 6, ///< Texture can be read as an input attachment where supported.
    Present = 1ULL << 7, ///< Texture can be used as a presentable image.
    ResolveSource = 1ULL << 8, ///< Texture can be the source of a multisample resolve.
    ResolveDestination = 1ULL << 9 ///< Texture can receive a multisample resolve.
};

/// High-level GPU memory placement preference.
enum class SIRRenderMemoryClass : uint8_t
{
	None, ///< No memory access type has been selected.
    DeviceLocal, ///< Prefer device-local memory for GPU access.
    Upload, ///< Prefer host-writable memory for uploading data.
    Readback, ///< Prefer host-readable memory for retrieving results.
    Unified, ///< Use memory accessible to both the CPU and GPU.
    Transient ///< Use short-lived memory suitable for frame-lifetime reuse.
};

/// Individual properties of a memory allocation.
enum class SIRRenderMemoryProperty : uint32_t
{
    DeviceLocal = 1U << 0, ///< Memory has device-local access characteristics.
    HostVisible = 1U << 1, ///< Memory can be mapped into host address space.
    HostCoherent = 1U << 2, ///< Host writes do not require a separate coherence flush.
    HostCached = 1U << 3, ///< Host reads may benefit from CPU caching.
    PersistentMapped = 1U << 4 ///< The allocation supports a persistently mapped pointer.
};

/// Resource allocation and residency options.
enum class SIRRenderResourceFlag : uint32_t
{
    Dedicated = 1U << 0, ///< Request an allocation dedicated to this resource.
    Aliasable = 1U << 1, ///< Resource may share/reuse physical memory with compatible resources.
    Sparse = 1U << 2, ///< Resource uses sparse or partially committed backing.
    Imported = 1U << 3, ///< Resource was created outside the normal allocation path.
    Protected = 1U << 4 ///< Resource requires protected-memory handling.
};

/// CPU mapping access and synchronization flags.
enum class SIRRenderMapFlag : uint32_t
{
    Read = 1U << 0, ///< Host mapping permits reading.
    Write = 1U << 1, ///< Host mapping permits writing.
    Discard = 1U << 2, ///< Previous contents may be discarded during a write mapping.
    Persistent = 1U << 3, ///< Keep the mapping valid across operations where supported.
    ExplicitFlush = 1U << 4 ///< Caller explicitly flushes modified noncoherent ranges.
};

/// Element, pixel, depth, or compressed texture format.
enum class SIRRenderFormat : uint16_t
{
    Unknown, ///< No specific pixel or texel format is selected.
    R8_UNORM, ///< Unsigned normalized texel with R=8b components.
    R8_SNORM, ///< Signed normalized texel with R=8b components.
    R8_UINT, ///< Unsigned integer texel with R=8b components.
    R8_SINT, ///< Signed integer texel with R=8b components.
    RG8_UNORM, ///< Unsigned normalized texel with G=8b components.
    RG8_SNORM, ///< Signed normalized texel with G=8b components.
    RG8_UINT, ///< Unsigned integer texel with G=8b components.
    RG8_SINT, ///< Signed integer texel with G=8b components.
    RGBA8_UNORM, ///< Unsigned normalized texel with A=8b components.
    RGBA8_SNORM, ///< Signed normalized texel with A=8b components.
    RGBA8_UINT, ///< Unsigned integer texel with A=8b components.
    RGBA8_SINT, ///< Signed integer texel with A=8b components.
    RGBA8_SRGB, ///< Srgb-encoded texel with A=8b components.
    BGRA8_UNORM, ///< Unsigned normalized texel with A=8b components.
    BGRA8_SRGB, ///< Srgb-encoded texel with A=8b components.
    R16_UNORM, ///< Unsigned normalized texel with R=16b components.
    R16_SNORM, ///< Signed normalized texel with R=16b components.
    R16_UINT, ///< Unsigned integer texel with R=16b components.
    R16_SINT, ///< Signed integer texel with R=16b components.
    R16_FLOAT, ///< Floating-point texel with R=16b components.
    RG16_UNORM, ///< Unsigned normalized texel with G=16b components.
    RG16_SNORM, ///< Signed normalized texel with G=16b components.
    RG16_UINT, ///< Unsigned integer texel with G=16b components.
    RG16_SINT, ///< Signed integer texel with G=16b components.
    RG16_FLOAT, ///< Floating-point texel with G=16b components.
    RGBA16_UNORM, ///< Unsigned normalized texel with A=16b components.
    RGBA16_SNORM, ///< Signed normalized texel with A=16b components.
    RGBA16_UINT, ///< Unsigned integer texel with A=16b components.
    RGBA16_SINT, ///< Signed integer texel with A=16b components.
    RGBA16_FLOAT, ///< Floating-point texel with A=16b components.
    R32_UINT, ///< Unsigned integer texel with R=32b components.
    R32_SINT, ///< Signed integer texel with R=32b components.
    R32_FLOAT, ///< Floating-point texel with R=32b components.
    RG32_UINT, ///< Unsigned integer texel with G=32b components.
    RG32_SINT, ///< Signed integer texel with G=32b components.
    RG32_FLOAT, ///< Floating-point texel with G=32b components.
    RGB32_FLOAT, ///< Floating-point texel with B=32b components.
    RGBA32_UINT, ///< Unsigned integer texel with A=32b components.
    RGBA32_SINT, ///< Signed integer texel with A=32b components.
    RGBA32_FLOAT, ///< Floating-point texel with A=32b components.
    RGB10A2_UNORM, ///< Packed 10-bit RGB plus 2-bit alpha, all unsigned normalized.
    RGB10A2_UINT, ///< Packed 10-bit RGB plus 2-bit alpha, all unsigned integers.
    R11G11B10_FLOAT, ///< Packed 11-bit red/green and 10-bit blue floating-point values.
    RGB9E5_UFLOAT, ///< RGB color with 9-bit mantissas and a shared 5-bit exponent.
    RGB565_UNORM, ///< Packed 5-bit red, 6-bit green, 5-bit blue normalized color.
    RGBA5551_UNORM, ///< Packed 5-bit RGB plus 1-bit normalized alpha.
    RGBA4444_UNORM, ///< Packed 4-bit normalized RGBA components.
    D16_UNORM, ///< 16-bit unsigned normalized depth texture.
    D24_UNORM_S8_UINT, ///< 24-bit normalized depth plus 8-bit unsigned stencil.
    D32_FLOAT, ///< 32-bit floating-point depth texture.
    D32_FLOAT_S8_UINT, ///< 32-bit floating-point depth plus 8-bit stencil.
    BC1_UNORM, ///< 4x4 block-compressed RGB/optional 1-bit alpha with unsigned normalized interpretation.
    BC1_SRGB, ///< 4x4 block-compressed RGB/optional 1-bit alpha with sRGB-encoded interpretation.
    BC2_UNORM, ///< 4x4 block-compressed color with explicit alpha with unsigned normalized interpretation.
    BC2_SRGB, ///< 4x4 block-compressed color with explicit alpha with sRGB-encoded interpretation.
    BC3_UNORM, ///< 4x4 block-compressed color with interpolated alpha with unsigned normalized interpretation.
    BC3_SRGB, ///< 4x4 block-compressed color with interpolated alpha with sRGB-encoded interpretation.
    BC4_UNORM, ///< 4x4 block-compressed single-channel data with unsigned normalized interpretation.
    BC4_SNORM, ///< 4x4 block-compressed single-channel data with signed normalized interpretation.
    BC5_UNORM, ///< 4x4 block-compressed two-channel data with unsigned normalized interpretation.
    BC5_SNORM, ///< 4x4 block-compressed two-channel data with signed normalized interpretation.
    BC6H_UFLOAT, ///< 4x4 block-compressed HDR RGB data with unsigned floating-point interpretation.
    BC6H_SFLOAT, ///< 4x4 block-compressed HDR RGB data with signed floating-point interpretation.
    BC7_UNORM, ///< 4x4 block-compressed RGB/RGBA color with unsigned normalized interpretation.
    BC7_SRGB, ///< 4x4 block-compressed RGB/RGBA color with sRGB-encoded interpretation.
    ETC2_RGB8_UNORM, ///< ETC2 block-compressed RGB color with unsigned normalized interpretation.
    ETC2_RGB8_SRGB, ///< ETC2 block-compressed RGB color with sRGB-encoded interpretation.
    ETC2_RGBA8_UNORM, ///< ETC2 block-compressed RGBA color with unsigned normalized interpretation.
    ETC2_RGBA8_SRGB, ///< ETC2 block-compressed RGBA color with sRGB-encoded interpretation.
    ASTC_4X4_UNORM, ///< ASTC 4x4 block-compressed color with unsigned normalized interpretation.
    ASTC_4X4_SRGB, ///< ASTC 4x4 block-compressed color with sRGB-encoded interpretation.
    ASTC_8X8_UNORM, ///< ASTC 8x8 block-compressed color with unsigned normalized interpretation.
    ASTC_8X8_SRGB ///< ASTC 8x8 block-compressed color with sRGB-encoded interpretation.
};

/// Operations supported for a specific format.
enum class SIRRenderFormatFeature : uint32_t
{
    Sampled = 1U << 0, ///< Format can be sampled by a shader.
    LinearFilter = 1U << 1, ///< Format supports linear texture filtering.
    ColorTarget = 1U << 2, ///< Format can be attached as a color render target.
    DepthTarget = 1U << 3, ///< Format can be attached as a depth target.
    Blendable = 1U << 4, ///< Format supports color blending.
    Storage = 1U << 5, ///< Format can be read or written as shader storage.
    Atomic = 1U << 6, ///< Format permits supported atomic shader operations.
    Vertex = 1U << 7, ///< Format can be consumed as a vertex attribute.
    BufferView = 1U << 8, ///< Format can be used in a formatted buffer view.
    BlitSource = 1U << 9, ///< Format is eligible as a blit source.
    BlitTarget = 1U << 10, ///< Format is eligible as a blit destination.
    Resolve = 1U << 11 ///< Format supports the requested resolve operations.
};

/// Shape of a texture resource.
enum class SIRRenderTextureDimension : uint8_t
{
    Texture1D, ///< One-dimensional texture coordinate space.
    Texture2D, ///< Two-dimensional texture coordinate space.
    Texture3D, ///< Three-dimensional volumetric texture.
    Cube ///< Six-face cube-map texture.
};

/// Texture plane or depth/stencil aspect selection.
enum class SIRRenderTextureAspect : uint32_t
{
    Color = 1U << 0, ///< Select the color aspect of a texture.
    Depth = 1U << 1, ///< Select the depth aspect.
    Stencil = 1U << 2, ///< Select the stencil aspect.
    Plane0 = 1U << 3, ///< Select the first plane of a multiplanar texture.
    Plane1 = 1U << 4, ///< Select the second plane of a multiplanar texture.
    Plane2 = 1U << 5 ///< Select the third plane of a multiplanar texture.
};

/// Accepted shader-source or shader-bytecode representation.
enum class SIRRenderShaderFormat : uint8_t
{
    DXIL, ///< Shader bytecode in DirectX Intermediate Language.
    DXBC, ///< Shader bytecode in DirectX Bytecode format.
    D3DShaderTokens, ///< Legacy Direct3D shader instruction-token stream.
    SPIRV, ///< Shader code in SPIR-V format.
    GLSL, ///< Shader source code in GLSL.
    MSL, ///< Shader source code in Metal Shading Language.
    NativeBinary ///< Backend-specific opaque compiled shader binary.
};

/// One programmable shader stage.
enum class SIRRenderShaderStage : uint8_t
{
    Vertex, ///< Processes vertex inputs and produces clip-space vertices.
    Fragment, ///< Computes fragment/pixel outputs.
    Geometry, ///< Can generate or modify primitives.
    TessellationControl, ///< Computes tessellation control factors.
    TessellationEvaluation, ///< Evaluates tessellated primitives.
    Compute, ///< Executes a general-purpose compute workgroup.
    Task, ///< Prepares work for a mesh shader stage.
    Mesh, ///< Emits mesh geometry without conventional vertex inputs.
    RayGeneration, ///< Entry point that launches ray-tracing work.
    AnyHit, ///< Processes optional hits along a ray.
    ClosestHit, ///< Processes the selected nearest hit.
    Miss, ///< Handles rays that hit no geometry.
    Intersection, ///< Intersects rays with procedural primitives.
    Callable ///< Callable shader invoked from ray-tracing programs.
};

/// Bit mask for shader-stage visibility.
enum class SIRRenderShaderStageFlag : uint32_t
{
    Vertex = 1U << 0, ///< Enable visibility for the vertex stage.
    Fragment = 1U << 1, ///< Enable visibility for the fragment stage.
    Geometry = 1U << 2, ///< Enable visibility for the geometry stage.
    TessellationControl = 1U << 3, ///< Enable visibility for tessellation control.
    TessellationEvaluation = 1U << 4, ///< Enable visibility for tessellation evaluation.
    Compute = 1U << 5, ///< Enable visibility for compute shaders.
    Task = 1U << 6, ///< Enable visibility for task shaders.
    Mesh = 1U << 7, ///< Enable visibility for mesh shaders.
    RayGeneration = 1U << 8, ///< Enable visibility for ray-generation shaders.
    AnyHit = 1U << 9, ///< Enable visibility for any-hit shaders.
    ClosestHit = 1U << 10, ///< Enable visibility for closest-hit shaders.
    Miss = 1U << 11, ///< Enable visibility for miss shaders.
    Intersection = 1U << 12, ///< Enable visibility for intersection shaders.
    Callable = 1U << 13 ///< Enable visibility for callable shaders.
};

/// Kind of executable pipeline being created.
enum class SIRRenderPipelineType : uint8_t
{
    Graphics, ///< Conventional graphics rasterization pipeline.
    Compute, ///< Compute dispatch pipeline.
    Mesh, ///< Graphics pipeline driven by mesh shaders.
    RayTracing, ///< Ray-tracing pipeline with shader groups.
    FixedFunction ///< Native fixed-function graphics configuration.
};

/// Topology used to assemble input primitives.
enum class SIRRenderPrimitiveTopology : uint8_t
{
    Points, ///< Each vertex defines an independent point.
    LineList, ///< Every two vertices form an independent line.
    LineStrip, ///< Consecutive vertices form connected lines.
    TriangleList, ///< Every three vertices form an independent triangle.
    TriangleStrip, ///< Consecutive vertices form connected triangles.
    TriangleFan, ///< Triangles fan out around a shared starting vertex.
    PatchList, ///< Control points are grouped into tessellation patches.
    LineAdjacency, ///< Line primitives include adjacent vertices.
    TriangleAdjacency ///< Triangle primitives include adjacent vertices.
};

/// Index element width for indexed draws.
enum class SIRRenderIndexType : uint8_t
{
    UInt16, ///< Indices are unsigned 16-bit integers.
    UInt32 ///< Indices are unsigned 32-bit integers.
};

/// Whether vertex data advances per vertex or per instance.
enum class SIRRenderVertexRate : uint8_t
{
    PerVertex, ///< Advance the vertex input for each vertex.
    PerInstance ///< Advance the vertex input for each instance.
};

/// Which faces the rasterizer discards.
enum class SIRRenderCullMode : uint8_t
{
    None, ///< Do not discard faces by winding.
    Front, ///< Discard front-facing primitives.
    Back ///< Discard back-facing primitives.
};

/// Winding order treated as front-facing.
enum class SIRRenderFrontFace : uint8_t
{
    CounterClockwise, ///< Counter-clockwise winding denotes the front face.
    Clockwise ///< Clockwise winding denotes the front face.
};

/// Filled, wireframe, or point rasterization mode.
enum class SIRRenderPolygonMode : uint8_t
{
    Fill, ///< Rasterize filled polygon interiors.
    Line, ///< Rasterize polygon edges as lines.
    Point ///< Rasterize polygon vertices as points.
};

/// Comparison operation for depth, stencil, or sampling.
enum class SIRRenderCompareOp : uint8_t
{
    Never, ///< Comparison never passes.
    Less, ///< Pass if source is less than the reference.
    Equal, ///< Pass if source equals the reference.
    LessOrEqual, ///< Pass if source is less than or equal to the reference.
    Greater, ///< Pass if source is greater than the reference.
    NotEqual, ///< Pass if source differs from the reference.
    GreaterOrEqual, ///< Pass if source is greater than or equal to the reference.
    Always ///< Comparison always passes.
};

/// Action performed on a stencil-test result.
enum class SIRRenderStencilOp : uint8_t
{
    Keep, ///< Preserve the existing stencil value.
    Zero, ///< Write zero to the stencil value.
    Replace, ///< Replace the stencil value with the reference.
    IncrementClamp, ///< Increment and clamp at the maximum value.
    DecrementClamp, ///< Decrement and clamp at zero.
    Invert, ///< Invert the stencil bits.
    IncrementWrap, ///< Increment and wrap to zero on overflow.
    DecrementWrap ///< Decrement and wrap to maximum on underflow.
};

/// Source/destination multiplier used when blending.
enum class SIRRenderBlendFactor : uint8_t
{
    Zero, ///< Use a factor of zero.
    One, ///< Use a factor of one.
    SourceColor, ///< Use the source RGB color as the factor.
    OneMinusSourceColor, ///< Use one minus the source RGB color.
    DestinationColor, ///< Use the destination RGB color.
    OneMinusDestinationColor, ///< Use one minus the destination RGB color.
    SourceAlpha, ///< Use the source alpha component.
    OneMinusSourceAlpha, ///< Use one minus the source alpha component.
    DestinationAlpha, ///< Use the destination alpha component.
    OneMinusDestinationAlpha, ///< Use one minus the destination alpha component.
    ConstantColor, ///< Use the constant blend color.
    OneMinusConstantColor, ///< Use one minus the constant blend color.
    ConstantAlpha, ///< Use the constant blend alpha.
    OneMinusConstantAlpha, ///< Use one minus the constant blend alpha.
    SourceAlphaSaturate, ///< Use the saturating source-alpha factor.
    Source1Color, ///< Use the second source color output.
    OneMinusSource1Color, ///< Use one minus the second source color.
    Source1Alpha, ///< Use the second source alpha output.
    OneMinusSource1Alpha ///< Use one minus the second source alpha.
};

/// Arithmetic operation combining blend inputs.
enum class SIRRenderBlendOp : uint8_t
{
    Add, ///< Add the weighted source and destination.
    Subtract, ///< Subtract destination contribution from source contribution.
    ReverseSubtract, ///< Subtract source contribution from destination contribution.
    Minimum, ///< Choose the smaller source/destination component.
    Maximum ///< Choose the larger source/destination component.
};

/// Per-channel logic operation on color output.
enum class SIRRenderLogicOp : uint8_t
{
    Clear, ///< Output zero bits.
    And, ///< Output source AND destination.
    AndReverse, ///< Output source AND (NOT destination).
    Copy, ///< Output the source value.
    AndInverted, ///< Output (NOT source) AND destination.
    NoOp, ///< Preserve the destination value.
    Xor, ///< Output source XOR destination.
    Or, ///< Output source OR destination.
    Nor, ///< Output NOT (source OR destination).
    Equivalent, ///< Output NOT (source XOR destination).
    Invert, ///< Output bitwise NOT source.
    OrReverse, ///< Output source OR (NOT destination).
    CopyInverted, ///< Output bitwise NOT source, ignoring destination.
    OrInverted, ///< Output (NOT source) OR destination.
    Nand, ///< Output NOT (source AND destination).
    Set ///< Output all one bits.
};

/// Minification or magnification sampling filter.
enum class SIRRenderFilter : uint8_t
{
    Nearest, ///< Select the nearest sample.
    Linear, ///< Interpolate neighboring samples linearly.
    Cubic ///< Use cubic interpolation where supported.
};

/// Filter used to select mip levels.
enum class SIRRenderMipFilter : uint8_t
{
    None, ///< Do not filter between mip levels.
    Nearest, ///< Choose the closest mip level.
    Linear ///< Interpolate between adjacent mip levels.
};

/// Behavior when a texture coordinate is outside its range.
enum class SIRRenderAddressMode : uint8_t
{
    Repeat, ///< Wrap coordinates periodically.
    MirroredRepeat, ///< Repeat with mirrored alternating intervals.
    ClampToEdge, ///< Clamp coordinates to the texture edge.
    ClampToBorder, ///< Use the border color outside the texture.
    MirrorClampToEdge ///< Mirror once then clamp to the edge.
};

/// Border color for clamped sampling.
enum class SIRRenderBorderColor : uint8_t
{
    TransparentBlack, ///< Use transparent black outside the texture.
    OpaqueBlack, ///< Use opaque black outside the texture.
    OpaqueWhite, ///< Use opaque white outside the texture.
    CustomFloat ///< Use a custom floating-point border color.
};

/// Special sampler reduction behavior.
enum class SIRRenderReductionMode : uint8_t
{
    WeightedAverage, ///< Compute the weighted average of contributing texels.
    Minimum, ///< Return the minimum contributing sample.
    Maximum ///< Return the maximum contributing sample.
};

/// Kind of resource expected at a shader binding.
enum class SIRRenderBindingType : uint8_t
{
    UniformBuffer, ///< Bind a uniform or constant buffer range.
    StorageBuffer, ///< Bind shader-readable/writable buffer storage.
    SampledTexture, ///< Bind a texture for shader sampling.
    StorageTexture, ///< Bind a texture for shader storage access.
    CombinedTextureSampler, ///< Bind a sampled texture together with its sampler.
    Sampler, ///< Bind sampler state without a texture.
    TexelBuffer, ///< Bind a typed buffer view as texels.
    InputAttachment, ///< Bind an attachment from the active render pass as input.
    AccelerationStructure ///< Bind a ray-tracing acceleration structure.
};

/// Optional behavior of a shader binding declaration.
enum class SIRRenderBindingFlag : uint32_t
{
    DynamicOffset = 1U << 0, ///< Binding address can include a dynamic offset.
    VariableCount = 1U << 1, ///< Descriptor array count can vary at allocation.
    PartiallyBound = 1U << 2, ///< Some elements may remain unbound.
    UpdateAfterBind = 1U << 3, ///< Compatible descriptors can be updated after binding.
    ImmutableSampler = 1U << 4 ///< Sampler definition is fixed by the layout.
};

/// Graphics state that may change without rebuilding a pipeline.
enum class SIRRenderDynamicState : uint32_t
{
    Viewport = 1U << 0, ///< Viewport is supplied during command recording.
    Scissor = 1U << 1, ///< Scissor rectangle is supplied dynamically.
    BlendConstant = 1U << 2, ///< Blend constant is supplied dynamically.
    StencilReference = 1U << 3, ///< Stencil reference is supplied dynamically.
    DepthBias = 1U << 4, ///< Depth bias is supplied dynamically.
    DepthBounds = 1U << 5, ///< Depth-bounds values are supplied dynamically.
    LineWidth = 1U << 6, ///< Rasterized line width is supplied dynamically.
    CullMode = 1U << 7, ///< Cull mode is supplied dynamically.
    FrontFace = 1U << 8 ///< Front-face winding is supplied dynamically.
};

/// Pipeline stages used when describing synchronization.
enum class SIRRenderPipelineStage : uint64_t
{
    Top = 1ULL << 0, ///< Earliest logical point in a pipeline dependency.
    DrawIndirect = 1ULL << 1, ///< Reading indirect draw/dispatch arguments.
    VertexInput = 1ULL << 2, ///< Fetching vertex attributes and indices.
    VertexShader = 1ULL << 3, ///< Executing vertex shader instructions.
    Tessellation = 1ULL << 4, ///< Executing tessellation shader stages.
    GeometryShader = 1ULL << 5, ///< Executing geometry shader instructions.
    FragmentShader = 1ULL << 6, ///< Executing fragment shader instructions.
    EarlyDepthStencil = 1ULL << 7, ///< Performing early depth/stencil tests.
    LateDepthStencil = 1ULL << 8, ///< Performing late depth/stencil tests.
    ColorOutput = 1ULL << 9, ///< Writing or blending color outputs.
    ComputeShader = 1ULL << 10, ///< Executing compute shader instructions.
    Transfer = 1ULL << 11, ///< Copying or transferring resource data.
    RayTracing = 1ULL << 12, ///< Executing ray-tracing stages.
    AccelerationBuild = 1ULL << 13, ///< Building or updating acceleration structures.
    Host = 1ULL << 14, ///< Host-side memory accesses.
    Bottom = 1ULL << 15, ///< Final logical point in a pipeline dependency.
    AllGraphics = 1ULL << 16, ///< All supported graphics pipeline stages.
    AllCommands = 1ULL << 17 ///< All command pipeline stages relevant to the queue.
};

/// Memory read/write access masks for synchronization.
enum class SIRRenderAccess : uint64_t
{
    IndirectRead = 1ULL << 0, ///< Read indirect command arguments.
    IndexRead = 1ULL << 1, ///< Read index-buffer elements.
    VertexRead = 1ULL << 2, ///< Read vertex attributes.
    UniformRead = 1ULL << 3, ///< Read uniform/constant data.
    ShaderSampledRead = 1ULL << 4, ///< Read shader-sampled texture or texel data.
    ShaderStorageRead = 1ULL << 5, ///< Read shader storage memory.
    ShaderStorageWrite = 1ULL << 6, ///< Write shader storage memory.
    ColorAttachmentRead = 1ULL << 7, ///< Read color-attachment data.
    ColorAttachmentWrite = 1ULL << 8, ///< Write color-attachment data.
    DepthStencilRead = 1ULL << 9, ///< Read depth/stencil attachment data.
    DepthStencilWrite = 1ULL << 10, ///< Write depth/stencil attachment data.
    TransferRead = 1ULL << 11, ///< Read a resource during transfer.
    TransferWrite = 1ULL << 12, ///< Write a resource during transfer.
    HostRead = 1ULL << 13, ///< Read resource memory on the CPU.
    HostWrite = 1ULL << 14, ///< Write resource memory on the CPU.
    AccelerationRead = 1ULL << 15, ///< Read acceleration-structure memory.
    AccelerationWrite = 1ULL << 16, ///< Write acceleration-structure memory.
    MemoryRead = 1ULL << 17, ///< Generic device memory read.
    MemoryWrite = 1ULL << 18 ///< Generic device memory write.
};

/// Logical image layout required for an operation.
enum class SIRRenderTextureLayout : uint8_t
{
    Undefined, ///< Previous image contents may be undefined/discarded.
    General, ///< General-purpose texture access layout.
    ShaderRead, ///< Texture is prepared for shader reads.
    ColorAttachment, ///< Texture is prepared for color attachment access.
    DepthStencilRead, ///< Texture is prepared for depth/stencil reads.
    DepthStencilWrite, ///< Texture is prepared for depth/stencil writes.
    TransferSource, ///< Texture is prepared to supply transfer data.
    TransferDestination, ///< Texture is prepared to receive transfer data.
    Present ///< Texture is prepared for presentation.
};

/// Whether a barrier targets a buffer, image, memory, or alias.
enum class SIRRenderBarrierKind : uint8_t
{
    Buffer, ///< Synchronize access to a buffer range.
    Texture, ///< Synchronize access to texture subresources.
    GlobalMemory, ///< Synchronize accesses without naming one resource.
    Aliasing ///< Synchronize reuse of overlapping/aliased memory.
};

/// How a render-pass attachment is initialized.
enum class SIRRenderLoadOp : uint8_t
{
    Load, ///< Preserve existing attachment contents at pass start.
    Clear, ///< Clear attachment contents at pass start.
    Discard ///< Previous attachment contents need not be preserved.
};

/// Whether attachment contents are preserved after a pass.
enum class SIRRenderStoreOp : uint8_t
{
    Store, ///< Preserve attachment contents after the pass.
    Discard ///< Final attachment contents may be discarded.
};

/// How multisampled data is resolved.
enum class SIRRenderResolveMode : uint8_t
{
    None, ///< Do not resolve multisampled data.
    Average, ///< Average the selected samples.
    Minimum, ///< Choose the minimum sample value.
    Maximum, ///< Choose the maximum sample value.
    SampleZero ///< Use sample index zero.
};

/// Category of GPU query.
enum class SIRRenderQueryType : uint8_t
{
    Timestamp, ///< Record timestamps for GPU timing.
    Occlusion, ///< Count or test samples passing visibility.
    PipelineStatistics ///< Collect supported graphics pipeline statistics.
};

/// Operating-system surface integration.
enum class SIRRenderSurfacePlatform : uint8_t
{
    Headless, ///< No native windowed presentation surface.
    Win32, ///< Use a Win32 window target.
    X11, ///< Use an X11 window target.
    Wayland, ///< Use a Wayland surface target.
    Cocoa ///< Use a Cocoa window target.
};

/// Presentation timing/queuing policy.
enum class SIRRenderPresentMode : uint8_t
{
    FIFO, ///< Queue presentation and synchronize with display refresh.
    Immediate, ///< Present as soon as permitted, possibly tearing.
    Mailbox, ///< Keep the newest pending presentation in a mailbox queue.
    FIFORelaxed ///< Prefer FIFO but permit relaxed late-frame presentation.
};

/// Color encoding of the presentation surface.
enum class SIRRenderColorSpace : uint8_t
{
    SRGB, ///< Use the standard sRGB color space.
    LinearSRGB, ///< Use linear-light sRGB primaries.
    DisplayP3, ///< Use Display P3 primaries/color space.
    Rec2020PQ, ///< Use Rec.2020 primaries with PQ HDR transfer.
    Rec2020HLG, ///< Use Rec.2020 primaries with HLG HDR transfer.
    ScRGB ///< Use extended-range linear scRGB.
};

/// Fixed-function texture-combiner operation.
enum class SIRRenderFixedTextureOp : uint8_t
{
    Disable, ///< Disable this texture-combiner stage.
    Select, ///< Select one of the specified inputs.
    Modulate, ///< Multiply the selected inputs.
    Modulate2X, ///< Multiply selected inputs then scale by two.
    Modulate4X, ///< Multiply selected inputs then scale by four.
    Add, ///< Add the selected inputs.
    AddSigned, ///< Perform a signed-bias addition.
    Subtract, ///< Subtract one selected input from another.
    Interpolate, ///< Interpolate between inputs using a factor.
    Dot3 ///< Compute a three-component dot product.
};

/// Source operand for a fixed-function combiner.
enum class SIRRenderFixedTextureArg : uint8_t
{
    Texture, ///< Use the sampled texture color.
    Current, ///< Use the result from the preceding texture stage.
    Diffuse, ///< Use the incoming diffuse/primary color.
    Specular, ///< Use the incoming specular/secondary color.
    Factor, ///< Use the fixed texture-factor color.
    Constant ///< Use a stage-defined constant color.
};

/// Fixed-function fog falloff mode.
enum class SIRRenderFixedFogMode : uint8_t
{
    None, ///< Disable fixed-function fog.
    Linear, ///< Fog varies linearly with depth/distance.
    Exponential, ///< Fog follows an exponential attenuation curve.
    ExponentialSquared ///< Fog follows an exponential-squared attenuation curve.
};

/// Fixed-function shading interpolation mode.
enum class SIRRenderFixedShadeMode : uint8_t
{
    Flat, ///< Use one lighting color per primitive.
    Gouraud ///< Interpolate vertex lighting colors across primitives.
};

/// Source of fixed-function texture coordinates.
enum class SIRRenderFixedTexCoordSource : uint8_t
{
    VertexSet, ///< Read an explicitly supplied vertex texture-coordinate set.
    SphereMap, ///< Generate sphere-map coordinates.
    CameraNormal, ///< Generate coordinates from camera-space normals.
    CameraPosition, ///< Generate coordinates from camera-space positions.
    ReflectionVector ///< Generate reflection-vector-based coordinates.
};

/// Source of fixed-function material colors.
enum class SIRRenderFixedColorSource : uint8_t
{
    Material, ///< Use the fixed material's configured color.
    VertexColor ///< Use incoming vertex color data.
};

/// Top-level or bottom-level acceleration structure.
enum class SIRRenderAccelerationType : uint8_t
{
    BottomLevel, ///< Bottom-level structure containing geometric primitives.
    TopLevel ///< Top-level structure referencing instances of bottom-level structures.
};

/// Geometry representation in a ray-tracing build.
enum class SIRRenderRayGeometryType : uint8_t
{
    Triangles, ///< Use triangle primitives as ray geometry.
    AABBs, ///< Use axis-aligned bounding boxes for procedural ray geometry.
    Instances ///< Use instances of other acceleration structures.
};

/// Kind of logical work described by the frame request.
enum class SIRRenderWorkKind : uint8_t
{
    Draw, ///< Draw geometry through a graphics pipeline.
    Dispatch, ///< Dispatch compute workgroups.
    DispatchMesh, ///< Dispatch task/mesh shading work.
    TraceRays, ///< Trace rays through a ray-tracing pipeline.
    BufferCopy, ///< Copy data between buffers.
    TextureCopy, ///< Copy data between textures.
    BufferTextureCopy, ///< Copy data between buffers and textures.
    Resolve ///< Resolve multisampled texture data.
};

/// Graphics, compute, transfer, or ray-tracing pass type.
enum class SIRRenderPassKind : uint8_t
{
    Graphics, ///< Pass executes graphics drawing commands.
    Compute, ///< Pass executes compute workloads.
    Transfer, ///< Pass performs resource transfer operations.
    RayTracing ///< Pass executes ray-tracing operations.
};

/// Read/write intent of a frame resource reference.
enum class SIRRenderResourceIntent : uint8_t
{
    Read, ///< Pass reads resource contents.
    Write, ///< Pass writes resource contents.
    ReadWrite ///< Pass both reads and writes resource contents.
};

// Device identity prevents handles from one device accidentally validating on
// another. Device epochs change after recreation; slot generations change on
// release/reuse. A generation must never wrap while stale handles can exist.
/// Opaque resource identity with device, epoch, slot, and generation.
struct SIRRenderHandle
{
    uint64_t nDeviceId{};
    uint32_t nDeviceEpoch{};
    uint32_t nSlot{};
    uint32_t nGeneration{};
    SIRRenderHandleType eType{ SIRRenderHandleType::None };
};

/// Completion point for one logical device queue.
struct SIRRenderCompletion
{
    uint64_t nDeviceId{};
    uint64_t nValue{};
    uint32_t nDeviceEpoch{};
    SIRRenderQueueType eQueue{ SIRRenderQueueType::Graphics };
};

/// Completion values covering several queues on one device.
struct SIRRenderCompletionSet
{
    uint64_t nDeviceId{};
    uint32_t nDeviceEpoch{};
    uint32_t nQueueMask{};
    uint64_t szValues[static_cast<uint32_t>( SIRRenderQueueType::Count )]{};
};

/// Major/minor/patch version numbers.
struct SIRRenderVersion
{
    uint32_t nMajor{};
    uint32_t nMinor{};
    uint32_t nPatch{};
};

/// Properties of an enumerated graphics adapter.
struct SIRRenderAdapterInfo
{
    uint32_t nIndex{};
    uint32_t nVendorId{};
    uint32_t nDeviceId{};
    uint32_t nDriverId{};
    uint64_t nAdapterLuid{};
    uint64_t nDedicatedBytes{};
    uint64_t nSharedBytes{};
    SIRRenderVersion apiVersion{};
    SIRRenderBackend eBackend{ SIRRenderBackend::Auto };
    SIRRenderBackendVariant eVariant{ SIRRenderBackendVariant::Default };
    bool bSoftware{};
    char szName[128]{};
};

// All persistent CPU-side backend bookkeeping is allocated through these
// engine-supplied hooks when non-null. Both callbacks must be supplied or both
// must be null. They must support the requested alignment and never throw.
using SIRRenderHostAllocateFn = void *( * )( void *pUser, uint64_t nSizeBytes, uint64_t nAlignmentBytes ) noexcept;
using SIRRenderHostFreeFn = void ( * )( void *pUser, void *pMemory ) noexcept;]
/// Packed min/mag/mip filter settings.
struct SIRRenderTextureFilter
{
    uint8_t nPacked;
    constexpr SIRRenderTextureFilter( SIRRenderFilter eMin, SIRRenderFilter eMag, SIRRenderMipFilter eMip ) noexcept
        : nPacked( static_cast<uint8_t>( eMin ) | ( static_cast<uint8_t>( eMag ) << 2 ) | ( static_cast<uint8_t>( eMip ) << 4 ) )
    {
    }
    SIR_FORCEINLINE constexpr SIRRenderFilter getMin( void ) const noexcept
    {
        return static_cast<SIRRenderFilter>( nPacked & 0x3 );
    }
    SIR_FORCEINLINE constexpr SIRRenderFilter getMag( void ) const noexcept
    {
        return static_cast<SIRRenderFilter>( ( nPacked >> 2 ) & 0x3 );
    }
    SIR_FORCEINLINE constexpr SIRRenderMipFilter getMip( void ) const noexcept
    {
        return static_cast<SIRRenderMipFilter>( ( nPacked >> 4 ) & 0x3 );
    }
};

/// Optional engine callback hooks for backend CPU allocations.
struct SIRRenderHostAllocator
{
    void *pUser{};
    SIRRenderHostAllocateFn pAllocate{};
    SIRRenderHostFreeFn pFree{};
};

/// Options and budgets used when creating a rendering device.
struct SIRRenderDeviceCreateDesc
{
    uint32_t nAdapterIndex{};
    uint32_t nFramesInFlight{ 2 };
    uint32_t nMinimumGLMajor{};
    uint32_t nMinimumGLMinor{};
    uint64_t nRequiredFeatureMask{};
    uint64_t nPreferredFeatureMask{};
    uint64_t nResidentBudgetBytes{};
    uint64_t nUploadBudgetBytes{};
    uint64_t nReadbackBudgetBytes{};
    uint32_t nRequestedEraMask{};* // Zero selects every supported pipeline family.*
    SIRRenderGLProfile eGLProfile{ SIRRenderGLProfile::NotApplicable };
    SIRRenderHostAllocator hostAllocator{};
    bool bHeadless{};
    bool bAllowSoftware{};
    bool bValidation{};
};

/// Numeric hardware and API limits.
struct SIRRenderLimits
{
    uint32_t nMaxTexture2D{};
    uint32_t nMaxTexture3D{};
    uint32_t nMaxTextureLayers{};
    uint32_t nMaxSamplers{};
    uint32_t nMaxTexturesPerStage{};
    uint32_t nMaxTextureStages{};
    uint32_t nMaxUniformRegistersPerStage{};
    uint32_t nMaxVertexShaderInstructions{};
    uint32_t nMaxFragmentShaderInstructions{};
    uint32_t nMaxVertexAttributes{};
    uint32_t nMaxVertexStreams{};
    uint32_t nMaxColorAttachments{};
    uint32_t nMaxViewports{};
    uint32_t nMaxUniformBufferBindings{};
    uint32_t nMaxStorageBufferBindings{};
    uint32_t nMaxBindingSets{};
    uint32_t nMaxBindingsPerSet{};
    uint32_t nMaxPushConstantBytes{};
    uint32_t nMaxUniformBufferRange{};
    uint32_t nMaxComputeGroups[3]{};
    uint32_t nMaxComputeWorkgroup[3]{};
    uint32_t nMaxComputeSharedBytes{};
    uint32_t nMaxMSAASamples{};
    uint32_t nMaxFixedLights{};
    uint32_t nMaxFramesInFlight{};
    uint32_t nMaxRayRecursionDepth{};
    uint32_t nMaxMeshOutputVertices{};
    uint32_t nMaxMeshOutputPrimitives{};
    uint32_t nUniformOffsetAlignment{};
    uint32_t nStorageOffsetAlignment{};
    uint32_t nNonCoherentAtomSize{};
};

/// Supported features, queues, limits, and backend identity.
struct SIRRenderDeviceCapabilities
{
    uint64_t nDeviceId{};
    uint64_t nFeatureMask{};
    uint32_t nDeviceEpoch{};
    uint32_t nSupportedEraMask{};
    uint32_t nShaderStageMask{};
    uint32_t nLogicalQueueMask{};
    uint32_t nNativeQueueMask{};
    SIRRenderBackend eBackend{ SIRRenderBackend::Auto };
    SIRRenderBackendVariant eVariant{ SIRRenderBackendVariant::Default };
    SIRRenderExecutionModel eExecution{ SIRRenderExecutionModel::StatefulDevice };
    SIRRenderGLProfile eGLProfile{ SIRRenderGLProfile::NotApplicable };
    SIRRenderVersion apiVersion{};
    SIRRenderVersion driverVersion{};
    SIRRenderLimits limits{};
    bool bUnifiedMemory{};
};

/// Shader target limits for a code format and stage.
struct SIRRenderShaderTargetCapabilities
{
    SIRRenderShaderFormat eFormat{ SIRRenderShaderFormat::SPIRV };
    SIRRenderShaderStage eStage{ SIRRenderShaderStage::Vertex };
    uint32_t nMaxShaderModelMajor{};
    uint32_t nMaxShaderModelMinor{};
    uint32_t nMaxConstantRegisters{};
    uint32_t nMaxInstructionSlots{};
    bool bDynamicBranching{};
    bool bDerivatives{};
};

/// Supported operations, sample counts, and block geometry for a format.
struct SIRRenderFormatSupport
{
    SIRRenderFormat eFormat{ SIRRenderFormat::Unknown };
    uint32_t nFeatureMask{};
    uint32_t nSampleCountMask{};
    uint32_t nBlockWidth{ 1 };
    uint32_t nBlockHeight{ 1 };
    uint32_t nBytesPerBlock{};
};

/// Size, alignment, and placement constraints of a resource.
struct SIRRenderMemoryRequirements
{
    uint64_t nSizeBytes{};
    uint64_t nAlignmentBytes{};
    uint32_t nCompatibleHeapMask{};
    bool bRequiresDedicated{};
    bool bPrefersDedicated{};
};

/// Tile shape and mip-tail requirements for sparse textures.
struct SIRRenderSparseTextureRequirements
{
    uint64_t nTileSizeBytes{};
    uint32_t nTileWidth{};
    uint32_t nTileHeight{};
    uint32_t nTileDepth{};
    uint32_t nFirstMipTail{};
    uint32_t nMipTailAlignmentBytes{};
};

/// Tracked GPU memory budget and residency measurements.
struct SIRRenderMemoryBudget
{
    uint64_t nBudgetBytes{};
    uint64_t nCommittedBytes{};
    uint64_t nResidentBytes{};
    uint64_t nPeakCommittedBytes{};
    uint64_t nMappedUploadBytes{};
    uint64_t nMappedReadbackBytes{};
};

/// Description of a device-managed memory heap.
struct SIRRenderMemoryHeapDesc
{
    uint64_t nSizeBytes{};
    uint64_t nAlignmentBytes{};
    uint32_t nRequiredProperties{};
    SIRRenderMemoryClass eMemory{ SIRRenderMemoryClass::DeviceLocal };
    SIRFixedString<64> szDebugName{};
};

/// Selected heap and byte offset for placed resources.
struct SIRRenderHeapPlacement
{
    SIRRenderHandle hHeap{};
    uint64_t nOffsetBytes{};
};

/// Offset and length of one buffer region in bytes.
struct SIRRenderBufferRange
{
    uint64_t nOffsetBytes{};
    uint64_t nSizeBytes{};
};

/// Mip, layer, and aspect selection for image subresources.
struct SIRRenderSubresourceRange
{
    uint32_t nBaseMip{};
    uint32_t nMipCount{ 1 };
    uint32_t nBaseLayer{};
    uint32_t nLayerCount{ 1 };
    uint32_t nAspectMask{};
};

/// Buffer capacity, usages, allocation, and placement options.
struct SIRRenderBufferDesc
{
    uint64_t nSizeBytes{};
    SIRRenderBufferUsage eUsageMask{};
    uint64_t nRequestedAlignment{};
    uint32_t nStrideBytes{};
    uint32_t nResourceFlags{};
    uint32_t nRequiredMemoryProperties{};
    SIRRenderMemoryClass eMemory{ SIRRenderMemoryClass::DeviceLocal };
    SIRRenderHeapPlacement placement{};
    SIRFixedString<64> szDebugName{};
};

/// Texture dimensions, format, usages, and allocation options.
struct SIRRenderTextureDesc
{
    uint32_t nWidth{};
    uint32_t nHeight{};
    uint32_t nDepth{ 1 };
    uint32_t nArrayLayers{ 1 };
    uint32_t nMipCount{ 1 };
    uint32_t nSampleCount{ 1 };
    uint64_t nUsageMask{};
    uint32_t nResourceFlags{};
    uint32_t nRequiredMemoryProperties{};
    SIRRenderTextureDimension eDimension{ SIRRenderTextureDimension::Texture2D };
    SIRRenderFormat eFormat{ SIRRenderFormat::Unknown };
    SIRRenderMemoryClass eMemory{ SIRRenderMemoryClass::DeviceLocal };
    SIRRenderHeapPlacement placement{};
    SIRFixedString<64> szDebugName{};
};

/// Logical view into selected texture subresources.
struct SIRRenderTextureViewDesc
{
    SIRRenderHandle hTexture{};
    SIRRenderSubresourceRange subresources{};
    SIRRenderTextureDimension eDimension{ SIRRenderTextureDimension::Texture2D };
    SIRRenderFormat eFormat{ SIRRenderFormat::Unknown };
    uint64_t nUsageMask{};
};

/// Logical view into a byte range of a buffer.
struct SIRRenderBufferViewDesc
{
    SIRRenderHandle hBuffer{};
    SIRRenderBufferRange range{};
    SIRRenderFormat eFormat{ SIRRenderFormat::Unknown };
    uint32_t nStrideBytes{};
    uint64_t nUsageMask{};
};

/// Filtering, addressing, comparison, and LOD sampling settings.
struct SIRRenderSamplerDesc
{
    SIRRenderTextureFilter eFilter{ SIRRenderFilter::Linear, SIRRenderFilter::Linear, SIRRenderMipFilter::Linear};
    SIRRenderAddressMode eAddressU{ SIRRenderAddressMode::Repeat };
    SIRRenderAddressMode eAddressV{ SIRRenderAddressMode::Repeat };
    SIRRenderAddressMode eAddressW{ SIRRenderAddressMode::Repeat };
    SIRRenderCompareOp eCompare{ SIRRenderCompareOp::Always };
    SIRRenderBorderColor eBorderColor{ SIRRenderBorderColor::TransparentBlack };
    SIRRenderReductionMode eReduction{ SIRRenderReductionMode::WeightedAverage };
    float fMaxAnisotropy{ 1.0F };
    float fMinLod{};
    float fMaxLod{ 1000.0F };
    float fLodBias{};
    float szCustomBorder[4]{};
    bool bCompareEnabled{};
};

/// Mapped CPU pointer and valid mapped byte range.
struct SIRRenderMappedBuffer
{
    void *pMapped{};
    uint64_t nOffsetBytes{};
    uint64_t nSizeBytes{};
    uint64_t nMapToken{};
};

/// Buffer region and CPU mapping flags requested by the caller.
struct SIRRenderMapDesc
{
    SIRRenderHandle hBuffer{};
    SIRRenderBufferRange range{};
    uint32_t nFlags{};
};

/// Residency of one sparse resource tile.
struct SIRRenderSparsePage
{
    SIRRenderHandle hResource{};
    SIRRenderHandle hHeap{};
    uint64_t nHeapOffsetBytes{};
    uint32_t nMip{};
    uint32_t nLayer{};
    uint32_t nTileX{};
    uint32_t nTileY{};
    uint32_t nTileZ{};
    bool bResident{};
};

/// Batch of sparse-page residency changes.
struct SIRRenderSparseBindingDesc
{
    SIRSpan<SIRRenderSparsePage> pages{};
};

// Shader objects and pipeline binding layouts. Shader source is offline-cooked;
// the RHI accepts native-target code and reflection, not a universal HLSL/GLSL
// cross-compiler. Reflection and binding-layout hashes must describe that code.
/// Device-target shader bytes and associated metadata.
struct SIRRenderShaderCode
{
    SIRSpan<uint8_t> bytes{};
    SIRFixedString<64> szEntryPoint{};
    uint64_t nContentHash{};
    uint64_t nReflectionHash{};
    SIRRenderShaderFormat eFormat{ SIRRenderShaderFormat::SPIRV };
    SIRRenderShaderStage eStage{ SIRRenderShaderStage::Vertex };
    uint8_t nShaderModelMajor{};
    uint8_t nShaderModelMinor{};
};

/// Specialization constant ID and value bytes.
struct SIRRenderShaderSpecialization
{
    uint32_t nConstantId{};
    SIRSpan<uint8_t> value{};
};

/// Shader handle plus optional specialization constants.
struct SIRRenderShaderReference
{
    SIRRenderHandle hShader{};
    SIRSpan<SIRRenderShaderSpecialization> specializations{};
};

/// A declared binding in a pipeline layout.
struct SIRRenderBindingLayoutEntry
{
    uint32_t nSet{};
    uint32_t nBinding{};
    uint32_t nArrayCount{ 1 };
    uint32_t nShaderStageMask{};
    uint32_t nFlags{};
    SIRRenderBindingType eType{ SIRRenderBindingType::UniformBuffer };
    SIRRenderHandle hImmutableSampler{};
};

/// Byte interval and shader stages for push constants.
struct SIRRenderPushConstantRange
{
    uint32_t nOffsetBytes{};
    uint32_t nSizeBytes{};
    uint32_t nStageMask{};
};

/// Set bindings and constant ranges accepted by a pipeline.
struct SIRRenderPipelineLayoutDesc
{
    SIRSpan<SIRRenderBindingLayoutEntry> bindings{};
    SIRSpan<SIRRenderPushConstantRange> pushConstants{};
    SIRFixedString<64> szDebugName{};
};

/// One resource/sampler assignment to a shader binding.
struct SIRRenderDescriptorWrite
{
    SIRRenderHandle hResource{};
    SIRRenderHandle hSampler{};
    SIRRenderBufferRange bufferRange{};
    uint32_t nBinding{};
    uint32_t nArrayIndex{};
    SIRRenderBindingType eType{ SIRRenderBindingType::UniformBuffer };
};

/// Creation parameters for a binding set.
struct SIRRenderBindingSetDesc
{
    SIRRenderHandle hPipelineLayout{};
    uint32_t nSet{};
    uint32_t nVariableDescriptorCount{};
    SIRFixedString<64> szDebugName{};
};

/// Batch of binding-set descriptor writes.
struct SIRRenderBindingSetUpdate
{
    SIRRenderHandle hSet{};
    SIRSpan<SIRRenderDescriptorWrite> writes{};
};

/// Binding sets and dynamic offsets selected for a draw.
struct SIRRenderBindingSetBind
{
    uint32_t nFirstSet{};
    SIRSpan<SIRRenderHandle> sets{};
    SIRSpan<uint32_t> dynamicOffsets{};
};

/// Vertex input stream binding, stride, and input rate.
struct SIRRenderVertexBinding
{
    uint32_t nBinding{};
    uint32_t nStrideBytes{};
    uint32_t nInstanceDivisor{ 1 };
    SIRRenderVertexRate eRate{ SIRRenderVertexRate::PerVertex };
};

/// One attribute's location, format, and byte offset.
struct SIRRenderVertexAttribute
{
    uint32_t nLocation{};
    uint32_t nBinding{};
    uint32_t nOffsetBytes{};
    SIRRenderFormat eFormat{ SIRRenderFormat::Unknown };
};

/// Vertex streams and attributes expected by a pipeline.
struct SIRRenderVertexLayout
{
    SIRSpan<SIRRenderVertexBinding> bindings{};
    SIRSpan<SIRRenderVertexAttribute> attributes{};
};

/// Stencil operations and masks for one polygon face.
struct SIRRenderStencilFaceState
{
    SIRRenderStencilOp eFail{ SIRRenderStencilOp::Keep };
    SIRRenderStencilOp eDepthFail{ SIRRenderStencilOp::Keep };
    SIRRenderStencilOp ePass{ SIRRenderStencilOp::Keep };
    SIRRenderCompareOp eCompare{ SIRRenderCompareOp::Always };
    uint32_t nCompareMask{ 0xFFFFFFFFU };
    uint32_t nWriteMask{ 0xFFFFFFFFU };
    uint32_t nReference{};
};

/// Depth/stencil testing and write configuration.
struct SIRRenderDepthStencilState
{
    SIRRenderStencilFaceState front{};
    SIRRenderStencilFaceState back{};
    SIRRenderCompareOp eDepthCompare{ SIRRenderCompareOp::GreaterOrEqual };
    float fMinDepthBounds{};
    float fMaxDepthBounds{ 1.0F };
    bool bDepthTest{ true };
    bool bDepthWrite{ true };
    bool bStencilTest{};
    bool bDepthBounds{};
};

/// Culling, polygon mode, depth bias, and scissor configuration.
struct SIRRenderRasterState
{
    SIRRenderCullMode eCull{ SIRRenderCullMode::Back };
    SIRRenderFrontFace eFrontFace{ SIRRenderFrontFace::CounterClockwise };
    SIRRenderPolygonMode ePolygon{ SIRRenderPolygonMode::Fill };
    float fDepthBiasConstant{};
    float fDepthBiasSlope{};
    float fDepthBiasClamp{};
    float fLineWidth{ 1.0F };
    bool bDepthClamp{};
    bool bScissor{};
    bool bConservative{};
};

/// Blend factors and write mask for one color attachment.
struct SIRRenderBlendAttachmentState
{
    SIRRenderBlendFactor eSourceColor{ SIRRenderBlendFactor::One };
    SIRRenderBlendFactor eDestinationColor{ SIRRenderBlendFactor::Zero };
    SIRRenderBlendFactor eSourceAlpha{ SIRRenderBlendFactor::One };
    SIRRenderBlendFactor eDestinationAlpha{ SIRRenderBlendFactor::Zero };
    SIRRenderBlendOp eColorOp{ SIRRenderBlendOp::Add };
    SIRRenderBlendOp eAlphaOp{ SIRRenderBlendOp::Add };
    uint8_t nWriteMask{ 0xFU };
    bool bEnabled{};
};

/// Blending configuration across color attachments.
struct SIRRenderBlendState
{
    SIRSpan<SIRRenderBlendAttachmentState> attachments{};
    float szConstants[4]{};
    uint32_t nSampleMask{ 0xFFFFFFFFU };
    SIRRenderLogicOp eLogicOp{ SIRRenderLogicOp::Copy };
    bool bLogicOp{};
    bool bAlphaToCoverage{};
    bool bAlphaToOne{};
};

/// Multisample settings for rasterization.
struct SIRRenderMultisampleState
{
    uint32_t nSampleCount{ 1 };
    float fMinSampleShading{};
    bool bSampleShading{};
};

/// Combined graphics raster, depth, blend, and topology state.
struct SIRRenderGraphicsState
{
    SIRRenderRasterState raster{};
    SIRRenderDepthStencilState depthStencil{};
    SIRRenderBlendState blend{};
    SIRRenderMultisampleState multisample{};
    SIRRenderPrimitiveTopology eTopology{ SIRRenderPrimitiveTopology::TriangleList };
    uint32_t nPatchControlPoints{};
    uint32_t nDynamicStateMask{};
};

/// Formats expected at the outputs of a graphics pipeline.
struct SIRRenderTargetLayout
{
    SIRSpan<SIRRenderFormat> colorFormats{};
    SIRRenderFormat eDepthFormat{ SIRRenderFormat::Unknown };
    SIRRenderFormat eStencilFormat{ SIRRenderFormat::Unknown };
    uint32_t nViewMask{};
};

// Advanced and programmable devices can consume the same logical graphics
// state but compile it through different pipeline-family policies.
/// Advanced-era graphics pipeline creation parameters.
struct SIRRenderAdvancedGraphicsPipelineDesc
{
    SIRRenderHandle hLayout{};
    SIRSpan<SIRRenderShaderReference> shaders{};
    SIRRenderVertexLayout vertexLayout{};
    SIRRenderGraphicsState state{};
    SIRRenderTargetLayout targets{};
};

/// Programmable-era graphics pipeline creation parameters.
struct SIRRenderProgrammablePipelineDesc
{
    SIRRenderHandle hLayout{};
    SIRSpan<SIRRenderShaderReference> shaders{};
    SIRRenderVertexLayout vertexLayout{};
    SIRRenderGraphicsState state{};
    SIRRenderTargetLayout targets{};
};

/// RGBA color used by legacy fixed-function state.
struct SIRRenderFixedColor
{
    float szRGBA[4]{ 1.0F, 1.0F, 1.0F, 1.0F };
};

/// Fixed-function material lighting properties.
struct SIRRenderFixedMaterial
{
    SIRRenderFixedColor ambient{};
    SIRRenderFixedColor diffuse{};
    SIRRenderFixedColor specular{};
    SIRRenderFixedColor emissive{};
    float fShininess{};
    SIRRenderFixedColorSource eAmbientSource{ SIRRenderFixedColorSource::Material };
    SIRRenderFixedColorSource eDiffuseSource{ SIRRenderFixedColorSource::Material };
    SIRRenderFixedColorSource eSpecularSource{ SIRRenderFixedColorSource::Material };
    bool bTwoSidedLighting{};
    bool bNormalizeNormals{};
};

/// Position, attenuation, and color of a fixed-function light.
struct SIRRenderFixedLight
{
    float szPosition[4]{};
    float szDirection[3]{};
    float fRange{};
    float fConstantAttenuation{ 1.0F };
    float fLinearAttenuation{};
    float fQuadraticAttenuation{};
    float fSpotInnerCosine{ 1.0F };
    float fSpotOuterCosine{ 1.0F };
    SIRRenderFixedColor ambient{};
    SIRRenderFixedColor diffuse{};
    SIRRenderFixedColor specular{};
    bool bEnabled{ true };
};

/// Fog color, range, density, and falloff.
struct SIRRenderFixedFog
{
    SIRRenderFixedColor color{};
    SIRRenderFixedFogMode eMode{ SIRRenderFixedFogMode::None };
    float fStart{};
    float fEnd{ 1.0F };
    float fDensity{ 1.0F };
};

// Matrix convention is column-major for the logical frame. The GL/D3D
// translator converts to the target API convention before issuing commands.
/// World, view, and projection matrices for fixed-function drawing.
struct SIRRenderFixedTransforms
{
    float szWorld[16]{ 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F };
    float szView[16]{ 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F };
    float szProjection[16]{ 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F };
};

/// Dynamic fixed-function transforms, material, fog, and lights.
struct SIRRenderFixedDynamicState
{
    SIRRenderFixedTransforms transforms{};
    SIRRenderFixedMaterial material{};
    SIRRenderFixedFog fog{};
    SIRSpan<SIRRenderFixedLight> lights{};
    SIRRenderFixedColor textureFactor{};
    float fAlphaReference{};
};

/// One legacy texture-combiner stage and coordinate transform.
struct SIRRenderFixedTextureStage
{
    SIRRenderFixedTextureOp eColorOp{ SIRRenderFixedTextureOp::Modulate };
    SIRRenderFixedTextureOp eAlphaOp{ SIRRenderFixedTextureOp::Modulate };
    SIRRenderFixedTextureArg eColorA{ SIRRenderFixedTextureArg::Texture };
    SIRRenderFixedTextureArg eColorB{ SIRRenderFixedTextureArg::Current };
    SIRRenderFixedTextureArg eColorC{ SIRRenderFixedTextureArg::Factor };
    SIRRenderFixedTextureArg eAlphaA{ SIRRenderFixedTextureArg::Texture };
    SIRRenderFixedTextureArg eAlphaB{ SIRRenderFixedTextureArg::Current };
    SIRRenderFixedTextureArg eAlphaC{ SIRRenderFixedTextureArg::Factor };
    SIRRenderFixedTexCoordSource eTexCoords{ SIRRenderFixedTexCoordSource::VertexSet };
    SIRRenderFixedColor constant{};
    float szTextureMatrix[16]{ 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F };
    uint32_t nTexCoordSet{};
    uint64_t nTextureSemantic{};
    bool bTransformTextureCoords{};
};

/// Complete fixed-function pipeline configuration.
struct SIRRenderFixedPipelineDesc
{
    SIRSpan<SIRRenderFixedTextureStage> textureStages{};
    SIRRenderVertexLayout vertexLayout{};
    SIRRenderGraphicsState state{};
    SIRRenderTargetLayout targets{};
    SIRRenderFixedShadeMode eShading{ SIRRenderFixedShadeMode::Gouraud };
    SIRRenderCompareOp eAlphaCompare{ SIRRenderCompareOp::Always };
    bool bLightingEnabled{ true };
    bool bAlphaTest{};
    bool bVertexColor{};
};

/// Compute shader and layout used for a compute pipeline.
struct SIRRenderComputePipelineDesc
{
    SIRRenderHandle hLayout{};
    SIRRenderShaderReference shader{};
};

/// Mesh shader stages and graphics state.
struct SIRRenderMeshPipelineDesc
{
    SIRRenderHandle hLayout{};
    SIRSpan<SIRRenderShaderReference> shaders{};
    SIRRenderGraphicsState state{};
    SIRRenderTargetLayout targets{};
};

/// Composition of shaders in one ray-tracing hit/group record.
struct SIRRenderRayTracingGroup
{
    uint32_t nGeneralShader{ 0xFFFFFFFFU };
    uint32_t nClosestHit{ 0xFFFFFFFFU };
    uint32_t nAnyHit{ 0xFFFFFFFFU };
    uint32_t nIntersection{ 0xFFFFFFFFU };
    bool bProcedural{};
};

/// Shader groups, recursion, and layout of a ray-tracing pipeline.
struct SIRRenderRayTracingPipelineDesc
{
    SIRRenderHandle hLayout{};
    SIRSpan<SIRRenderShaderReference> shaders{};
    SIRSpan<SIRRenderRayTracingGroup> groups{};
    uint32_t nMaxRecursionDepth{ 1 };
    uint32_t nMaxPayloadBytes{};
    uint32_t nMaxAttributeBytes{};
};

// Exactly one descriptor pointer must be populated and must match eType/eEra.
// Graphics: Advanced -> pAdvanced, Programmable -> pProgrammable.
// FixedFunction: pFixed. Compute: Advanced or Programmable if supported.
// Mesh and RayTracing: Advanced only, subject to actual features.
/// Tagged selection of a pipeline-era-specific creation descriptor.
struct SIRRenderPipelineDesc
{
    SIRPipelineEra eEra{ SIRPipelineEra::Advanced };
    SIRRenderPipelineType eType{ SIRRenderPipelineType::Graphics };
    const SIRRenderAdvancedGraphicsPipelineDesc *pAdvanced{};
    const SIRRenderProgrammablePipelineDesc *pProgrammable{};
    const SIRRenderFixedPipelineDesc *pFixed{};
    const SIRRenderComputePipelineDesc *pCompute{};
    const SIRRenderMeshPipelineDesc *pMesh{};
    const SIRRenderRayTracingPipelineDesc *pRayTracing{};
    uint64_t nCacheKey{};
    SIRRenderHandle hPipelineCache{};
    SIRFixedString<64> szDebugName{};
    bool bAllowAsyncCompilation{};
};

/// Asynchronous pipeline construction state.
enum class SIRRenderPipelineStatus : uint8_t
{
    Pending, ///< Pipeline compilation or preparation is ongoing.
    Ready, ///< Pipeline is ready for use.
    Failed ///< Pipeline creation or compilation failed.
};

/// Initial bytes and label for a reusable pipeline cache.
struct SIRRenderPipelineCacheDesc
{
    SIRSpan<uint8_t> initialBytes{};
    SIRFixedString<64> szDebugName{};
};

// Synchronization is stage + access + texture layout, not one overloaded
// 'resource state'. This can lower to Vulkan sync2, D3D12 enhanced barriers,
// GL memory barriers, or ordered legacy commands as capabilities allow.
/// Execution stage, memory access, and image layout at one endpoint.
struct SIRRenderSyncScope
{
    uint64_t nStageMask{};
    uint64_t nAccessMask{};
    SIRRenderTextureLayout eLayout{ SIRRenderTextureLayout::General };
};

/// Resource dependency, optional aliasing, and queue transfer.
struct SIRRenderBarrier
{
    SIRRenderHandle hResource{};
    SIRRenderHandle hAliasPrevious{};
    SIRRenderBufferRange bufferRange{};
    SIRRenderSubresourceRange subresources{};
    SIRRenderSyncScope before{};
    SIRRenderSyncScope after{};
    SIRRenderBarrierKind eKind{ SIRRenderBarrierKind::Buffer };
    SIRRenderQueueType eSourceQueue{ SIRRenderQueueType::Graphics };
    SIRRenderQueueType eDestinationQueue{ SIRRenderQueueType::Graphics };
    bool bDiscardContents{};
};

/// Collection of resource barriers recorded together.
struct SIRRenderBarrierBatch
{
    SIRSpan<SIRRenderBarrier> barriers{};
};

/// Viewport origin, size, and depth interval.
struct SIRRenderViewport
{
    float fX{};
    float fY{};
    float fWidth{};
    float fHeight{};
    float fMinDepth{};
    float fMaxDepth{ 1.0F };
};

/// Integer render/scissor rectangle.
struct SIRRenderRect
{
    int32_t nX{};
    int32_t nY{};
    uint32_t nWidth{};
    uint32_t nHeight{};
};

/// Color attachment view, load/store actions, and optional resolve.
struct SIRRenderColorAttachment
{
    SIRRenderHandle hView{};
    SIRRenderHandle hResolveView{};
    SIRRenderLoadOp eLoad{ SIRRenderLoadOp::Load };
    SIRRenderStoreOp eStore{ SIRRenderStoreOp::Store };
    SIRRenderResolveMode eResolve{ SIRRenderResolveMode::None };
    float szClear[4]{};
};

/// Depth/stencil attachment load/store/resolve actions.
struct SIRRenderDepthStencilAttachment
{
    SIRRenderHandle hView{};
    SIRRenderHandle hResolveView{};
    SIRRenderLoadOp eDepthLoad{ SIRRenderLoadOp::Load };
    SIRRenderStoreOp eDepthStore{ SIRRenderStoreOp::Store };
    SIRRenderLoadOp eStencilLoad{ SIRRenderLoadOp::Load };
    SIRRenderStoreOp eStencilStore{ SIRRenderStoreOp::Store };
    SIRRenderResolveMode eDepthResolve{ SIRRenderResolveMode::None };
    SIRRenderResolveMode eStencilResolve{ SIRRenderResolveMode::None };
    float fClearDepth{};
    uint32_t nClearStencil{};
};

/// Attachments and render area for one graphics pass.
struct SIRRenderRenderPassDesc
{
    SIRSpan<SIRRenderColorAttachment> colors{};
    SIRRenderDepthStencilAttachment depthStencil{};
    SIRRenderRect renderArea{};
    uint32_t nViewMask{};
};

/// Texture views and extent making up a framebuffer.
struct SIRRenderFramebufferDesc
{
    SIRSpan<SIRRenderHandle> colorViews{};
    SIRRenderHandle hDepthStencilView{};
    uint32_t nWidth{};
    uint32_t nHeight{};
    uint32_t nLayers{ 1 };
    uint32_t nSampleCount{ 1 };
};

/// Vertex buffer, stream slot, stride, and offset to bind.
struct SIRRenderVertexBufferBind
{
    SIRRenderHandle hBuffer{};
    uint64_t nOffsetBytes{};
    uint32_t nBinding{};
    uint32_t nStrideBytes{};
};

/// Index buffer, element format, and offset to bind.
struct SIRRenderIndexBufferBind
{
    SIRRenderHandle hBuffer{};
    uint64_t nOffsetBytes{};
    SIRRenderIndexType eIndexType{ SIRRenderIndexType::UInt16 };
};

/// Vertex count, instance count, and start indices for a draw.
struct SIRRenderDrawArguments
{
    uint32_t nVertexCount{};
    uint32_t nInstanceCount{ 1 };
    uint32_t nFirstVertex{};
    uint32_t nFirstInstance{};
};

/// Index count, instance count, and vertex offset for a draw.
struct SIRRenderDrawIndexedArguments
{
    uint32_t nIndexCount{};
    uint32_t nInstanceCount{ 1 };
    uint32_t nFirstIndex{};
    int32_t nVertexOffset{};
    uint32_t nFirstInstance{};
};

/// Indirect arguments buffer and optional GPU-generated draw count.
struct SIRRenderIndirectArguments
{
    SIRRenderHandle hArguments{};
    SIRRenderHandle hCountBuffer{};
    uint64_t nArgumentOffsetBytes{};
    uint64_t nCountOffsetBytes{};
    uint32_t nMaxCommandCount{ 1 };
    uint32_t nStrideBytes{};
    bool bIndexed{};
};

/// Direct compute dispatch group dimensions.
struct SIRRenderDispatchArguments
{
    uint32_t nGroupX{ 1 };
    uint32_t nGroupY{ 1 };
    uint32_t nGroupZ{ 1 };
};

/// Buffer and offset holding indirect dispatch dimensions.
struct SIRRenderDispatchIndirectArguments
{
    SIRRenderHandle hArguments{};
    uint64_t nOffsetBytes{};
};

/// Source/destination offsets and byte length for a copy.
struct SIRRenderBufferCopy
{
    SIRRenderHandle hSource{};
    SIRRenderHandle hDestination{};
    uint64_t nSourceOffsetBytes{};
    uint64_t nDestinationOffsetBytes{};
    uint64_t nSizeBytes{};
};

/// Mip, layer, aspect, origin, and extent of a texture region.
struct SIRRenderTextureRegion
{
    uint32_t nMip{};
    uint32_t nLayer{};
    uint32_t nX{};
    uint32_t nY{};
    uint32_t nZ{};
    uint32_t nWidth{};
    uint32_t nHeight{};
    uint32_t nDepth{ 1 };
    uint32_t nAspectMask{};
};

/// Source and destination regions for an image copy.
struct SIRRenderTextureCopy
{
    SIRRenderHandle hSource{};
    SIRRenderHandle hDestination{};
    SIRRenderTextureRegion source{};
    SIRRenderTextureRegion destination{};
};

/// Buffer/image copy region, pitch, and transfer direction.
struct SIRRenderBufferTextureCopy
{
    SIRRenderHandle hBuffer{};
    SIRRenderHandle hTexture{};
    SIRRenderTextureRegion region{};
    uint64_t nBufferOffsetBytes{};
    uint64_t nRowPitchBytes{};
    uint64_t nSlicePitchBytes{};
    bool bBufferToTexture{};
};

/// Source/destination images and multisample resolve mode.
struct SIRRenderTextureResolve
{
    SIRRenderHandle hSource{};
    SIRRenderHandle hDestination{};
    SIRRenderTextureRegion source{};
    SIRRenderTextureRegion destination{};
    SIRRenderResolveMode eMode{ SIRRenderResolveMode::Average };
};

/// Source/destination images and filtered blit settings.
struct SIRRenderTextureBlit
{
    SIRRenderHandle hSource{};
    SIRRenderHandle hDestination{};
    SIRRenderTextureRegion source{};
    SIRRenderTextureRegion destination{};
    SIRRenderFilter eFilter{ SIRRenderFilter::Linear };
};

/// Texture subresource region and values to clear.
struct SIRRenderClearTexture
{
    SIRRenderHandle hTexture{};
    SIRRenderSubresourceRange subresources{};
    float szColor[4]{};
    float fDepth{};
    uint32_t nStencil{};
};

/// GPU query type and number of allocated slots.
struct SIRRenderQueryPoolDesc
{
    SIRRenderQueryType eType{ SIRRenderQueryType::Timestamp };
    uint32_t nQueryCount{};
};

/// Pool, query slot, and stage for query recording.
struct SIRRenderQueryCommand
{
    SIRRenderHandle hPool{};
    uint32_t nQuery{};
    uint64_t nStageMask{};
};

/// Query range and destination layout for resolved results.
struct SIRRenderQueryResultsDesc
{
    SIRRenderHandle hPool{};
    SIRRenderHandle hDestination{};
    uint32_t nFirstQuery{};
    uint32_t nQueryCount{};
    uint64_t nDestinationOffsetBytes{};
    uint64_t nStrideBytes{};
    bool bWait{};
    bool bIncludeAvailability{};
};

/// Ray-tracing geometry buffers and primitive format.
struct SIRRenderRayGeometry
{
    SIRRenderRayGeometryType eType{ SIRRenderRayGeometryType::Triangles };
    SIRRenderHandle hVertexBuffer{};
    SIRRenderHandle hIndexBuffer{};
    SIRRenderHandle hTransformBuffer{};
    uint64_t nVertexOffsetBytes{};
    uint64_t nIndexOffsetBytes{};
    uint32_t nVertexStrideBytes{};
    uint32_t nPrimitiveCount{};
    SIRRenderFormat eVertexFormat{ SIRRenderFormat::RGB32_FLOAT };
    SIRRenderIndexType eIndexType{ SIRRenderIndexType::UInt32 };
    bool bOpaque{ true };
};

/// Type, backing storage, and flags of an acceleration structure.
struct SIRRenderAccelerationStructureDesc
{
    SIRRenderAccelerationType eType{ SIRRenderAccelerationType::BottomLevel };
    SIRRenderHandle hBackingBuffer{};
    uint64_t nBackingOffsetBytes{};
    uint64_t nBackingSizeBytes{};
    uint32_t nBuildFlags{};
};

/// Inputs and scratch space for acceleration-structure construction.
struct SIRRenderAccelerationBuild
{
    SIRRenderHandle hDestination{};
    SIRRenderHandle hSource{};
    SIRRenderHandle hScratch{};
    uint64_t nScratchOffsetBytes{};
    SIRSpan<SIRRenderRayGeometry> geometries{};
    bool bUpdate{};
};

/// A contiguous section of a ray shader table.
struct SIRRenderShaderTableRegion
{
    SIRRenderHandle hBuffer{};
    uint64_t nOffsetBytes{};
    uint64_t nSizeBytes{};
    uint64_t nStrideBytes{};
};

/// Ray generation, miss, hit, and callable shader-table sections.
struct SIRRenderShaderTableDesc
{
    SIRRenderShaderTableRegion rayGeneration{};
    SIRRenderShaderTableRegion miss{};
    SIRRenderShaderTableRegion hit{};
    SIRRenderShaderTableRegion callable{};
};

/// Ray-tracing pipeline/table and 3D dispatch dimensions.
struct SIRRenderTraceRaysArguments
{
    SIRRenderHandle hPipeline{};
    SIRRenderHandle hShaderTable{};
    uint32_t nWidth{ 1 };
    uint32_t nHeight{ 1 };
    uint32_t nDepth{ 1 };
};

/// Human-readable GPU debug annotation.
struct SIRRenderDebugMarker
{
    SIRFixedString<64> szName{};
    float szColor[4]{ 1.0F, 1.0F, 1.0F, 1.0F };
};

// Window services live in the platform subsystem. The native surface owns no
// OS window. Each swapchain owns presentation images until its own epoch ends.
/// Borrowed OS window/display objects for surface creation.
struct SIRRenderNativeSurface
{
    SIRRenderSurfacePlatform ePlatform{ SIRRenderSurfacePlatform::Headless };
    void *pWindow{};
    void *pDisplayOrInstance{};
    uint64_t nWindowEpoch{};
};

/// Native surface and its debugging label.
struct SIRRenderSurfaceDesc
{
    SIRRenderNativeSurface native{};
    SIRFixedString<64> szDebugName{};
};

/// Presentation format, mode, size, and buffering preferences.
struct SIRRenderSwapchainDesc
{
    SIRRenderHandle hSurface{};
    SIRRenderFormat eFormat{ SIRRenderFormat::BGRA8_UNORM };
    SIRRenderColorSpace eColorSpace{ SIRRenderColorSpace::SRGB };
    SIRRenderPresentMode ePresentMode{ SIRRenderPresentMode::FIFO };
    uint32_t nWidth{};
    uint32_t nHeight{};
    uint32_t nImageCount{ 2 };
    uint32_t nSampleCount{ 1 };
    bool bAllowTearing{};
};

/// Supported surface presentation capabilities.
struct SIRRenderSwapchainSupport
{
    uint32_t nMinImageCount{};
    uint32_t nMaxImageCount{};
    uint32_t nSupportedPresentModes{};
    uint32_t nSupportedColorSpaces{};
    bool bSupportsHDR{};
    bool bSupportsTearing{};
};

/// Swapchain image acquisition request and timeout.
struct SIRRenderAcquireDesc
{
    SIRRenderHandle hSwapchain{};
    uint64_t nTimeoutNanoseconds{};
};

/// Acquired image and the epochs that validate it.
struct SIRRenderAcquiredImage
{
    SIRRenderHandle hSwapchain{};
    SIRRenderHandle hImage{};
    SIRRenderHandle hImageView{};
    uint64_t nAcquisitionId{};
    uint64_t nWindowEpoch{};
    uint32_t nSwapchainEpoch{};
};

/// Image and completion dependency for a present operation.
struct SIRRenderPresentDesc
{
    SIRRenderAcquiredImage image{};
    SIRRenderCompletion waitFor{};
};

// The RHI command list is a driver-owned transient recording object, not a
// portable bytecode format. Command parameters are borrowed only during record.
/// Initial queue and usage flags of a command recording.
struct SIRRenderCommandListBeginDesc
{
    SIRRenderQueueType eQueue{ SIRRenderQueueType::Graphics };
    bool bSecondary{};
    bool bOneTimeSubmit{ true };
};

class SIRRenderCommandList;
/// Recorded command lists, waits, and acquired image dependencies.
struct SIRRenderSubmitDesc
{
    SIRSpan<SIRRenderCommandList *> lists{};
    SIRSpan<SIRRenderCompletion> waits{};
    SIRSpan<SIRRenderAcquiredImage> acquiredImages{};
};

// Engine-facing frame input: all pipelines receive the same pass graph, draw
// intent, material parameters, and declared resource use. Device-family
// translators lower this graph to their own executable command recordings.
// No scene objects, native resource pointers, or external GRDG types cross it.
/// Physical handle or transient ID naming a frame resource.
struct SIRRenderResourceReference
{
    SIRRenderHandle hPhysical{};
    uint64_t nTransientId{};
};

/// Temporary buffer/texture declaration within a frame.
struct SIRRenderFrameTransientResource
{
    uint64_t nTransientId{};
    const SIRRenderBufferDesc *pBuffer{};
    const SIRRenderTextureDesc *pTexture{};
    bool bAllowAliasing{};
};

/// Declared range, access, and intent of a frame resource.
struct SIRRenderFrameResourceUse
{
    SIRRenderResourceReference resource{};
    SIRRenderBufferRange bufferRange{};
    SIRRenderSubresourceRange subresources{};
    SIRRenderSyncScope scope{};
    SIRRenderResourceIntent eIntent{ SIRRenderResourceIntent::Read };
};

/// Color attachment referenced by logical frame work.
struct SIRRenderFrameColorAttachment
{
    SIRRenderResourceReference view{};
    SIRRenderResourceReference resolveView{};
    SIRRenderLoadOp eLoad{ SIRRenderLoadOp::Load };
    SIRRenderStoreOp eStore{ SIRRenderStoreOp::Store };
    SIRRenderResolveMode eResolve{ SIRRenderResolveMode::None };
    float szClear[4]{};
};

/// Depth/stencil attachment referenced by logical frame work.
struct SIRRenderFrameDepthStencilAttachment
{
    SIRRenderResourceReference view{};
    SIRRenderResourceReference resolveView{};
    SIRRenderLoadOp eDepthLoad{ SIRRenderLoadOp::Load };
    SIRRenderStoreOp eDepthStore{ SIRRenderStoreOp::Store };
    SIRRenderLoadOp eStencilLoad{ SIRRenderLoadOp::Load };
    SIRRenderStoreOp eStencilStore{ SIRRenderStoreOp::Store };
    SIRRenderResolveMode eDepthResolve{ SIRRenderResolveMode::None };
    SIRRenderResolveMode eStencilResolve{ SIRRenderResolveMode::None };
    float fClearDepth{};
    uint32_t nClearStencil{};
};

/// Attachment collection and render area of a frame pass.
struct SIRRenderFrameAttachments
{
    SIRSpan<SIRRenderFrameColorAttachment> colors{};
    SIRRenderFrameDepthStencilAttachment depthStencil{};
    SIRRenderRect renderArea{};
};

/// Value category stored in a frame material parameter.
enum class SIRRenderParameterType : uint8_t
{
    Bytes, ///< Inline parameter bytes.
    UniformBuffer, ///< Parameter references a uniform buffer.
    StorageBuffer, ///< Parameter references a storage buffer.
    SampledTexture, ///< Parameter references a sampled texture.
    StorageTexture, ///< Parameter references a writable texture.
    Sampler, ///< Parameter references sampler state.
    CombinedTextureSampler, ///< Parameter references a combined texture/sampler.
    AccelerationStructure ///< Parameter references an acceleration structure.
};

/// Material semantic, value bytes, or bound resource.
struct SIRRenderFrameParameter
{
    uint64_t nSemanticId{};
    SIRRenderResourceReference resource{};
    SIRRenderHandle hSampler{};
    SIRSpan<uint8_t> bytes{};
    SIRRenderBufferRange bufferRange{};
    SIRRenderParameterType eType{ SIRRenderParameterType::Bytes };
};

/// Vertex stream used by a logical frame draw.
struct SIRRenderFrameVertexBuffer
{
    SIRRenderResourceReference buffer{};
    uint64_t nOffsetBytes{};
    uint32_t nBinding{};
    uint32_t nStrideBytes{};
};

// Indirect arguments are logical frame resources until the family compiler
// materializes a native buffer handle. Counted indirect drawing requires
// device support; a missing count buffer is valid when bCounted is false.
/// Indirect draw argument and optional count-resource references.
struct SIRRenderFrameIndirectDraw
{
    SIRRenderResourceReference arguments{};
    SIRRenderResourceReference count{};
    uint64_t nArgumentOffsetBytes{};
    uint64_t nCountOffsetBytes{};
    uint32_t nMaxCommandCount{ 1 };
    uint32_t nStrideBytes{};
    bool bCounted{};
};

// Optional per-draw state. nMask contains SIRRenderDynamicState bits; absent
// state uses the pass/pipeline defaults. Fixed-function material/transform
// semantics are passed through SIRRenderFrameParameter, not native GL/D3D data.
/// Optional viewport/scissor/blend/depth state for one draw.
struct SIRRenderFrameDynamicState
{
    SIRRenderViewport viewport{};
    SIRRenderRect scissor{};
    SIRRenderFixedColor blendConstant{};
    uint32_t nMask{};
    uint32_t nStencilReference{};
    float fDepthBiasConstant{};
    float fDepthBiasSlope{};
    float fDepthBiasClamp{};
    float fMinDepthBounds{};
    float fMaxDepthBounds{ 1.0F };
    float fLineWidth{ 1.0F };
    SIRRenderCullMode eCull{ SIRRenderCullMode::Back };
    SIRRenderFrontFace eFrontFace{ SIRRenderFrontFace::CounterClockwise };
};

/// One logical geometry draw and its material parameters.
struct SIRRenderFrameDraw
{
    uint64_t nTechniqueId{};
    SIRSpan<SIRRenderFrameVertexBuffer> vertices{};
    SIRRenderResourceReference indexBuffer{};
    SIRSpan<SIRRenderFrameParameter> parameters{};
    SIRRenderDrawArguments verticesDraw{};
    SIRRenderDrawIndexedArguments indicesDraw{};
    SIRRenderFrameIndirectDraw indirect{};
    SIRRenderFrameDynamicState dynamicState{};
    SIRRenderIndexType eIndexType{ SIRRenderIndexType::UInt16 };
    bool bIndexed{};
    bool bIndirect{};
};

/// Compute dispatch and its material/resource parameters.
struct SIRRenderFrameDispatch
{
    uint64_t nTechniqueId{};
    SIRSpan<SIRRenderFrameParameter> parameters{};
    SIRRenderDispatchArguments groups{};
    SIRRenderResourceReference indirectArguments{};
    uint64_t nIndirectOffsetBytes{};
    bool bIndirect{};
};

/// Mesh dispatch and its material/resource parameters.
struct SIRRenderFrameDispatchMesh
{
    uint64_t nTechniqueId{};
    SIRSpan<SIRRenderFrameParameter> parameters{};
    SIRRenderDispatchArguments groups{};
    SIRRenderResourceReference indirectArguments{};
    uint64_t nIndirectOffsetBytes{};
    bool bIndirect{};
};

/// Ray dispatch and its material/resource parameters.
struct SIRRenderFrameTraceRays
{
    uint64_t nTechniqueId{};
    SIRSpan<SIRRenderFrameParameter> parameters{};
    SIRRenderResourceReference shaderTable{};
    uint32_t nWidth{ 1 };
    uint32_t nHeight{ 1 };
    uint32_t nDepth{ 1 };
};

/// Logical copy/resolve operation within a frame.
struct SIRRenderFrameTransfer
{
    SIRRenderResourceReference source{};
    SIRRenderResourceReference destination{};
    SIRRenderBufferRange sourceBufferRange{};
    SIRRenderBufferRange destinationBufferRange{};
    SIRRenderTextureRegion sourceTexture{};
    SIRRenderTextureRegion destinationTexture{};
    uint64_t nByteCount{};
    uint64_t nRowPitchBytes{};
    uint64_t nSlicePitchBytes{};
};

// Exactly one pointer per work kind. No command stream is passed through this
// descriptor: it expresses intent for the pipeline-family translator.
/// Tagged draw, dispatch, ray, or transfer operation.
struct SIRRenderFrameWorkItem
{
    SIRRenderWorkKind eKind{ SIRRenderWorkKind::Draw };
    const SIRRenderFrameDraw *pDraw{};
    const SIRRenderFrameDispatch *pDispatch{};
    const SIRRenderFrameDispatchMesh *pMesh{};
    const SIRRenderFrameTraceRays *pRays{};
    const SIRRenderFrameTransfer *pTransfer{};
};

/// One dependency-aware frame pass and its resource uses.
struct SIRRenderFramePass
{
    uint64_t nPassId{};
    SIRFixedString<64> szDebugName{};
    SIRSpan<uint64_t> dependencies{};
    SIRSpan<SIRRenderFrameResourceUse> resources{};
    SIRSpan<SIRRenderFrameWorkItem> items{};
    SIRRenderFrameAttachments attachments{};
    SIRRenderPassKind eKind{ SIRRenderPassKind::Graphics };
    SIRRenderQueueType ePreferredQueue{ SIRRenderQueueType::Graphics };
    bool bAllowMerge{ true };
};

// The technique set belongs to the renderer/material compiler. Multiple
// candidates may target one era; nPriority resolves ties after capability
// validation. Pipeline descriptors are deep-copied when compiled/cached.
/// One pipeline candidate with feature requirements and priority.
struct SIRRenderTechniqueCandidate
{
    uint64_t nPipelineRecipeId{};
    uint64_t nRequiredFeatureMask{};
    uint64_t nForbiddenFeatureMask{};
    uint32_t nRequiredShaderStageMask{};
    uint32_t nRequiredTextureStages{};
    uint32_t nPriority{};
    SIRPipelineEra eEra{ SIRPipelineEra::Advanced };
};

/// All implementation candidates for a material technique.
struct SIRRenderTechniqueSet
{
    uint64_t nTechniqueId{};
    SIRSpan<SIRRenderTechniqueCandidate> candidates{};
};

// Native recipe construction happens below the engine-facing frame boundary.
// The provider materializes shader handles, pipeline layouts, and semantic
// routes for this exact device and candidate. The logical frame stores IDs only.
/// How a semantic material parameter maps to a pipeline-era input.
enum class SIRRenderParameterDestination : uint8_t
{
    Descriptor, ///< Map parameter to a shader descriptor binding.
    PushConstant, ///< Map parameter to inline/push-constant bytes.
    FixedTextureStage, ///< Map parameter to a fixed-function texture stage.
    FixedTransforms, ///< Map parameter to fixed-function transforms.
    FixedMaterial, ///< Map parameter to fixed-function material state.
    FixedLighting, ///< Map parameter to fixed-function lighting state.
    FixedFog ///< Map parameter to fixed-function fog state.
};

/// Routing of a semantic parameter into a backend pipeline binding.
struct SIRRenderParameterRoute
{
    uint64_t nSemanticId{};
    uint32_t nSet{};
    uint32_t nBinding{};
    uint32_t nArrayElement{};
    uint32_t nByteOffset{};
    uint32_t nStageMask{};
    SIRRenderParameterDestination eDestination{ SIRRenderParameterDestination::Descriptor };
};

/// Request for a specific compiled pipeline recipe.
struct SIRRenderPipelineRecipeRequest
{
    uint64_t nPipelineRecipeId{};
    uint64_t nDeviceId{};
    uint64_t nRenderTargetSignature{};
    uint32_t nDeviceEpoch{};
    SIRRenderBackend eBackend{ SIRRenderBackend::Auto };
    SIRPipelineEra eEra{ SIRPipelineEra::Advanced };
};

/// Pipeline recipe and semantic binding routes supplied by renderer.
struct SIRRenderPipelineRecipe
{
    SIRRenderPipelineDesc pipeline{};
    SIRSpan<SIRRenderParameterRoute> parameterRoutes{};
    uint64_t nRecipeVersion{};
};

class SIRRenderPipelineRecipeProvider;
/// Coordinate/depth/front-face conventions of a frame.
struct SIRRenderFrameConvention
{
    bool bDepthZeroToOne{ true };
    bool bOriginUpperLeft{ true };
    bool bReverseZ{ true };
    SIRRenderFrontFace eFrontFace{ SIRRenderFrontFace::CounterClockwise };
};

/// Frame passes, temporary resources, techniques, and target image.
struct SIRRenderFrameRequest
{
    uint64_t nFrameId{};
    SIRSpan<SIRRenderFrameTransientResource> transients{};
    SIRSpan<SIRRenderTechniqueSet> techniques{};
    SIRSpan<SIRRenderFramePass> passes{};
    SIRRenderFrameConvention convention{};
    SIRRenderAcquiredImage acquiredImage{};
};

/// Complete request to lower/execute and optionally present a frame.
struct SIRRenderFrameExecutionDesc
{
    SIRRenderFrameRequest frame{};
    SIRRenderPipelineRecipeProvider *pRecipes{};
    bool bPresent{};
    bool bAllowAsyncQueues{ true };
};

/// Diagnostic error code, context IDs, and native driver status.
struct SIRRenderDiagnostic
{
    SIRRenderResult eResult{ SIRRenderResult::Success };
    SIRRenderBackend eBackend{ SIRRenderBackend::Auto };
    uint64_t nPassId{};
    uint64_t nTechniqueId{};
    uint32_t nNativeCode{};
    char szMessage[256]{};
};

static_assert( sizeof( SIRRenderTextureFilter ) == 1 );
static_assert( std::is_standard_layout<SIRRenderHandle>::value, "RHI handles require standard layout" );
static_assert( std::is_trivially_copyable<SIRRenderHandle>::value, "RHI handles must be trivially copyable" );
static_assert( sizeof( SIRRenderHandle ) == 24, "RHI handle layout changed" );
static_assert( std::is_trivially_copyable<SIRRenderCompletion>::value, "Completion must be trivially copyable" );
static_assert( std::is_trivially_copyable<SIRRenderPipelineDesc>::value, "Pipeline descriptor must remain POD-like" );

#endif // __SIR_RENDER_TYPES_H__
