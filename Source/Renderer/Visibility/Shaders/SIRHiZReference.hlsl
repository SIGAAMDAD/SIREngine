/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

// The CPU SIRHiZDispatchPlan selects the source/destination mip views and
// provides SIRHiZParameters. Each dispatch writes one R32_FLOAT mip.
// The native device must order storage writes before subsequent mip reads.
// The mip-0 copy accepts a window into a larger single-sample depth source;
// reduction passes read only the immediately preceding R32_FLOAT mip.

cbuffer SIRHiZParameters : register(b0)
{
    uint2 nSourceExtent;
    uint2 nDestinationExtent;
    uint2 nSourceOrigin;
    uint2 nReserved;
};

Texture2D<float> g_Source : register(t0);
RWTexture2D<float> g_Destination : register(u0);

// Invalid source values fail open to reverse-Z far/clear depth.
// On the valid [0,1] domain, these kernels match the HIZ-00 CPU oracle.
float sirHiZConservativeDepth(float fDepth)
{
    return (fDepth >= 0.0f && fDepth <= 1.0f) ? fDepth : 0.0f;
}

[numthreads(8, 8, 1)]
void SIRHiZCopy(uint3 nDispatchId : SV_DispatchThreadID)
{
    uint2 nPixel = nDispatchId.xy;
    if (any(nPixel >= nDestinationExtent))
    {
        return;
    }
    // Check the origin before subtracting it to avoid unsigned wrap.
    if (any(nSourceOrigin >= nSourceExtent) ||
        any(nDestinationExtent > nSourceExtent - nSourceOrigin))
    {
        g_Destination[nPixel] = 0.0f;
        return;
    }

    g_Destination[nPixel] = sirHiZConservativeDepth(g_Source.Load(int3(nPixel + nSourceOrigin, 0)));
}

[numthreads(8, 8, 1)]
void SIRHiZReduce(uint3 nDispatchId : SV_DispatchThreadID)
{
    uint2 nPixel = nDispatchId.xy;
    if (any(nPixel >= nDestinationExtent))
    {
        return;
    }

    // HIZ-00 uses ceil(source / 2), including odd-sized trailing children.
    // A mismatched mip chain is filled with far depth (no false occlusion).
    uint2 nExpectedExtent = (nSourceExtent / 2) + (nSourceExtent % 2);
    if (any(nSourceExtent == 0) || any(nSourceOrigin != 0) ||
        any(nExpectedExtent != nDestinationExtent))
    {
        g_Destination[nPixel] = 0.0f;
        return;
    }

    uint2 nFirstChild = nPixel * 2;
    float fMinimum = 1.0f;

    [unroll]
    for (uint iY = 0; iY < 2; ++iY)
    {
        [unroll]
        for (uint iX = 0; iX < 2; ++iX)
        {
            uint2 nChild = nFirstChild + uint2(iX, iY);
            if (all(nChild < nSourceExtent))
            {
                float fDepth = sirHiZConservativeDepth(g_Source.Load(int3(nChild, 0)));
                fMinimum = min(fMinimum, fDepth);
            }
        }
    }

    g_Destination[nPixel] = fMinimum;
}
