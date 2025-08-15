import cbz;
import cbz_graphics;

// PBR Forward lit material

namespace cbz {

// --- TEXTURE SLOTS ---
[vk_binding(cbz::TEXTURE0)]
cbz::SampledTexture albedo;

[vk_binding(cbz::TEXTURE1)]
cbz::SampledTexture metallic_roughness_map;

[vk_binding(cbz::TEXTURE2)]
cbz::SampledTexture normal_map;

[vk_binding(cbz::TEXTURE3)]
cbz::SampledTexture occlusion_map;

[vk_binding(cbz::TEXTURE4)]
cbz::SampledTexture emissive_map;

[vk_binding(cbz::TEXTURE5)]
cbz::SampledTextureCube irradiance_map;

// --- PBR ---
public struct CookTorrenceBRDF : IBRDF {
  public float3 albedo;
  public float metallic;
  public float roughness;

  public float3 evaluate(float3 wo, float3 wi, float3 n) {
    float3 h = normalize(wo + wi);

    // Specular factor
    float3 f0 = float3(0.04, 0.04, 0.04); // surface reflection at 0
    f0 = lerp(f0, albedo, metallic);
    float3 f = fresnel_schlick(max(dot(h, wo), 0.0), f0);

    float ndf = distribution_GGX(n, h, roughness);
    float g = geometry_smith(n, wo, wi, roughness);

    float3 numerator = ndf * g * f;
    float denominator =
        4 * max(dot(n, wo), 0.0) * max(dot(n, wi), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float3 ks = f;
    float3 kd = float3(1.0f, 1.0f, 1.0f) - ks;
    kd *= (1.0f - metallic);

    float lambertian_factor = max(dot(n, wi), 0.0f);
    return (kd * albedo / PI + specular) * lambertian_factor;
  }

  float3 fresnel_schlick(float cos_theta, float3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
  }

  public float3 fresnel_schlick_roughness(float cos_theta, float3 f0, float roughness) {
    return f0 + (max(float3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
  }

  float distribution_GGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
  }

  float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
  }

  float geometry_smith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
  }
};

public struct ULitMaterial : cbz::IMaterial {
  float3 albedo_factor;
  float metallic_factor;
  float roughness_factor;
  float ao_factor;
  float normal_factor;
  float3 emissive_factor;
  float emissive_strength;

  public typedef cbz::CookTorrenceBRDF BRDF;

  public cbz::SurfaceGeometry prepare_geometry(cbz::SurfaceGeometry sg) {
    // TODO: Calculate tangets for normal mapping
    // float3 n = normal_map.sample(sg.uv).xyz * 2.0 - 1.0;
    // sg.normal = normalize(mul(sg.TBN, n));
    return sg;
  }

  public cbz::CookTorrenceBRDF prepare(cbz::SurfaceGeometry sg) {
    cbz::CookTorrenceBRDF brdf;

    brdf.albedo = albedo.sample(sg.uv).rgb;
    brdf.albedo *= albedo_factor;

    brdf.metallic = metallic_roughness_map.sample(sg.uv).b;
    brdf.roughness = metallic_roughness_map.sample(sg.uv).g;

    return brdf;
  }

  public float3 emissive(cbz::SurfaceGeometry sg) {
    return emissive_map.sample(sg.uv).rgb * emissive_factor * emissive_strength;
  }

  public float3 ambient(cbz::SurfaceGeometry sg, float3 wo) {
    float3 ambientAlbedo = albedo.sample(sg.uv).rgb;
    ambientAlbedo *= albedo_factor;
 
    float ambientRoughness = metallic_roughness_map.sample(sg.uv).g;

    float3 f0 = float3(0.04, 0.04, 0.04);
    float3 ks = fresnel_schlick_roughness(max(dot(sg.normal, wo), 0.0), f0, ambientRoughness);
    float3 kd = 1.0f - ks;
    float3 irradiance = irradiance_map.sample(sg.normal).rgb;

    float3 diffuse = irradiance * ambientAlbedo;
    float ao = occlusion_map.sample(sg.uv).r * ao_factor;

    return kd * diffuse * ao;
  }

  private float3 fresnel_schlick_roughness(float cos_theta, float3 f0, float roughness) {
    return f0 + (max(float3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
  }
};

}
