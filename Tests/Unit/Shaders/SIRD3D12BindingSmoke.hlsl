RWBuffer<uint> output : register(u0, space0);

[numthreads(64, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    output[threadId.x] = 0x50000000u + 7u * threadId.x;
}
