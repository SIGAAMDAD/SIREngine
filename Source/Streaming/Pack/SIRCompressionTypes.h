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

#ifndef __SIR_COMPRESSIONTYPES_H__
#define __SIR_COMPRESSIONTYPES_H__

#include <Streaming/Pack/SIRPackFormat.h>
#include <Resources/SIRResourceTypes.h>

typedef struct sirCompressionDesc_s
{
	SIRResourceConstBytes source;
	SIRResourceMutableBytes destination;
	SIRResourceMutableBytes scratch;
	uint16_t nCodecVersion;
	uint32_t nDictionaryId;
} sirCompressionDesc_t;

#endif // __SIR_COMPRESSIONTYPES_H__
