import cbz;
import cbz_graphics;

struct Vin {
  float3 position;
  float3 normal;
  float4 tangent;
  float2 uv;
}

struct VOut {
  float4 position : SV_Position;

  // World space normal
  float3 fragPosition;
  float3 normal;

  float2 uv;
};

[shader("vertex")]
VOut vertexMain(Vin in, cbz::Draw draw) {
  VOut output;
  output.fragPosition = mul(draw.model(), float4(in.position.xyz, 1.0f)).xyz;
  output.position = mul(draw.viewProj(), float4(output.fragPosition, 1.0f));

  output.normal = normalize(mul(transpose((float3x3)draw.modelInverse()), in.normal));
  output.uv = in.uv;

  return output;
}

struct FOut {
  // World space
  float4 gPosition : SV_Target0;
  float4 gNormal : SV_Target1;

  float4 gAlbedoSpec : SV_Target2;
};

[[vk_binding(cbz::TEXTURE0)]] 
cbz::SampledTexture albedo;

[shader("fragment")]
FOut fragmentMain(VOut in) {
  FOut out;
  out.gPosition = float4(in.fragPosition.xyz, 1.0f);
  out.gNormal = float4(in.normal, 1.0f);
  out.gAlbedoSpec = albedo.sample(in.uv);
  return out;
}
