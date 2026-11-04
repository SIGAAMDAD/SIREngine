// Offscreen native D3D12 indexed draw fixture (DXIL VS/PS).
struct SirVertex
{
    float3 position : POSITION;
    float4 color : COLOR0;
};

struct SirPixel
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

SirPixel vsMain(SirVertex input)
{
    SirPixel output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 psMain(SirPixel input) : SV_TARGET0
{
    return input.color;
}
