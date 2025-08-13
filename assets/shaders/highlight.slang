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
}

[shader("vertex")]
VOut vertexMain(Vin input, cbz::Draw draw) {
  VOut output;
  output.position = mul(draw.mvp(), float4(input.position, 1.0f));
  return output;
}

[shader("fragment")]
float4 fragmentMain(VOut in) : SV_Target
{
    return float4(1.0f);
}