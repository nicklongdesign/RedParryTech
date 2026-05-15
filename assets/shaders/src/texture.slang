// Vertex shader input
struct VertexInput {
    float2 position : POSITION;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// Vertex shader output
struct VertexOutput {
    float4 position : SV_Position;
    float3 fragColor : COLOR0;
    float2 uv : TEXCOORD0;
};

// Uniform buffer (matches GLSL's UniformBufferObject)
struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

struct FragmentOutput {
    float4 color : SV_Target0;
};

struct Tex {
    Sampler2D sample;
}

ParameterBlock<Tex> tex;

[shader("vertex")]
VertexOutput vMain(
    uniform UniformBufferObject ubo,
    VertexInput input) 
{
    VertexOutput output;
    
    // Matrix multiplication (note: mul() is column-major by default)
    output.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.position, 0.0, 1.0))));
    output.fragColor = input.color;
    output.uv = input.uv;
    
    return output;
}

[shader("fragment")]
FragmentOutput fMain(VertexOutput input)
{
    FragmentOutput out;
    uint status = 0;
    out.color = tex.sample.Sample(input.uv, int2(0), 0, status);

    return out;
}