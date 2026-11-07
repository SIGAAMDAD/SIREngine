#ifndef TERRAIN_3D_VT_UPLOAD_PREFLIGHT_H
#define TERRAIN_3D_VT_UPLOAD_PREFLIGHT_H

#include <stdint.h>

// P07: keep the batch reservation identity check independent of Godot/RD.
// The only two identities that can be re-used by an in-flight batch are
// virtual page and physical slot; the same real victim cannot be retired twice.
// nInvalidPage is supplied by the caller's current VT database contract.
template <typename T>
inline bool t3dVtUploadReservationsConflict( const T& rExisting, const T& rCandidate, const uint32_t nInvalidPage )
{
	return rExisting.nVirtualPage == rCandidate.nVirtualPage || rExisting.nPhysicalSlot == rCandidate.nPhysicalSlot ||
	       ( rCandidate.nVictimPage != nInvalidPage && rExisting.nVictimPage == rCandidate.nVictimPage );
}

// A physical page may only be exposed after all required channel payloads
// pass the same shape contract. Keep this pure for native regression tests.
inline bool t3dVtUploadChannelViewValid( const uint32_t nResourceChannel, const uint32_t nChannelCount, const uint8_t *const pBytes,
                                         const uint32_t nBytes, const uint32_t nExpectedBytes, const int64_t nStagingBytes )
{
	return nResourceChannel < nChannelCount && pBytes != nullptr && nExpectedBytes != 0u && nBytes == nExpectedBytes &&
	       nStagingBytes == ( int64_t )nExpectedBytes;
}

inline bool t3dVtUploadVictimCapacityValid( const uint32_t nPendingVictims, const uint32_t nPhysicalCapacity, const uint32_t nRetired )
{
	return nRetired <= nPhysicalCapacity && nPendingVictims <= nPhysicalCapacity - nRetired;
}

#endif // TERRAIN_3D_VT_UPLOAD_PREFLIGHT_H
