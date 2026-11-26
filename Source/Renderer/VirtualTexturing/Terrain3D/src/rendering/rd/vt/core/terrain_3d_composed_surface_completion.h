/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

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
