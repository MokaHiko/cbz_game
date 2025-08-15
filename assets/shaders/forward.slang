import cbz;
import cbz_graphics;
import cbz_graphics_pbr_forward;

struct Vin {
  float3 position;
  float3 normal;
  float4 tangent;
  float2 uv;
}

struct VOut {
  float4 position : SV_Position;

  float3 frag_position;
  float3 frag_light_pos;
  float3 normal;
  //float3x3 TBN;
  float3 tangentU;
  float3 tangentV;
  float2 shadow_map_uv;
  float2 uv;
}

struct UScene {
  cbz::Vec4 camera_position;
  //int point_light_count;
};

ConstantBuffer<UScene> uScene;
ConstantBuffer<cbz::DirectionalLight> uDirLight;

[shader("vertex")]
VOut vertexMain(Vin input, cbz::Draw draw) {
  VOut output;

  output.uv = input.uv;
  output.frag_position = mul(draw.model(), float4(input.position, 1.0)).xyz;
  output.position = mul(draw.viewProj(), float4(output.frag_position, 1.0f));

  output.normal = normalize( mul(transpose((float3x3)draw.modelInverse()), input.normal));
  float3 t = normalize(mul((float3x3)draw.model(), input.tangent.xyz));
  float3 b = input.tangent.w * cross(output.normal, t);
  //output.TBN = transpose(float3x3(t, b, output.normal));
  //output.TBN = float3x3(1.0f);

  output.frag_light_pos = uDirLight.getPosition();
  return output;
}

struct FOut {
  float4 color : SV_Target0;
  float4 bright : SV_Target1;
};

ConstantBuffer<cbz::ULitMaterial> material;

[shader("fragment")]
FOut fragmentMain(VOut input) {
  FOut out;

  cbz::SurfaceGeometry sg;
  sg.frag_position = input.frag_position;
  sg.frag_light_pos = input.frag_light_pos;
  sg.normal = input.normal;
  //sg.TBN = input.TBN;
  sg.tangentU = input.tangentU;
  sg.tangentV = input.tangentV;

  sg.shadow_map_uv = input.shadow_map_uv;
  sg.uv = input.uv;

  //sg = material.prepare_geometry(sg);
  cbz::IBRDF brdf = material.prepare(sg);
  float3 wo = normalize(uScene.camera_position.xyz - input.frag_position);

  float3 lo = 0;
  lo += uDirLight.illuminate(sg, brdf, wo);
  lo += material.ambient(sg, wo);
  lo += material.emissive(sg);
  out.color = float4(lo, 1.0f);

  float brightness = dot(out.color.rgb, float3(0.2126, 0.7152, 0.0722)) * length(material.emissive(sg));
  if (brightness > 1.0) {
    out.bright = float4(out.color.rgb, 1.0);
  } else {
    out.bright = 0;
  }

  return out;
}