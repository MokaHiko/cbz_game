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
  output.position = mul(draw.viewProj(), float4(in.position.xyz, 1.0f));
  return output;
}

[[vk_binding(cbz::TEXTURE0)]]
cbz::SampledTextureCube envCubeMap;

[shader("fragment")]
float4 fragmentMain(VOut in) : SV_Target {
  float3 worldUp = {0.0f, 1.0f, 0.0f};

  float3 normal = normalize(in.localPosition);
  float3 right = normalize(cross(worldUp, normal));
  float3 up = normalize(cross(normal, right));

  float sampleDelta = 0.25f;
  float sampleCount = 0.0f;

  float3 irradiance = {};
  for(float phi = 0.0f; phi < 2.0f * cbz::PI; phi += sampleDelta) {
    for(float theta = 0.0f; theta < cbz::PI / 2.0f; theta += sampleDelta) {
      // Spherical to cartesian
      float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

      // Tangent space to world space
      float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
      
      irradiance += envCubeMap.sample(sampleVec).xyz * cos(theta) * sin(theta);
      ++sampleCount;
    }
  }

  return float4(cbz::PI * irradiance / sampleCount, 1.0f);
}