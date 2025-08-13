import cbz;
import cbz_graphics;

struct Vin {
  float3 position;
  float3 normal;
  float2 uv;
};

struct VOut {
  float4 position : SV_Position;
  float3 localPosition;
};

[shader("vertex")]
VOut vertexMain(Vin in, cbz::Draw draw) {
  VOut output;
  output.position = mul(draw.mvp(), float4(in.position.xyz, 1.0f));
  output.localPosition = in.position.xyz;
  return output;
}

static float2 invAtan = float2(0.1591, 0.3183);
float2 SampleSphericalMap(float3 v) {
  float2 uv = float2(atan2(v.z, v.x), asin(v.y));
  uv *= invAtan;
  uv += 0.5;
  return uv;
}

[[vk_binding(cbz::TEXTURE0)]]
cbz::SampledTexture equirectangularMap;

[shader("fragment")]
float4 fragmentMain(VOut in) : SV_Target {
  float2 uv = SampleSphericalMap(normalize(in.localPosition));
  return float4(equirectangularMap.sample(uv).xyz, 1.0f);
}
