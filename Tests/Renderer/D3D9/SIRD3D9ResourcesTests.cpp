#include <Renderer/Drivers/D3D9/SIRD3D9Resources.h>
#include <assert.h>

int main( void )
{
	sirD3D9ResourceSlot_t slots[2] = {};
	sirD3D9ResourceTable_t table = {};
	sirGraphicsHandle_t a = {};
	sirGraphicsHandle_t b = {};
	void *pNative = nullptr;
	int nResourceA = 3;
	int nResourceB = 4;
	assert( sirD3D9ResourcesInitialize( &table, slots, 2, 128 ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9ResourcesInsert( &table, SIR_GRAPHICS_HANDLE_BUFFER, 64, &nResourceA, &a ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9ResourcesLookup( &table, a, &pNative ) == SIR_GRAPHICS_SUCCESS && pNative == &nResourceA );
	assert( sirD3D9ResourcesInsert( &table, SIR_GRAPHICS_HANDLE_TEXTURE, 80, &nResourceB, &b ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	assert( sirD3D9ResourcesInsert( &table, SIR_GRAPHICS_HANDLE_TEXTURE, 64, &nResourceB, &b ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9ResourcesInsert( &table, SIR_GRAPHICS_HANDLE_TEXTURE, 1, &nResourceB, &b ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	assert( sirD3D9ResourcesRetire( &table, a, 4 ) == SIR_GRAPHICS_SUCCESS );
	assert( sirD3D9ResourcesLookup( &table, a, &pNative ) == SIR_GRAPHICS_INVALID_HANDLE );
	assert( sirD3D9ResourcesCollect( &table, 3 ) == nullptr );
	assert( sirD3D9ResourcesCollect( &table, 4 ) == &nResourceA );
	assert( sirD3D9ResourcesLookup( &table, a, &pNative ) == SIR_GRAPHICS_INVALID_HANDLE );
	sirGraphicsHandle_t c = {};
	assert( sirD3D9ResourcesInsert( &table, SIR_GRAPHICS_HANDLE_BUFFER, 32, &nResourceA, &c ) == SIR_GRAPHICS_SUCCESS );
	assert( c.nSlot == a.nSlot && c.nGeneration != a.nGeneration );
	assert( sirD3D9ResourcesLookup( &table, c, &pNative ) == SIR_GRAPHICS_SUCCESS );
	sirD3D9ResourcesInvalidate( &table );
	assert( table.nUsedBytes == 0 && table.nDeviceGeneration == 2 );
	assert( sirD3D9ResourcesLookup( &table, c, &pNative ) == SIR_GRAPHICS_INVALID_HANDLE );
	return 0;
}
