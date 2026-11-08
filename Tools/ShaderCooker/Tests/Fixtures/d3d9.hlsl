float4 VSMain(float4 position : POSITION) : POSITION
{
    return position;
}

float4 PSMain() : COLOR0
{
    return float4(0.3f, 0.6f, 0.9f, 1.0f);
}
