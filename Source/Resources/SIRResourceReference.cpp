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

#include "SIRResourceReference.h"

SIRResourceResult SIRResourceAcquireReference( SIRResourceGraph& rGraph, SIRResourceHandle hResource, SIRResourceKind eKind ) noexcept
{
	return rGraph.acquireReference( hResource, eKind );
}

void SIRResourceReleaseReference( SIRResourceGraph& rGraph, SIRResourceHandle hResource ) noexcept
{
	rGraph.releaseReference( hResource );
}

bool SIRResourceIsReferenceAlive( const SIRResourceGraph& rGraph, SIRResourceHandle hResource ) noexcept
{
	return rGraph.isReferenceAlive( hResource );
}
