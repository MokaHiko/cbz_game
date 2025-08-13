import cbz;

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
VOut vertexMain(Vin in)
{
    VOut output;
    output.position = float4(in.position.xy, 0.0f, 1.0f);
    output.uv = in.uv;
    return output;
}

[[vk_binding(cbz::TEXTURE0)]]
cbz::SampledTexture blitTexture;

[[vk_binding(cbz::TEXTURE1)]]
cbz::SampledTexture highlightTexture;

struct UBlitSettings {
    float4 settings; // x : exposure, y : gamma

    float exposure() { return settings.x;}
    float gamma() { return settings.y;}
};

ConstantBuffer<UBlitSettings> uBlitSettings;

// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
float3 aces(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

[shader("fragment")]
float4 fragmentMain(VOut in) : SV_Target
{
    float3 hdr_color = 0;
    if (length(blitTexture.sample(in.uv).xyz) > 0) {
        hdr_color = blitTexture.sample(in.uv).xyz;
    } else {
        return highlightTexture.sample(in.uv);
    }

    float3 mapped = aces(hdr_color * uBlitSettings.exposure());

    // Gamma correction if not srgb output/swapchain
    // const float gamma = uBlitSettings.gamma();
    // mapped = pow(mapped, float3(1.0 / gamma));

    // Clamp for unorm
    // mapped = clamp(pow(mapped, float3(1.0 / gamma)), 0.0, 1.0);

    return float4(mapped, 1.0f);
}