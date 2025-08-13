import cbz;
import cbz_graphics;

struct Vin {
  float3 position;
  float2 uv;
}

struct VOut {
  float4 position : SV_Position;
  float2 uv;
};

[shader("vertex")]
VOut vertexMain(Vin in, cbz::Draw draw) {
  VOut output;
  output.position = float4(in.position.xyz, 1.0f);
  output.uv = in.uv;

  return output;
}

[[vk_binding(cbz::TEXTURE0)]] 
cbz::SampledTexture gPosition;

[[vk_binding(cbz::TEXTURE1)]] 
cbz::SampledTexture gNormal;

[[vk_binding(cbz::TEXTURE2)]] 
cbz::SampledTexture gAlbedoSpec;

struct FOut {
  float4 color : SV_Target0;
}

// [shader("fragment")]
// FOut fragmentMain(VOut input) {
//   FOut out;

//   float3 wo = normalize(scene.camera_position - input.sg.frag_position);

//   float3 lo = 0;
//   lo += dirLight.illuminate(sg, brdf, wo);

//   for (int i = 0; i < scene.point_light_count; i++) {
//     lo += pointLights[i].illuminate(sg, brdf, wo);
//   }

//   lo += material.ambient(sg);
//   lo += material.emissive(sg);
//   out.color = float4(lo, 1.0f);

//   float brightness = dot(out.color.rgb, float3(0.2126, 0.7152, 0.0722)) * length(material.emissive(sg));
//   if (brightness > 1.0) {
//     out.bright = float4(out.color.rgb, 1.0);
//   } else {
//     out.bright = 0;
//   }

//   return out;
// }