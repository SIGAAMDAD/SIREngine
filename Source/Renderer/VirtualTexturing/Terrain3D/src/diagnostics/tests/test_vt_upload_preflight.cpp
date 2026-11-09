#include "rendering/rd/vt/gpu/terrain_3d_vt_upload_preflight.h"

#include <assert.h>
#include <stdint.h>

struct Request
{
	uint32_t nVirtualPage;
	uint32_t nPhysicalSlot;
	uint32_t nVictimPage;
};

int main()
{
	const uint32_t invalid = 0xffffffffu;
	const Request first = { 100u, 1u, invalid };
	assert( !t3dVtUploadReservationsConflict( first, Request{ 101u, 2u, invalid }, invalid ) );
	assert( t3dVtUploadReservationsConflict( first, Request{ 100u, 2u, invalid }, invalid ) );
	assert( t3dVtUploadReservationsConflict( first, Request{ 101u, 1u, invalid }, invalid ) );
	const Request evicting = { 102u, 3u, 75u };
	assert( t3dVtUploadReservationsConflict( evicting, Request{ 103u, 4u, 75u }, invalid ) );
	assert( !t3dVtUploadReservationsConflict( evicting, Request{ 103u, 4u, invalid }, invalid ) );
	assert( !t3dVtUploadReservationsConflict( evicting, Request{ 103u, 4u, 76u }, invalid ) );
	const uint8_t valid_data[4] = { 1u, 2u, 3u, 4u };
	assert( t3dVtUploadChannelViewValid( 0u, 4u, valid_data, 4u, 4u, 4 ) );
	assert( !t3dVtUploadChannelViewValid( 4u, 4u, valid_data, 4u, 4u, 4 ) );
	assert( !t3dVtUploadChannelViewValid( 0u, 4u, nullptr, 4u, 4u, 4 ) );
	assert( !t3dVtUploadChannelViewValid( 0u, 4u, valid_data, 3u, 4u, 4 ) );
	assert( !t3dVtUploadChannelViewValid( 0u, 4u, valid_data, 4u, 4u, 3 ) );
	assert( !t3dVtUploadChannelViewValid( 0u, 4u, valid_data, 0u, 0u, 0 ) );
	assert( t3dVtUploadVictimCapacityValid( 0u, 8u, 8u ) );
	assert( t3dVtUploadVictimCapacityValid( 3u, 8u, 5u ) );
	assert( !t3dVtUploadVictimCapacityValid( 4u, 8u, 5u ) );
	assert( !t3dVtUploadVictimCapacityValid( 0u, 8u, 9u ) );
	return 0;
}
