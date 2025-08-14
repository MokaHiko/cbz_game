#include "renderer/cubozoa_gltf.h"

#include "renderer/cubozoa_render_types.h"

#include <cbz/cbz_asset.h>
#include <cbz_gfx/cbz_gfx.h>

#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

static std::vector<std::unique_ptr<Asset<MaterialPBR>>> sMaterials;
static std::unordered_map<std::string, std::unique_ptr<Asset<TextureRef>>>
    sTextures;

class TextureAsset : public Asset<TextureRef> {
public:
  TextureAsset(const std::string &path, cbz::ImageHandle imgh)
      : Asset(path), mImgh(imgh) {}
  ~TextureAsset() { cbz::ImageDestroy(mImgh); };

  TextureRef makeRef() override {
    ++mReferenceCount;
    return {this, mImgh};
  }

private:
  cbz::ImageHandle mImgh = {CBZ_INVALID_HANDLE}; // Handle to image
  uint32_t mReferenceCount;
};

class MaterialPBRAsset : public Asset<MaterialPBR> {
public:
  MaterialPBRAsset(const std::string &name) : Asset(name) {};

  MaterialPBR makeRef() override {
    ++mReferenceCount;

    if (mUH.idx == CBZ_INVALID_HANDLE) {
      mUH = cbz::UniformCreate("material", CBZ_UNIFORM_TYPE_VEC4, 3);
    }

    // TODO: Keep track of reference ptrs incase of material mutation.
    return {uData,           mUH,
            albedoTexture,   metallicRoughnessTexture,
            normalTexture,   occlusionTexture,
            emissiveTexture, this};
  }

  cbz::Result load() override { return cbz::Result::eSuccess; }

  // Uniform data
  MaterialPBRUniformData uData;

  // PBR
  TextureRef albedoTexture;
  TextureRef metallicRoughnessTexture;

  // Material
  TextureRef normalTexture;
  TextureRef occlusionTexture;

  TextureRef emissiveTexture;

private:
  cbz::UniformHandle mUH = {CBZ_INVALID_HANDLE};
  uint32_t mReferenceCount;
};

class PrimitiveAsset : public Asset<Primitive> {
public:
  PrimitiveAsset(const char *path) : Asset(path) {};
  ~PrimitiveAsset() {};

  cbz::VertexBufferHandle vbh;
  cbz::IndexBufferHandle ibh;

  MaterialPBR materialRef;

  Primitive makeRef() override {
    ++mReferenceCount;
    return {vbh, ibh, materialRef, this};
  };

private:
  std::string mName;
  uint32_t mReferenceCount;
};

// Load meshes
struct MeshReference {
  MeshReference() = default;
  MeshReference(MeshReference &&) = default;
  MeshReference &operator=(MeshReference &&) = default;

  ~MeshReference() {
    for (PrimitiveAsset &primitive : primitives) {
      // Destroy owned resources
      cbz::VertexBufferDestroy(primitive.vbh);
      cbz::IndexBufferDestroy(primitive.ibh);

      // Empty reference
      primitive.materialRef = {};
    }
  }

  std::vector<PrimitiveAsset> primitives;
};

// -- Default Resources
cbz::ImageHandle sWhiteIMGH;
cbz::ImageHandle sBlackIMGH;
cbz::ImageHandle sDefaultNormalIMGH;

struct Gltf {
public:
  int _;
};

class GltfAsset : public Asset<Gltf> {
public:
  struct Node {
    // x, y , z
    float translation[3];
    int parentIndex;
    int index;

    // w , x, y , z
    float rotation[4];

    // x, y , z
    float scale[3];

    CBZBool32 hasMesh;
    uint32_t meshIndex;

    std::string name;
  };

  std::vector<uint32_t> rootIndices;
  std::vector<Node> nodes;

  std::vector<MeshReference> meshes;
  std::vector<MaterialPBRAsset> materials;
  std::vector<cbz::ImageHandle> images;

  GltfAsset(std::filesystem::path path) : Asset(path) {};

  ~GltfAsset() {
    for (cbz::ImageHandle imgh : images) {
      cbz::ImageDestroy(imgh);
    }
  };

  cbz::Result load() override {
    // Creates a Parser instance. Optimally, you should reuse this across loads,
    // but don't use it across threads. To enable extensions, you have to pass
    // them into the parser's constructor.
    static fastgltf::Parser parser;

    // The GltfDataBuffer class contains static factories which create a buffer
    // for holding the glTF data. These return Expected<GltfDataBuffer>, which
    // can be checked if an error occurs. The parser accepts any subtype of
    // GltfDataGetter, which defines an interface for reading chunks of the glTF
    // file for the Parser to handle. fastgltf provides a few predefined classes
    // which inherit from GltfDataGetter, so choose whichever fits your usecase
    // the best.
    auto data = fastgltf::GltfDataBuffer::FromPath(getPath());
    if (data.error() != fastgltf::Error::None) {
      // The file couldn't be loaded, or the buffer could not be allocated.
      return cbz::Result::eFileError;
    }

    auto gltfOptions = fastgltf::Options::LoadExternalBuffers |
                       fastgltf::Options::GenerateMeshIndices |
                       fastgltf::Options::DecomposeNodeMatrices |
                       fastgltf::Options::LoadExternalImages;

    // This loads the glTF file into the gltf object and parses the JSON.
    // It automatically detects whether this is a JSON-based or binary glTF.
    // If you know the type, you can also use loadGltfJson or loadGltfBinary.
    auto asset =
        parser.loadGltf(data.get(), getPath().parent_path(), gltfOptions);
    if (auto error = asset.error(); error != fastgltf::Error::None) {
      // Some error occurred while reading the buffer, parsing the JSON, or
      // validating the data.
      return cbz::Result::eFileError;
    }

#ifdef CBZ_DEBUG
    if (auto error = fastgltf::validate(asset.get());
        error != fastgltf::Error::None) {
      spdlog::error("Invalid gltf!");
      return cbz::Result::eFileError;
    }
#endif // CBZ_DEBUG

    // Reserve space for images
    images.resize(asset->images.size());

    // Load materials
    for (const auto &fastGLTFMaterial : asset->materials) {
      spdlog::trace("Material: '{}' : ", fastGLTFMaterial.name);
      const fastgltf::PBRData &pbrData = fastGLTFMaterial.pbrData;
      MaterialPBRAsset &material = materials.emplace_back(
          MaterialPBRAsset(fastGLTFMaterial.name.c_str()));

      material.uData.albedoFactor[0] = pbrData.baseColorFactor.x();
      material.uData.albedoFactor[1] = pbrData.baseColorFactor.y();
      material.uData.albedoFactor[2] = pbrData.baseColorFactor.z();
      material.uData.albedoFactor[3] = pbrData.baseColorFactor.w();

      material.uData.metallicFactor = pbrData.metallicFactor;
      material.uData.roughnessFactor = pbrData.roughnessFactor;

      if (pbrData.baseColorTexture.has_value()) {
        const fastgltf::Texture &texture =
            asset->textures[pbrData.baseColorTexture->textureIndex];

        spdlog::trace(" - Albedo Texture: '{}' : ", texture.name);
        material.albedoTexture =
            TextureRef(this, loadTexture(&asset.get(), &texture,
                                         CBZ_TEXTURE_FORMAT_RGBA8UNORMSRGB));
      } else {
        material.albedoTexture = TextureRef(this, sWhiteIMGH);
      }

      if (pbrData.metallicRoughnessTexture.has_value()) {
        const fastgltf::Texture &texture =
            asset->textures[pbrData.metallicRoughnessTexture->textureIndex];

        spdlog::trace(" - MetallicRoughness Texture: '{}' : ", texture.name);
        material.metallicRoughnessTexture =
            TextureRef(this, loadTexture(&asset.get(), &texture,
                                         CBZ_TEXTURE_FORMAT_RGBA8UNORM));
      } else {
        material.metallicRoughnessTexture = TextureRef(this, sWhiteIMGH);
      }

      if (fastGLTFMaterial.normalTexture.has_value()) {
        const fastgltf::Texture &texture =
            asset->textures[fastGLTFMaterial.normalTexture->textureIndex];

        spdlog::trace(" - Normal Texture: '{}' : ", texture.name);
        material.normalTexture =
            TextureRef(this, loadTexture(&asset.get(), &texture,
                                         CBZ_TEXTURE_FORMAT_RGBA8UNORM));
      } else {
        material.normalTexture = TextureRef(this, sDefaultNormalIMGH);
      }

      if (fastGLTFMaterial.occlusionTexture.has_value()) {
        const fastgltf::Texture &texture =
            asset->textures[fastGLTFMaterial.occlusionTexture->textureIndex];

        spdlog::trace(" - OcclusionTexture Texture: '{}' : ", texture.name);
        material.occlusionTexture =
            TextureRef(this, loadTexture(&asset.get(), &texture,
                                         CBZ_TEXTURE_FORMAT_RGBA8UNORM));
      } else {
        material.occlusionTexture = TextureRef(this, sWhiteIMGH);
      }

      material.uData.emissiveFactor[0] = fastGLTFMaterial.emissiveFactor.x();
      material.uData.emissiveFactor[1] = fastGLTFMaterial.emissiveFactor.y();
      material.uData.emissiveFactor[2] = fastGLTFMaterial.emissiveFactor.z();

      if (fastGLTFMaterial.emissiveTexture.has_value()) {
        const fastgltf::Texture &texture =
            asset->textures[fastGLTFMaterial.emissiveTexture->textureIndex];

        spdlog::trace(" - emissiveTexture Texture: '{}' : ", texture.name);
        material.emissiveTexture =
            TextureRef(this, loadTexture(&asset.get(), &texture,
                                         CBZ_TEXTURE_FORMAT_RGBA8UNORMSRGB));
      } else {
        material.emissiveTexture = TextureRef(this, sBlackIMGH);
      }
    }

    for (auto &fastgltfMesh : asset->meshes) {
      MeshReference &outMesh = meshes.emplace_back();

      spdlog::trace("Mesh: '{}' : ", fastgltfMesh.name);
      spdlog::trace(" - {} primitives ", fastgltfMesh.primitives.size());

      // outMesh.primitives.resize(fastgltfMesh.primitives.size());
      for (auto it = fastgltfMesh.primitives.begin();
           it != fastgltfMesh.primitives.end(); ++it) {
        PrimitiveAsset &primitive =
            outMesh.primitives.emplace_back(fastgltfMesh.name.c_str());

        auto *positionIt = it->findAttribute("POSITION");

        // A mesh primitive is required to hold the POSITION attribute.
        if (positionIt == it->attributes.end()) {
          spdlog::error("Unable to load mesh {}!", fastgltfMesh.name.c_str());
          continue;
        }

        assert(it->indicesAccessor
                   .has_value()); // We specify GenerateMeshIndices, so we
                                  // should always have indices return true;
                                  //
        switch (it->type) {
        case fastgltf::PrimitiveType::Triangles: {

        } break;

        case fastgltf::PrimitiveType::Points:
        case fastgltf::PrimitiveType::Lines:
        case fastgltf::PrimitiveType::LineLoop:
        case fastgltf::PrimitiveType::LineStrip:
        case fastgltf::PrimitiveType::TriangleStrip:
        case fastgltf::PrimitiveType::TriangleFan: {
          spdlog::error(" - Unsupported primitive type!");
          continue;
        } break;
        }

        struct VertexPBR {
          float position[3];
          float normal[3];
          float tangent[4];
          float uv[2];
        };

        std::vector<VertexPBR> vertices;

        // POSITION
        const fastgltf::Accessor &positionAccessor =
            asset->accessors[positionIt->accessorIndex];

        vertices.resize(positionAccessor.count);

        if (!positionAccessor.bufferViewIndex.has_value())
          continue;

        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
            asset.get(), positionAccessor,
            [&](fastgltf::math::fvec3 pos, std::size_t idx) {
              vertices[idx].position[0] = pos.x();
              vertices[idx].position[1] = pos.y();
              vertices[idx].position[2] = pos.z();
            });

        if (it->materialIndex.has_value()) {
        }

        // NORMAL
        if (const auto *normal = it->findAttribute("NORMAL");
            normal != it->attributes.end()) {

          const fastgltf::Accessor &normalAccessor =
              asset->accessors[normal->accessorIndex];

          fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
              asset.get(), normalAccessor,
              [&](fastgltf::math::fvec3 normal, std::size_t idx) {
                vertices[idx].normal[0] = normal.x();
                vertices[idx].normal[1] = normal.y();
                vertices[idx].normal[2] = normal.z();
              });
        } else {
          spdlog::warn("Model has no normals!");
        }

        // TANGENT
        if (const auto *tangent = it->findAttribute("TANGENT");
            tangent != it->attributes.end()) {

          const fastgltf::Accessor &tangentAccessor =
              asset->accessors[tangent->accessorIndex];

          fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
              asset.get(), tangentAccessor,
              [&](const fastgltf::math::fvec4 tangent, std::size_t idx) {
                vertices[idx].tangent[0] = tangent.x();
                vertices[idx].tangent[1] = tangent.y();
                vertices[idx].tangent[2] = tangent.z();
                vertices[idx].tangent[3] = tangent.w(); // handedness
              });
        } else {
          // TODO: Mikktspace calcs
          // spdlog::warn("Model has no tangents!");
        }

        // TEXCOORD_0
        if (const auto *texcoord = it->findAttribute("TEXCOORD_0");
            texcoord != it->attributes.end()) {

          const fastgltf::Accessor &texCoordAccessor =
              asset->accessors[texcoord->accessorIndex];
          fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
              asset.get(), texCoordAccessor,
              [&](fastgltf::math::fvec2 uv, std::size_t idx) {
                vertices[idx].uv[0] = uv.x();
                vertices[idx].uv[1] = uv.y();
              });
        }

        cbz::VertexLayout vertexLayout = {};
        vertexLayout.begin(CBZ_VERTEX_STEP_MODE_VERTEX);
        vertexLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_POSITION,
                                    CBZ_VERTEX_FORMAT_FLOAT32X3);
        vertexLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_NORMAL,
                                    CBZ_VERTEX_FORMAT_FLOAT32X3);
        vertexLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_TANGENT,
                                    CBZ_VERTEX_FORMAT_FLOAT32X4);
        vertexLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_TEXCOORD0,
                                    CBZ_VERTEX_FORMAT_FLOAT32X2);
        vertexLayout.end();

        primitive.vbh = cbz::VertexBufferCreate(vertexLayout, vertices.size(),
                                                vertices.data());

        // Indices
        const fastgltf::Accessor &indexAccessor =
            asset->accessors[it->indicesAccessor.value()];

        switch (indexAccessor.componentType) {
        case fastgltf::ComponentType::UnsignedShort: {
          std::vector<uint16_t> indices(indexAccessor.count);
          fastgltf::copyFromAccessor<uint16_t>(asset.get(), indexAccessor,
                                               indices.data());

          primitive.ibh = cbz::IndexBufferCreate(
              CBZ_INDEX_FORMAT_UINT16, indexAccessor.count, indices.data());
        } break;

        case fastgltf::ComponentType::UnsignedInt: {
          std::vector<uint32_t> indices(indexAccessor.count);
          fastgltf::copyFromAccessor<uint32_t>(asset.get(), indexAccessor,
                                               indices.data());

          primitive.ibh = cbz::IndexBufferCreate(
              CBZ_INDEX_FORMAT_UINT32, indexAccessor.count, indices.data());
        } break;

        case fastgltf::ComponentType::UnsignedByte: {
          // Convert byte to uint16_t index format
          std::vector<uint8_t> byteIndices(indexAccessor.count);
          fastgltf::copyFromAccessor<uint8_t>(asset.get(), indexAccessor,
                                              byteIndices.data());
          std::vector<uint16_t> indices(byteIndices.size());

          for (size_t i = 0; i < byteIndices.size(); i++) {
            indices[i] = static_cast<uint16_t>(byteIndices[i]);
          }
          primitive.ibh = cbz::IndexBufferCreate(
              CBZ_INDEX_FORMAT_UINT16, indexAccessor.count, indices.data());
        } break;

        default: {
          spdlog::error("Unsupported Index type!");
          return cbz::Result::eFileError;
        } break;
        }

        // Material
        if (it->materialIndex.has_value()) {
          primitive.materialRef =
              materials[it->materialIndex.value()].makeRef();
        }
      }
    }

    const auto &scene = asset->scenes[asset->defaultScene.value_or(0)];

    nodes.resize(asset->nodes.size());
    for (auto nodeIndex : scene.nodeIndices) {
      rootIndices.push_back(nodeIndex);
      loadNode(&asset.get(), nodeIndex, -1);
    }

    return cbz::Result::eSuccess;
  }

  Gltf makeRef() override { return {}; }

protected:
  cbz::ImageHandle loadTexture(const fastgltf::Asset *asset,
                               const fastgltf::Texture *texture,
                               CBZTextureFormat format) {
    const fastgltf::Image &image = asset->images[texture->imageIndex.value()];
    if (std::holds_alternative<fastgltf::sources::Vector>(image.data)) {
      const auto &imageVector = std::get<fastgltf::sources::Vector>(image.data);
      spdlog::trace("- Vector : {} bytes", imageVector.bytes.size());
      return {CBZ_INVALID_HANDLE};
    }

    if (std::holds_alternative<fastgltf::sources::Array>(image.data)) {
      const fastgltf::sources::Array &imageArray =
          std::get<fastgltf::sources::Array>(image.data);

      spdlog::trace("Image: '{}'", image.name);
      spdlog::trace(" - Array: {} bytes", imageArray.bytes.size_bytes());

      int w, h, channelCount;
      stbi_uc *stbData = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc *>(imageArray.bytes.data()),
          static_cast<int>(imageArray.bytes.size()), &w, &h, &channelCount, 4);
      channelCount = 4;

      spdlog::trace(" - w: {} h: {} channels : {}", w, h, channelCount);

      cbz::ImageHandle imgh = {CBZ_INVALID_HANDLE};
      if (channelCount == 4) {
        imgh = cbz::Image2DCreate(format, w, h);
        cbz::ImageSetName(imgh, texture->name.c_str(), texture->name.length());
        cbz::Image2DUpdate(imgh, stbData, w * h);

        images.push_back(imgh);
      } else {
        spdlog::error("Unsupported Image format!");
        exit(0);
      }

      stbi_image_free(stbData);
      return imgh;
    }

    if (std::holds_alternative<fastgltf::sources::URI>(image.data)) {
      const fastgltf::sources::URI &imageURI =
          std::get<fastgltf::sources::URI>(image.data);
      spdlog::trace(" - URI: {}", imageURI.uri.c_str());
      return {CBZ_INVALID_HANDLE};
    }

    return {CBZ_INVALID_HANDLE};
  }

  void loadNode(fastgltf::Asset *asset, size_t nodeIndex,
                size_t parentNodeIndex) {
    const fastgltf::Node &fastGltfNode = asset->nodes[nodeIndex];

    Node &node = nodes[nodeIndex];
    node.name = fastGltfNode.name;
    node.index = nodeIndex;
    node.parentIndex = parentNodeIndex;

    if (std::holds_alternative<fastgltf::TRS>(fastGltfNode.transform)) {
      const fastgltf::TRS &trs =
          std::get<fastgltf::TRS>(fastGltfNode.transform);
      node.translation[0] = trs.translation.x();
      node.translation[1] = trs.translation.y();
      node.translation[2] = trs.translation.z();

      node.rotation[0] = trs.rotation.w();
      node.rotation[1] = trs.rotation.x();
      node.rotation[2] = trs.rotation.y();
      node.rotation[3] = trs.rotation.z();

      node.scale[0] = trs.scale.x();
      node.scale[1] = trs.scale.y();
      node.scale[2] = trs.scale.z();
    } else {
      spdlog::error("Node has no TRS!");
    }

    if (fastGltfNode.meshIndex.has_value()) {
      node.meshIndex = fastGltfNode.meshIndex.value();
      node.hasMesh = true;
    } else {
      node.hasMesh = false;
    }

    for (auto childIndex : fastGltfNode.children) {
      loadNode(asset, childIndex, nodeIndex);
    }
  }
};

[[nodiscard]] std::unique_ptr<Asset<Gltf>>
GltfAssetCreate(const std::string &path) {
  return std::make_unique<GltfAsset>(path);
}

StaticMeshRenderer::StaticMeshRenderer() {
  // TODO: --- Move to gltf loader/asset system ---
  sBlackIMGH = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UNORM, 1, 1);

  sWhiteIMGH = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UNORM, 1, 1);
  uint8_t whiteData[] = {255, 255, 255, 255};
  cbz::Image2DUpdate(sWhiteIMGH, whiteData, 1);

  sDefaultNormalIMGH = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UNORM, 1, 1);
  uint8_t defaultNormalData[] = {128, 128, 255, 255};
  cbz::Image2DUpdate(sDefaultNormalIMGH, defaultNormalData, 1);

  mPBRForwardShader =
      cbz::ShaderCreate("assets/shaders/forward.wgsl", CBZ_SHADER_WGLSL);
  mPBRForwardProgram = cbz::GraphicsProgramCreate(
      mPBRForwardShader,
      CBZ_GRAPHICS_PROGRAM_CULL_BACK | CBZ_GRAPHICS_PROGRAM_FRONT_FACE_CCW);

  mGBufferShader =
      cbz::ShaderCreate("assets/shaders/gBuffer.wgsl", CBZ_SHADER_WGLSL);
  mGBufferProgram = cbz::GraphicsProgramCreate(mGBufferShader);
}

StaticMeshRenderer::~StaticMeshRenderer() {
  cbz::ImageDestroy(sWhiteIMGH);
  cbz::ImageDestroy(sBlackIMGH);
  cbz::ImageDestroy(sDefaultNormalIMGH);

  cbz::ShaderDestroy(mPBRForwardShader);
  cbz::GraphicsProgramDestroy(mPBRForwardProgram);
  cbz::ShaderDestroy(mGBufferShader);
  cbz::GraphicsProgramDestroy(mGBufferProgram);
}

#include "renderer/cubozoa_render_graph.h"
void StaticMeshRenderer::render(const Camera *camera,
                                const LightSource *lightSources, uint32_t count,
                                Transform *transform, Primitive *primitive) {
  glm::mat4 globalTransform = transform->get();

  if (false) { // GBuffer
    cbz::VertexBufferSet(primitive->vbh);
    cbz::IndexBufferSet(primitive->ibh);

    cbz::TextureSet(CBZ_TEXTURE_0, primitive->materialRef.albedoTexture.imgh);

    cbz::TransformSet(glm::value_ptr(globalTransform));
    cbz::ViewSet(camera->view);
    cbz::ProjectionSet(camera->proj);

    cbz::Submit(RENDER_PASS_TYPE_GEOMETRY, mGBufferProgram);
  }

  // Forward pass
  cbz::VertexBufferSet(primitive->vbh);
  cbz::IndexBufferSet(primitive->ibh);

  static cbz::UniformHandle mSceneUH =
      cbz::UniformCreate("uScene", CBZ_UNIFORM_TYPE_VEC4);
  struct {
    float cameraPosition[4];
  } scene;
  memcpy(scene.cameraPosition, camera->position, sizeof(float) * 3);
  cbz::UniformSet(mSceneUH, &scene);

  for (uint32_t i = 0; i < count; i++) {
    cbz::UniformSet(lightSources[i].uh, &lightSources[i].properties,
                    sizeof(lightSources[i].properties) / CBZ_UNIFORM_SIZE_VEC4);
  }

  cbz::UniformSet(primitive->materialRef.uh, &primitive->materialRef.uData);
  cbz::TextureSet(CBZ_TEXTURE_0, primitive->materialRef.albedoTexture.imgh);
  cbz::TextureSet(CBZ_TEXTURE_1,
                  primitive->materialRef.metallicRoughnessTexture.imgh);
  cbz::TextureSet(CBZ_TEXTURE_2, primitive->materialRef.normalTexture.imgh);
  cbz::TextureSet(CBZ_TEXTURE_3, primitive->materialRef.occlusionTexture.imgh);
  cbz::TextureSet(CBZ_TEXTURE_4, primitive->materialRef.emissiveTexture.imgh);

  cbz::TransformSet(glm::value_ptr(globalTransform));
  cbz::ViewSet(camera->view);
  cbz::ProjectionSet(camera->proj);

  // Forward rendered objects directly draw to the light pass
  cbz::Submit(RENDER_PASS_TYPE_LIGHTING, mPBRForwardProgram);
}

// TODO: Make prefabs via lua. Including gltf creation resouces
cbz::ecs::Entity EntityCreateFromGltf(cbz::ecs::IWorld *world,
                                      Asset<Gltf> *asset) {
  // TODO: This is confusing add a asset and reference type
  GltfAsset *gltf = static_cast<GltfAsset *>(asset);

  // Instantitate root entity
  cbz::ecs::Entity out = world->instantiate(gltf->getName().c_str());
  out.addComponent<Position>();
  out.addComponent<Scale>();
  out.addComponent<Rotation>();
  out.addComponent<Transform>();

  // Instantitate all nodes
  std::vector<cbz::ecs::Entity> nodeEntities(gltf->nodes.size());
  for (GltfAsset::Node &node : gltf->nodes) {
    cbz::ecs::Entity e = nodeEntities[node.index] =
        world->instantiate(node.name.c_str());

    e.addComponent<Position>(node.translation[0], node.translation[1],
                             node.translation[2]);
    e.addComponent<Scale>(node.scale[0], node.scale[1], node.scale[2]);
    e.addComponent<Rotation>(node.rotation[0], node.rotation[1],
                             node.rotation[2], node.rotation[3]);

    // Default parent
    e.addComponent<Transform>().setParent(out);
  }

  for (GltfAsset::Node &node : gltf->nodes) {
    cbz::ecs::Entity e = nodeEntities[node.index];

    // Re parent if parent exists in node heirarchy
    if (node.parentIndex != -1) {
      e.getComponent<Transform>().setParent(nodeEntities[node.parentIndex]);
    }

    // Skip if empty node
    if (!node.hasMesh) {
      continue;
    }

    // Multiple primitives are created as individual entities
    for (PrimitiveAsset &primitiveAsset :
         gltf->meshes[node.meshIndex].primitives) {
      cbz::ecs::Entity primitiveEntity =
          world->instantiate(primitiveAsset.getName().c_str());
      primitiveEntity.addComponent<Primitive>(primitiveAsset.makeRef());

      primitiveEntity.addComponent<Position>();
      primitiveEntity.addComponent<Scale>();
      primitiveEntity.addComponent<Rotation>();

      // Set node entity as parent
      primitiveEntity.addComponent<Transform>().setParent(e);
    }
  }

  return out;
}

TextureRef AssetManager::loadTexture(const std::string &path,
                                     CBZTextureFormat format) {
  if (sTextures.find(path) != sTextures.end()) {
    spdlog::error("Texture with path '{}' already exists", path);
    return {};
  }

  // TODO: Make asset that can return fail
  int w, h, channelCount;
  stbi_uc *stbData = stbi_load(path.c_str(), &w, &h, &channelCount, 4);
  if (!stbData) {
    spdlog::info("Failed to load {}", path);
  }
  channelCount = 4;

  spdlog::trace(" - w: {} h: {} channels : {}", w, h, channelCount);

  cbz::ImageHandle imgh = {CBZ_INVALID_HANDLE};
  if (channelCount == 4) {
    imgh = cbz::Image2DCreate(format, w, h);
    cbz::Image2DUpdate(imgh, stbData, w * h);
  } else {
    spdlog::error("Unsupported Image format!");
    stbi_image_free(stbData);
    return {};
  }

  stbi_image_free(stbData);

  std::unique_ptr<TextureAsset> textureAsset =
      std::make_unique<TextureAsset>(path, imgh);

  sTextures[path] = std::move(textureAsset);
  return sTextures[path]->makeRef();
}

MaterialHandle AssetManager::loadMaterial(const std::string &path) {
  auto mat = std::make_unique<MaterialPBRAsset>(path);

  sol::load_result script = mLua.load_file(path);

  if (!script.valid()) {
    sol::error err = script;
    spdlog::error("Lua Load Error: {}", err.what());
    spdlog::error("Failed to load material at {}", path);
    return {};
  }

  sol::protected_function_result res = script();
  if (!res.valid()) {
    sol::error err = res;
    spdlog::error("Lua Run Error: {}", err.what());
    spdlog::error("Failed to load material at {}", path);
    return {};
  }

  sol::table materialTable = res;
  std::string name = materialTable["description"]["name"];
  std::string shader = materialTable["description"]["shader"];

  mat->albedoTexture = loadTexture(materialTable["textures"]["albedo"],
                                   CBZ_TEXTURE_FORMAT_RGBA8UNORMSRGB);
  mat->metallicRoughnessTexture = TextureRef(nullptr, sWhiteIMGH);
  mat->normalTexture = TextureRef(nullptr, sDefaultNormalIMGH);
  mat->occlusionTexture = TextureRef(nullptr, sWhiteIMGH);
  mat->emissiveTexture = TextureRef(nullptr, sBlackIMGH);

  MaterialHandle out = static_cast<uint32_t>(sMaterials.size());
  sMaterials.emplace_back(std::move(mat));
  return out;
}

AssetManager::AssetManager(sol::state &luaState) : mLua(luaState) {
  mLua.create_table("material");

  mLua["material"]["load"] = [this](const char *path) {
    return loadMaterial(path);
  };
}

AssetManager::~AssetManager() {}
