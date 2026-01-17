struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

cbuffer TransformBuffer
{
    float4x4 modelViewProj;
    float time;
};

// Vertex Shader
[shader("vertex")]
VertexOutput main(VertexInput input)
{
    VertexOutput output;
    
    // Simple wave effect using time
    float3 pos = input.position;
    pos.y += sin(pos.x * 2.0 + time) * 0.1;
    
    output.position = mul(modelViewProj, float4(pos, 1.0));
    output.color = input.color;
    
    return output;
}

// Fragment/Pixel Shader
[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    // Simple color output with slight color variation based on position
    float3 color = input.color;
    color *= (sin(input.position.x * 0.01) * 0.1 + 0.9);
    
    return float4(color, 1.0);
}