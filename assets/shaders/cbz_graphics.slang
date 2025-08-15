import cbz;

namespace cbz {
// --- TRANSFORMS ---
public struct TransformData {
  float4x4 model;
  float4x4 view;
  float4x4 proj;

  float4x4 model_inv;
  float4x4 view_inv;
  float4x4 proj_inv;
};

[[vk_binding(GLOBAL_TRANSFORM_BUFFER)]]
StructuredBuffer<TransformData> gTransforms;

public struct Draw {
  public float4x4 mvp() { return mul(proj(), mul(view(), model())); }

  public float4x4 viewProj() { return mul(proj(), view()); }

  public float4x4 model() { return gTransforms[_cbzDrawID].model; }
  public float4x4 modelInverse() { return gTransforms[_cbzDrawID].model_inv; }

  public float4x4 view() { return gTransforms[_cbzDrawID].view; }

  public float4x4 proj() { return gTransforms[_cbzDrawID].proj; }

  public uint id() { return _cbzDrawID; }

  private uint _cbzDrawID : SV_InstanceID;
}

// --- MATERIAL SYSTEM ---
public struct SurfaceGeometry {
  public float3 frag_position;
  public float3 frag_light_pos;

  public float3 normal;
  public float3x3 TBN;

  public float3 tangentU;
  public float3 tangentV;

  public float2 shadow_map_uv;
  public float2 uv;
};

public interface IBRDF {
  // Technically, a BRDF is only a function of the incident
  // (`wi`) and exitant (`wo`) directions, but for simplicity
  // we are passing in the surface normal (`N`) as well.
  //
  float3 evaluate(float3 wo, float3 wi, float3 N);
};

// It is important to note that a reflectance function is *not*
// a "material." In most cases, a material will have spatially-varying
// properties so that it cannot be summarized as a single `IBRDF`
// instance.
//
// Thus a "material" is a value that can produce a BRDF for any point
// on a surface (e.g., by sampling texture maps, etc.).
//
public interface IMaterial {
  public associatedtype BRDF : IBRDF;

  // For our simple example program, it is enough for a material to
  // be able to return a BRDF given a point on the surface.
  //
  // A more complex implementation of material shading might also
  // have the material return updated surface geometry to reflect
  // the result of normal mapping, occlusion mapping, etc. or
  // return an opacity/coverage value for partially transparent
  // surfaces.
  //
  public BRDF prepare(SurfaceGeometry sg);
  public SurfaceGeometry prepare_geometry(SurfaceGeometry sg);

  public float3 emissive(SurfaceGeometry sg);
  public float3 ambient(SurfaceGeometry sg, float3 wo);
};

// A light, or an entire lighting *environment* is an object
// that can illuminate a surface using some BRDF implemented
// with our abstractions above.
//
public interface ILightEnv {
  // The `illuminate` method is intended to integrate incoming
  // illumination from this light (environment) incident at the
  // surface point given by `g` (which has the reflectance function
  // `brdf`) and reflected into the outgoing direction `wo`.
  //
  float3 illuminate<B : IBRDF>(SurfaceGeometry g, B brdf, float3 wo);
  //
  // Note that the `illuminate()` method is allowed as an interface
  // requirement in Slang even though it is a generic. Contrast that
  // with C++ where a `template` method cannot be `virtual`.
};

// Given the `ILightEnv` interface, we can write up almost textbook
// definition of directional and point lights.

public struct DirectionalLight : ILightEnv {
  float4x4 lightSpaceMatrix;
  float4 position;
  float4 direction;
  float4 intensity;

  public float3 getPosition() {
    return position.xyz;
  };

  // public Sampler2D shadow_map;

  public float3 illuminate<B : IBRDF>(SurfaceGeometry g, B brdf, float3 wo) {
    // Shadow
    float4 frag_pos_light_space =
        mul(lightSpaceMatrix, float4(g.frag_position, 1.0f));

    // perform perspective divide
    float3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

    // Store before normalizing
    // get depth of current fragment from light's perspective // TODO: VULKAN
    // ALREADY [0,1]
    float current_depth = proj_coords.z;

    // get closest depth value from light's perspective (using [0,1] range
    // fragPosLight as coords)
    proj_coords = proj_coords * 0.5f + 0.5f;

    float shadow = 0.0f;

    uint width, height, levels;
    // shadow_map.GetDimensions(0, width, height, levels);
    // float2 texel_size = 1.0f / float2(width, height);
    // for (int x = -1; x <= 1; ++x) {
    //   for (int y = -1; y <= 1; ++y) {
    //     float pcf_depth = shadow_map .Sample(float2(proj_coords.x, 1 -
    //     proj_coords.y) + texel_size * float2(x, y)) .r; float bias = 0.0005f;
    //     shadow += current_depth - bias > pcf_depth ? 1.0f : 0.0f;
    //   }
    // }
    shadow /= 9.0f;

    if (current_depth > 1.0f) {
      shadow = 0.0f;
    }

    return intensity.rgb * brdf.evaluate(wo, -direction.xyz, g.normal) *
           (1.0f - shadow);
  }
};

public struct PointLight : ILightEnv {
  float4 position;
  float4 intensity;

  public float3 illuminate<B : IBRDF>(SurfaceGeometry g, B brdf, float3 wo) {
    float3 delta = position.xyz - g.frag_position;
    float d = length(delta);
    float3 direction = normalize(delta);
    float3 illuminance = intensity.rgb / (d * d);
    return illuminance * brdf.evaluate(wo, -direction, g.normal);
  }
};

} // namespace cbz