// Headless D3D12 DXIL compute-path contract fixture.
// The root signature exposes a UAV table at u0 and two 32-bit constants at b1.
RWStructuredBuffer<uint> g_Values : register(u0);

cbuffer SirComputeParameters : register(b1)
{
    uint nCount;
    uint nSeed;
};

[numthreads(64, 1, 1)]
void main(uint3 nThreadId : SV_DispatchThreadID)
{
    if (nThreadId.x < nCount)
    {
        g_Values[nThreadId.x] = (nThreadId.x * 13u) ^ nSeed;
    }
}
