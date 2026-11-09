// Authoring-only adapter to the existing reference BC encoder. No new codec.
#ifndef TERRAIN3D_COMPOSED_SURFACE_ENCODER_ADAPTER_H
#define TERRAIN3D_COMPOSED_SURFACE_ENCODER_ADAPTER_H
#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"
#include "authoring/vt/build/terrain_3d_vt_reference_encoder.h"

inline bool t3d_comp_surf_reference_encode( void *pUser, const uint32_t nChannel, const uint8_t *pRgba, const uint32_t nRgbaBytes,
                                            uint8_t *pCompressed, const uint32_t nCapacity, uint32_t& rWritten )
{
	( void )pUser;
	if ( nChannel >= T3D_COMP_SURF_CHANNEL_COUNT || nRgbaBytes != T3D_COMP_SURF_RGBA_SCRATCH_BYTES || !pRgba || !pCompressed ||
	     nCapacity < t3d_comp_surf_channel_bytes( nChannel ) )
		return false;
	t3dVTEncoderInterface_t encoder{};
	t3d_vt_reference_encoder_make_interface( encoder );
	t3dVTEncodeTileRequest_t request{};
	request.m_pSourcePixels = pRgba;
	request.m_nSourceBytes = nRgbaBytes;
	request.m_nChannel = nChannel;
	request.m_nFormat = nChannel == T3D_COMP_SURF_CHANNEL_ALBEDO   ? T3D_VT_PHYSICAL_FORMAT_BC1_UNORM
	                    : nChannel == T3D_COMP_SURF_CHANNEL_NORMAL ? T3D_VT_PHYSICAL_FORMAT_BC5_UNORM
	                                                               : T3D_VT_PHYSICAL_FORMAT_BC7_UNORM;
	request.m_nWidth = T3D_COMP_SURF_PHYSICAL_TILE;
	request.m_nHeight = T3D_COMP_SURF_PHYSICAL_TILE;
	request.m_nComponents = 4u;
	request.m_nQuality = 1u;
	request.m_nContractVersion = T3D_VT_FILE_FORMAT_VERSION; // Codec selector only; .t3sc ABI remains independent.
	request.m_pDestination = pCompressed;
	request.m_nDestinationCapacity = nCapacity;
	rWritten = 0u;
	return encoder.m_pEncodeTile && encoder.m_pEncodeTile( encoder.m_pUser, request, rWritten ) &&
	       rWritten == t3d_comp_surf_channel_bytes( nChannel );
}
#endif
