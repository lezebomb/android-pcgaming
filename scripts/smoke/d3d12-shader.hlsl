// Full-screen triangle: actual DXIL vertex/pixel execution, no vertex buffers.
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint id : SV_VertexID) {
    VertexOutput output;
    output.uv = float2((id << 1) & 2, id & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}

#ifdef DG_BC_TEXTURE_CHECK
Texture2D<float4> sourceTexture : register(t0);
SamplerState pointSampler : register(s0);
#endif
float4 PSMain(VertexOutput input) : SV_Target {
#ifdef DG_BC_TEXTURE_CHECK
    return sourceTexture.SampleLevel(pointSampler, input.uv, 0);
#else
    return float4(input.uv, 0.25, 1);
#endif
}
