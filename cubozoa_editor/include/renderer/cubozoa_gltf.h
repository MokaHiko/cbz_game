#ifndef CBZ_GLTF_H_
#define CBZ_GLTF_H_

#include "renderer/cubozoa_render_types.h"
#include <cbz_gfx/cbz_gfx.h>

struct Camera;
struct Primitive;
class Transform;
class StaticMeshRenderer {
public:
  StaticMeshRenderer();
  ~StaticMeshRenderer();

  // TODO: Change with light environment
  void render(const Camera *camera, const LightSource *lightSources,
              uint32_t count, Transform *transform, Primitive *mesh);

private:
  cbz::ShaderHandle mPBRForwardShader;
  cbz::GraphicsProgramHandle mPBRForwardProgram;
  cbz::ShaderHandle mGBufferShader;
  cbz::GraphicsProgramHandle mGBufferProgram;
};

// TODO: move to asset/entity creation
#include <cbz/cbz_asset.h>
#include <cbz_ecs/cbz_ecs_types.h>

struct Gltf;
[[nodiscard]] extern std::unique_ptr<Asset<Gltf>>
GltfAssetCreate(const std::string &path);

[[nodiscard]] extern cbz::ecs::Entity
EntityCreateFromGltf(cbz::ecs::IWorld *world, Asset<Gltf> *asset);

typedef uint32_t MaterialHandle;

#include <sol/sol.hpp>

struct TextureRef;
struct MaterialPBR;
class AssetManager {
public:
  AssetManager(sol::state &luaState);
  ~AssetManager();

  [[nodiscard]] MaterialHandle loadMaterial(const std::string &path);
  [[nodiscard]] TextureRef loadTexture(const std::string &path,
                                       CBZTextureFormat format);

private:
  sol::state &mLua;
};

#endif // CBZ_GLTF_H_
