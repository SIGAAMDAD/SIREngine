// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_PAGE_BUILDER_H
#define TERRAIN3D_VT_PAGE_BUILDER_H

#include "Terrain3DTypes.h"

#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"

#include <stdint.h>

uint32_t t3d_vt_page_builder_channel_components( uint32_t nChannel );
uint32_t t3d_vt_page_builder_logical_channel_components( t3dVTLogicalChannel_t Channel );
bool t3d_vt_page_builder_fill_logical_neutral( t3dVTLogicalChannel_t Channel, uint32_t nWidth, uint32_t nHeight, uint8_t *pDestination,
                                               uint64_t nDestinationBytes );
bool t3d_vt_page_builder_normalize_logical_channel_u8( t3dVTLogicalChannel_t Channel, uint32_t nOrmSourceLayout,
                                                       const t3dVTCookImageView_t& rSource, uint8_t *pDestination,
                                                       uint64_t nDestinationBytes );
bool t3d_vt_page_builder_resample_logical_channel_u8( t3dVTLogicalChannel_t Channel, uint32_t nOrmSourceLayout, bool bAlphaScissor,
                                                      uint8_t nAlphaThreshold, const t3dVTCookImageView_t& rSource, uint32_t nTargetWidth,
                                                      uint32_t nTargetHeight, uint8_t *pDestination, uint64_t nDestinationBytes );
bool t3d_vt_page_builder_build_logical_channel_mips_u8( t3dVTLogicalChannel_t Channel, bool bAlphaScissor, uint8_t nAlphaThreshold,
                                                        uint8_t *pImageBytes, uint64_t nImageBytes, const t3dVTMipChainLayout_t& rLayout,
                                                        uint32_t nMipQuality = 1u );
uint32_t t3d_vt_page_builder_mip_count( uint32_t nWidth, uint32_t nHeight );
bool t3d_vt_page_builder_layout_mips( uint32_t nWidth, uint32_t nHeight, uint32_t nComponents, t3dVTMipChainLayout_t& rLayout );
bool t3d_vt_page_builder_fill_neutral( uint32_t nChannel, uint32_t nWidth, uint32_t nHeight, uint8_t *pDestination,
                                       uint64_t nDestinationBytes );
bool t3d_vt_page_builder_normalize_channel_u8( uint32_t nChannel, uint32_t nSurfaceSourceLayout, const t3dVTCookImageView_t& rSource,
                                               uint8_t *pDestination, uint64_t nDestinationBytes );
bool t3d_vt_page_builder_resample_u8( const t3dVTCookImageView_t& rSource, uint32_t nTargetWidth, uint32_t nTargetHeight,
                                      uint8_t *pDestination, uint64_t nDestinationBytes );
bool t3d_vt_page_builder_build_mips_u8( uint8_t *pImageBytes, uint64_t nImageBytes, const t3dVTMipChainLayout_t& rLayout );
bool t3d_vt_page_builder_build_channel_mips_u8( uint32_t nChannel, uint32_t nQuality, uint8_t *pImageBytes, uint64_t nImageBytes,
                                                const t3dVTMipChainLayout_t& rLayout );
bool t3d_vt_page_builder_build_tile_u8( const uint8_t *pMipPixels, uint32_t nMipWidth, uint32_t nMipHeight, uint32_t nComponents,
                                        uint32_t nPageX, uint32_t nPageY, t3dVTCookWrapMode_t eWrapMode, uint8_t *pDestination,
                                        uint64_t nDestinationBytes );
uint32_t t3d_vt_page_builder_pages_wide( uint32_t nMipWidth );
uint32_t t3d_vt_page_builder_pages_high( uint32_t nMipHeight );

#endif // TERRAIN3D_VT_PAGE_BUILDER_H
