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

#ifndef __SIR_COMPRESSIONREGISTRY_H__
#define __SIR_COMPRESSIONREGISTRY_H__

#include <Streaming/Pack/SIRCompressionTypes.h>
#include <Core/Compression/SIRCompressionCodec.h>

constexpr uint32_t SIR_PACK_MAX_CODECS = 8;

class SIRCompressionRegistry final
{
public:
	SIRCompressionRegistry( void ) noexcept;
	SIRPackResult registerCodec( SIRCompressionCodec& codec ) noexcept;
	const SIRCompressionCodec *find( sirPackCodec_t codec ) const noexcept;
	SIRPackResult decompress( sirPackCodec_t codec, const sirCompressionDesc_t& desc ) const noexcept;
	void freeze( void ) noexcept;
	bool isFrozen( void ) const noexcept;

private:
	SIRCompressionCodec *m_pCodecs[SIR_PACK_MAX_CODECS];
	uint32_t m_nCount;
	bool m_bFrozen;
};

#endif // __SIR_COMPRESSIONREGISTRY_H__
