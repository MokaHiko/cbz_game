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

  output.localPosition = in.position.xyz;

  float4 clipPos = mul(draw.viewProj(), float4(in.position.xyz, 1.0f));
  output.position = clipPos.xyww;
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
cbz::SampledTextureCube skyBox;

[shader("fragment")]
float4 fragmentMain(VOut in) : SV_Target {
  return float4(skyBox.sample(in.localPosition).xyz, 1.0f);
}
