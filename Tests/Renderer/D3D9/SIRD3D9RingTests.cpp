#include <Renderer/Drivers/D3D9/SIRD3D9UploadRing.h>
#include <assert.h>

int main( void )
{
	sirD3D9UploadRing_t ring = {};
	sirD3D9UploadRange_t range = {};
	assert( sirD3D9RingInitialize( nullptr, 64 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	assert( sirD3D9RingInitialize( &ring, 0 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	assert( sirD3D9RingInitialize( &ring, 64 ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9RingReserve( &ring, 20, 16, &range ) == SIR_GRAPHICS_SUCCESS );
	assert( range.nOffset == 0 && range.nLockFlags == SIR_D3D9_RING_DISCARD );
	assert( sirD3D9RingReserve( &ring, 16, 16, &range ) == SIR_GRAPHICS_SUCCESS );
	assert( range.nOffset == 32 && range.nLockFlags == SIR_D3D9_RING_NOOVERWRITE );
	assert( sirD3D9RingReserve( &ring, 32, 16, &range ) == SIR_GRAPHICS_BUSY );
	assert( sirD3D9RingSubmit( &ring, 1 ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9RingReserve( &ring, 32, 16, &range ) == SIR_GRAPHICS_BUSY );
	assert( sirD3D9RingComplete( &ring, 2 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	assert( sirD3D9RingComplete( &ring, 1 ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9RingReserve( &ring, 32, 16, &range ) == SIR_GRAPHICS_SUCCESS );
	assert( range.nOffset == 0 && range.nGeneration == 2 && range.nLockFlags == SIR_D3D9_RING_DISCARD );
	assert( sirD3D9RingReserve( &ring, 65, 1, &range ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	assert( sirD3D9RingReserve( &ring, 1, 3, &range ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	sirD3D9RingReset( &ring );
	assert( ring.nCursor == 0 && ring.nGeneration == 3 && ring.nCapacity == 64 );
	return 0;
}
