#include "include/common.hlsli"
RWStructuredBuffer<uint> outputData : register(u0);
[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) { outputData[tid.x] = (uint)sirFixtureColor().x; }
