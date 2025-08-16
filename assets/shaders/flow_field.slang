import cbz;
import cbz_graphics;

struct Vin
{
    float3 position;
    float2 uv;
};

struct VOut
{
    float4 position : SV_Position;
    float2 uv;
};

[shader("vertex")]
VOut vertexMain(Vin in, cbz::Draw draw)
{
    VOut output;
    output.position = mul(draw.mvp(), float4(in.position, 1.0f));
    output.uv = in.uv;
    return output;
}

[[vk_binding(cbz::TEXTURE0)]]
cbz::SampledTexture flowFieldTexture;

[shader("fragment")]
float4 fragmentMain(VOut in) : SV_Target
{
    return flowFieldTexture.sample(in.uv);
}