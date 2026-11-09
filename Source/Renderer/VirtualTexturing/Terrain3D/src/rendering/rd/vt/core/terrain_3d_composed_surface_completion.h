// CS10: submission epochs are acknowledged only by completed GPU readbacks.
#ifndef TERRAIN3D_COMPOSED_SURFACE_COMPLETION_H
#define TERRAIN3D_COMPOSED_SURFACE_COMPLETION_H

#include <stdint.h>

inline bool t3d_comp_surf_gpu_epoch_completed( const uint64_t nRequiredEpoch, const uint64_t nCompletedEpoch )
{
	return nRequiredEpoch != 0u && nCompletedEpoch >= nRequiredEpoch;
}

// Zero/expired epochs are not valid submission tokens. The producer must
// record the owner's current epoch before encoding the transfer commands.
inline bool t3d_comp_surf_gpu_submission_epoch_valid( const uint64_t nSubmissionEpoch, const uint64_t nCompletedEpoch )
{
	return nSubmissionEpoch != 0u && nSubmissionEpoch > nCompletedEpoch;
}

#endif
