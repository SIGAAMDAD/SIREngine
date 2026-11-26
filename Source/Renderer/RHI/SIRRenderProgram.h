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

#ifndef __SIR_RENDER_PROGRAM_H__
#define __SIR_RENDER_PROGRAM_H__

#include <Renderer/RHI/SIRRenderResource.h>

class SIRRenderProgram : public SIRRenderResource
{
public:
	SIRRenderProgram( const SIRRenderProgram& ) = delete;
	SIRRenderProgram( SIRRenderProgram&& ) = delete;
	SIRRenderProgram& operator=( const SIRRenderProgram& ) = delete;
	SIRRenderProgram& operator=( SIRRenderProgram&& ) = delete;
	virtual ~SIRRenderProgram() noexcept = default;

protected:
	explicit SIRRenderProgram( const SIRSharedPtr<SIRRenderDevice>& device ) noexcept
	    : SIRRenderResource( device )
	{
	}
};

#endif // __SIR_RENDER_PROGRAM_H__
