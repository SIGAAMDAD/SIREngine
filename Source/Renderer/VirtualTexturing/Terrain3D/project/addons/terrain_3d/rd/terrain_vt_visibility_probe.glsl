// T02 cold authoring-only GPU visibility bake.
// Stage code is loaded into RDShaderSource by Terrain3DVTVisibilityBaker.
// No shipping/runtime shader ID, render pass, or global RD resource is added.
#[vertex]
#version 450

struct ProbeVertex
{
	vec4 world; vec4 uv_and_material;
};
layout(set = 0, binding = 0, std430) readonly buffer StagedGeometry
{
	ProbeVertex vertices[];
}
geometry_data;

layout(push_constant, std430) uniform ProbePush
{
	vec4 eye;
	vec4 right;
	vec4 up;
	vec4 forward;
	uvec4 parameters; // x = view bin [0,9], y = nominal texture edge, z = material count, w = depth-only
}
push_data;
layout(location = 0) out vec2 out_uv;
layout(location = 1) flat out uint out_material;

void main()
{
	ProbeVertex vertex = geometry_data.vertices[gl_VertexIndex];
	vec3 delta = vertex.world.xyz - push_data.eye.xyz;
	float front_depth = dot(delta, push_data.forward.xyz);
	// Vulkan 0..1 depth; near 0.1m and far 8192m. 90-degree cube/yaw view.
	float n = 0.1;
	float f = 8192.0;
	gl_Position = vec4(dot(delta, push_data.right.xyz) * push_data.eye.w,
	dot(delta, push_data.up.xyz) * push_data.eye.w,
	f / (f - n) * (front_depth - n), front_depth);
	out_uv = vertex.uv_and_material.xy;
	out_material = uint(vertex.uv_and_material.z);
}

#[fragment]
#version 450
layout(early_fragment_tests) in;
layout(location = 0) in vec2 in_uv;
layout(location = 1) flat in uint in_material;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1, std430) buffer VisibilityHistogram
{
	uint count[]; // material-major, view-major (8), mip-major (16)
}
hist;
layout(push_constant, std430) uniform ProbePush
{
	vec4 eye;
	vec4 right;
	vec4 up;
	vec4 forward;
	uvec4 parameters;
}
push_data;

void main()
{
	// Final visibility needs a completed depth prepass. Early depth tests in
	// a single draw are insufficient: hidden fragments can increment before
	// closer triangles update depth later in the same draw.
	if (push_data.parameters.w != 0u)
	{
		out_color = vec4(0.0);
		return;
	}
	if (in_material >= push_data.parameters.z)
		discard;
	vec2 scaled_uv = in_uv * float(push_data.parameters.y);
	vec2 dx = dFdx(scaled_uv);
	vec2 dy = dFdy(scaled_uv);
	float rho = max(dot(dx, dx), dot(dy, dy));
	uint mip = uint(clamp(floor(0.5 * log2(max(rho, 1.0))), 0.0, 15.0));
	uint base = in_material * 8u * 16u + mip;
	if (push_data.parameters.x < 8u)
	{
		atomicAdd(hist.count[base + push_data.parameters.x * 16u], 1u);
	}
	else
	{
		// +Y / -Y cubefaces: conservative contribution to every yaw octant.
		for (uint view = 0u; view < 8u; ++view)
		{
			atomicAdd(hist.count[base + view * 16u], 1u);
		}
	}
	out_color = vec4(0.0);
}
