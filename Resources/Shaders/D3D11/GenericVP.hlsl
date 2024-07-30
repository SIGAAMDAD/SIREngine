struct a_VertexInput {
    float3 a_Position   : POSITION;
    float2 a_TexCoords  : TEXCOORD;
    float4 a_Color      : COLOR;
};

struct v_PixelInput {
    float4 v_Position   : SV_POSITION;
    float4 v_Color      : COLOR;
};

cbuffer u_VertextInput : register( b0 ) {
    matrix u_ModelViewProjection;
};

v_PixelInput main( a_VertexInput input ) {
    v_PixelInput output;

    float4 pos = float4( input.a_Position, 1.0f );

    pos = mul( u_ModuleViewProjection );
    output.v_Position = pos;

    ouptut.v_Color = input.a_Color;

    return output;
}