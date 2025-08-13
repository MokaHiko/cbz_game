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
  nointerpolation uint entityId;
};

// Each entity id is a 32 bit uint
[vk_binding(cbz::BUFFER0)]
cbz::Buffer<cbz::UVec4> entityIds;

[shader("vertex")]
VOut vertexMain(Vin in, cbz::Draw draw) {
  VOut output;
  output.position = mul(draw.mvp(), float4(in.position.xyz, 1.0f));

  uint32_t bufferIdx = draw.id() / 4;
  uint32_t offset = draw.id() % 4;

  output.entityId = entityIds[bufferIdx][offset];
  return output;
}

[shader("fragment")]
cbz::UVec4 fragmentMain(VOut in) : SV_Target {
  return cbz::UVec4(in.entityId & 0xFF, (in.entityId >> 8) & 0xFF,
                in.entityId >> 16 & 0xFF, in.entityId >> 24 & 0xFF);
}