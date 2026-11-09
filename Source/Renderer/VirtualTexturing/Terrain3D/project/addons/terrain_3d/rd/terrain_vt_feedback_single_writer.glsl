// Experimental deterministic coarse VT feedback from an already
// depth-tested full-resolution R32_UINT virtual-page attachment.
// The raster producer is intentionally a separate integration requirement:
// ordinary fragment SSBO stores are not a collision-free input contract.
// Each compute invocation exclusively owns ONE coarse feedback cell; no atomics.
// The v2 4-slot grid header is prepared by the existing VT feedback owner.
#[versions]
reduce = "";

#[compute]
#version 450
#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform usampler2D source_pages;
layout(set = 0, binding = 1, std430) buffer CoarseFeedbackGrid
{
	uint words[];
}
destination;

layout(push_constant, std430) uniform FeedbackParams
{
	uvec4 dimensions; // x/y: coarse grid, z/w: full-resolution depth-tested source
	uvec4 limits;     // x: slot 0..3, y: slot stride words, z: page count, w: unused
}
params;

const uint INVALID_PAGE = 0xffffffffu;
const uint GRID_HEADER_WORDS = 10u;
const uint GRID_SLOTS = 4u;
const uint SAMPLE_AXIS = 4u;
const uint SAMPLES_PER_CELL = SAMPLE_AXIS * SAMPLE_AXIS;

// Original fragment producer: floor(pixel * gridExtent / viewportExtent).
// Its inverse cell interval is [ceil(cell * viewport / grid),
// ceil((cell + 1) * viewport / grid)). With grid <= 240 and viewport <=
// 65535, multiplication safely fits 32 bits.
uint inverse_cell_edge(uint cell, uint viewport_extent, uint grid_extent)
{
	return (cell * viewport_extent + grid_extent - 1u) / grid_extent;
}

void main()
{
	uvec2 cell = gl_GlobalInvocationID.xy;
	uint grid_width = params.dimensions.x;
	uint grid_height = params.dimensions.y;
	uint viewport_width = params.dimensions.z;
	uint viewport_height = params.dimensions.w;
	uint slot = params.limits.x;
	uint slot_stride = params.limits.y;
	uint page_count = params.limits.z;

	if (cell.x >= grid_width || cell.y >= grid_height)
		return;
	if (grid_width == 0u || grid_height == 0u || grid_width > 240u || grid_height > 136u ||
	viewport_width == 0u || viewport_height == 0u ||
	viewport_width > 65535u || viewport_height > 65535u ||
	slot >= GRID_SLOTS || page_count == 0u ||
	slot_stride < GRID_HEADER_WORDS + grid_width * grid_height ||
	textureSize(source_pages, 0) != ivec2(viewport_width, viewport_height)) return;

	uint offset = slot * slot_stride + GRID_HEADER_WORDS + cell.y * grid_width + cell.x;
	if (offset >= destination.words.length())
		return;

	uint x0 = inverse_cell_edge(cell.x, viewport_width, grid_width);
	uint x1 = min(inverse_cell_edge(cell.x + 1u, viewport_width, grid_width), viewport_width);
	uint y0 = inverse_cell_edge(cell.y, viewport_height, grid_height);
	uint y1 = min(inverse_cell_edge(cell.y + 1u, viewport_height, grid_height), viewport_height);

	uint selected = INVALID_PAGE;
	if (x0 < x1 && y0 < y1)
	{
		// Build an exact, compact per-cell histogram in one pass. The previous
		// reduction rescanned remaining candidates after every unique page;
		// this counts each occurrence as it is fetched. No inter-invocation
		// atomics or changes to the grid output format are required.
		uint unique_pages[SAMPLES_PER_CELL];
		uint frequencies[SAMPLES_PER_CELL];
		uint unique_count = 0u;
		uint w = x1 - x0;
		uint h = y1 - y0;
		for (uint i = 0u; i < SAMPLES_PER_CELL; ++i)
		{
			uint x = x0 + min(((2u * (i % SAMPLE_AXIS) + 1u) * w) / (2u * SAMPLE_AXIS), w - 1u);
			uint y = y0 + min(((2u * (i / SAMPLE_AXIS) + 1u) * h) / (2u * SAMPLE_AXIS), h - 1u);
			uint page = texelFetch(source_pages, ivec2(x, y), 0).r;
			if (page >= page_count)
				continue;
			uint found = unique_count;
			for (uint k = 0u; k < unique_count; ++k)
			{
				if (unique_pages[k] == page)
				{
					found = k;
					break;
				}
			}
			if (found == unique_count)
			{
				unique_pages[unique_count] = page;
				frequencies[unique_count] = 1u;
				++unique_count;
			}
			else
			{
				++frequencies[found];
			}
		}
		uint best_frequency = 0u;
		for (uint i = 0u; i < unique_count; ++i)
		{
			uint candidate = unique_pages[i];
			uint frequency = frequencies[i];
			if (frequency > best_frequency ||
			(frequency == best_frequency && candidate < selected))
			{
				selected = candidate;
				best_frequency = frequency;
			}
		}
	}
	// One invocation owns each destination word. The prepared v2 header and
	// the other three feedback slots are never modified by this shader.
	destination.words[offset] = selected;
}
