#ifndef TERRAIN3D_VT_NEUTRAL_PAYLOAD_H
#define TERRAIN3D_VT_NEUTRAL_PAYLOAD_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// The caller supplies a nonempty neutral tile and a nonempty destination.
// Keep this CPU operation independent of RenderingDevice so it can be measured.
inline void t3d_vt_repeat_neutral_payload_cold( uint8_t *const pDestination, const size_t nBytes, const uint8_t *const pPattern,
                                                const size_t nPatternBytes )
{
	size_t nFilled = nBytes < nPatternBytes ? nBytes : nPatternBytes;
	memcpy( pDestination, pPattern, nFilled );
	while ( nFilled < nBytes ) {
		const size_t nRemaining = nBytes - nFilled;
		const size_t nCopyBytes = nFilled < nRemaining ? nFilled : nRemaining;
		memcpy( pDestination + nFilled, pDestination, nCopyBytes );
		nFilled += nCopyBytes;
	}
}

#endif
