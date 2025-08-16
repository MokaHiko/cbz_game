#ifndef RENDERER_TYPES_H_
#define RENDERER_TYPES_H_

#include <cbz/cbz_defines.h>
#include <cbz_gfx/cbz_gfx_defines.h>

struct TextureRef {
  TextureRef() = default;
  ~TextureRef() = default;

  TextureRef(void *asset, cbz::ImageHandle imageHandle)
      : asset(asset), imgh(imageHandle) {}

  // private:
  void *asset = NULL; // Pointer to asset backing reference
  cbz::ImageHandle imgh = {CBZ_INVALID_HANDLE}; // Handle to image
  // TODO: Sampler type
};

struct MaterialPBRUniformData {
  float albedoFactor[4];
  float metallicFactor;
  float emissiveFactor[3];
  float roughnessFactor;
};

class MaterialPBRAsset;

typedef enum : uint8_t {
  MATERIAL_TEXTURE_PRIMARY = 0,
  MATERIAL_TEXTURE_MAX = 5,
} MaterialTextures;

typedef enum : uint32_t {
  MATERIAL_PBR_TEXTURE_ALBEDO = 0,
  MATERIAL_PBR_TEXTURE_METALLIC_ROUGHNESS,
  MATERIAL_PBR_TEXTURE_NORMAL,
  MATERIAL_PBR_TEXTURE_OCCLUSION,
  MATERIAL_PBR_TEXTURE_EMISSIVE,
  MATERIAL_PBR_TEXTURE_COUNT,
} MaterialPBRTextures;

// @brief A reference to a 'PrimitiveAsset'
struct Material {
  TextureRef textures[MATERIAL_TEXTURE_MAX];
  uint8_t textureCount;

  MaterialPBRUniformData uData;
  cbz::UniformHandle uh;

  cbz::GraphicsProgramHandle ph;
  MaterialPBRAsset *asset;
};

class PrimitiveAsset;

// @brief A reference to a 'PrimitiveAsset'
struct Primitive {
  cbz::VertexBufferHandle vbh;
  cbz::IndexBufferHandle ibh;

  Material materialRef;
  PrimitiveAsset *asset;
};

typedef enum : uint32_t {
  LIGHT_TYPE_DIRECTIONAL = 0,
  LIGHT_TYPE_POINT,
  LIGHT_TYPE_COUNT
} LightType;

// TODO : Make light sources/light env in ssbo
struct LightSource {
  struct {
    float lightSpaceMatrix[16];

    float position[3];
    uint32_t _uniform_padding1;

    float direction[3];
    uint32_t _uniform_padding2;

    float intensity[3] = {15, 15, 13};
    uint32_t _uniform_padding3;
  } properties;
  cbz::UniformHandle uh = {CBZ_INVALID_HANDLE};

  // shadowMapSize = windowResolution * shadowMapResolutionFactor
  float shadowMapResolutionFactor;
  cbz::ImageHandle shadowMap;
  uint8_t castShadows;

  LightType type;
};

typedef enum {
  CAMERA_TYPE_PERSPECTIVE = 0,
  CAMERA_TYPE_ORTHOGRAPHIC,
  CAMERA_TYPE_COUNT,
} CameraType;

struct Camera {
  float view[16];
  float proj[16];

  float position[4];
  float lookAt[4];

  float fov;
  float aspectRatio;
  float near;
  float far;

  CameraType type;
  cbz::UniformHandle uh;
};

struct Skybox {
  cbz::ImageHandle skyboxCubeMap;
  cbz::ImageHandle irradianceCubeMap;
  cbz::ImageHandle hdriMap;
};

#endif // RENDERER_TYPES_H_
