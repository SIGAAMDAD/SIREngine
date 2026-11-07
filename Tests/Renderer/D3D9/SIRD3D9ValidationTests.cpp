#include <Renderer/Drivers/D3D9/SIRD3D9Validation.h>
#include <assert.h>

int main( void )
{
	const uint32_t szVertex[] = { 0xfffe0300U, 0x0000ffffU };
	const uint32_t szPixel[] = { 0xffff0200U, 0x0000ffffU };
	assert( sirD3D9ValidateShaderTokens( szVertex, sizeof( szVertex ), SIR_GRAPHICS_STAGE_VERTEX, 0x0300 ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9ValidateShaderTokens( szVertex, sizeof( szVertex ), SIR_GRAPHICS_STAGE_FRAGMENT, 0x0300 ) == SIR_GRAPHICS_UNSUPPORTED );
	assert( sirD3D9ValidateShaderTokens( szVertex, sizeof( szVertex ), SIR_GRAPHICS_STAGE_VERTEX, 0x0200 ) == SIR_GRAPHICS_UNSUPPORTED );
	assert( sirD3D9ValidateShaderTokens( szPixel, sizeof( szPixel ), SIR_GRAPHICS_STAGE_FRAGMENT, 0x0300 ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9ValidateShaderTokens( nullptr, 8, SIR_GRAPHICS_STAGE_VERTEX, 0x0300 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	assert( sirD3D9ValidateShaderTokens( szVertex, 7, SIR_GRAPHICS_STAGE_VERTEX, 0x0300 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 512;
	buffer.nAlignmentBytes = 16;
	buffer.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	assert( sirD3D9ValidateBuffer( &buffer ) == SIR_GRAPHICS_SUCCESS );
	buffer.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	assert( sirD3D9ValidateBuffer( &buffer ) == SIR_GRAPHICS_UNSUPPORTED );
	buffer.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	buffer.nSizeBytes = UINT64_MAX;
	assert( sirD3D9ValidateBuffer( &buffer ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 256;
	texture.nHeight = 256;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	assert( sirD3D9ValidateTexture( &texture, 2048, 4 ) == SIR_GRAPHICS_SUCCESS );
	texture.eFormat = SIR_GRAPHICS_FORMAT_BC7_UNORM;
	assert( sirD3D9ValidateTexture( &texture, 2048, 4 ) == SIR_GRAPHICS_UNSUPPORTED );
	texture.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	texture.nSampleCount = 4;
	assert( sirD3D9ValidateTexture( &texture, 2048, 4 ) == SIR_GRAPHICS_UNSUPPORTED );
	texture.nWidth = 4096;
	assert( sirD3D9ValidateTexture( &texture, 2048, 4 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	return 0;
}
