struct v_PixelInput {
    float4 v_Position   : SV_POSITION;
    float4 v_Color      : COLOR;
};

float4 main( v_PixelInput input ) : SV_TARGET {
    return float4( input.v_Color );
}