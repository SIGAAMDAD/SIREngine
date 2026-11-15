/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRPACKCODEC_H__
#define __SIRENGINE_SIRPACKCODEC_H__

#include <Resources/SIRPackFormat.h>

// A decoded destination is caller-owned and never internally allocated.
// Zstd's decoding context is constructed in caller-owned aligned scratch;
// LZ4 and NONE require none. Dictionaries are unsupported in V1.
uint64_t sirPackDecodeScratchBytes( sirPackCodec_t eCodec ) noexcept;
sirPackResult_t sirPackDecode( const sirPackChunk_t& chunk, const void *pEncoded, uint64_t nEncodedBytes, void *pDecoded,
                               uint64_t nDecodedCapacity, void *pScratch, uint64_t nScratchBytes ) noexcept;

// Offline cooker helper. This may use the codec's offline encoder allocation
// policy; it never runs in the streaming runtime or on an engine frame thread.
sirPackResult_t sirPackEncodeOffline( sirPackCodec_t eCodec, const void *pDecoded, uint64_t nDecodedBytes, void *pEncoded,
                                      uint64_t nEncodedCapacity, uint64_t *pEncodedBytes ) noexcept;

#endif // __SIRENGINE_SIRPACKCODEC_H__
