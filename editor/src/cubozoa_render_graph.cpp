#include "renderer/cubozoa_render_graph.h"

#include <cbz_gfx/cbz_gfx.h>
#include <cbz_gfx/cbz_gfx_imgui.h>

#include <cbz_ecs/cbz_ecs.h>

#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/detail/type_half.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <spdlog/spdlog.h>

static std::vector<float> sFSQuadVertices = {
    // x,   y,     z,   uv
    -1.0f, 1.0f,  0.0f, 0.0f, 0.0f, // Vertex 1
    1.0f,  1.0f,  0.0f, 1.0f, 0.0f, // Vertex 2
    1.0f,  -1.0f, 0.0f, 1.0f, 1.0f, // Vertex 4
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // Vertex 3
};

static std::vector<uint16_t> sFSQuadIndices = {
    0, 1, 2, // Triangle #0 connects points #0, #1 and #2
    0, 2, 3  // Triangle #1 connects points #0, #2 and #3
};

struct BlitSettings {
  float exposure = 1.0;
  float gamma = 2.2;
  float _padding[2];
} sBlitSettings = {};

static cbz::UniformHandle sBlitSettingsUH;

// Highlight entity may be different from focused if focused has no
// renderable
static cbz::ecs::Entity sHighlightEntity = {};

cbz::ecs::EntityId
DebugRenderPipeline::getEntityAtMousePosition([[maybe_unused]] uint32_t x,
                                              [[maybe_unused]] uint32_t y) {
#ifdef CBZ_DEBUG
  uint32_t offset = (y * sPickingTexelBufferW + x) * sTexelSize;
  uint32_t entityId =
      *(uint32_t *)(static_cast<const uint8_t *>(sPickingTexelBuffer.data()) +
                    offset);

  return (cbz::ecs::EntityId)entityId;
#else
  return cbz::ecs::INVALID_ENTITY_ID;
#endif //  CBZ_DEBUG
}

// TODO: Move to asset manager
#include <stb_image.h>

float cubeVertices[] = {
    // Front face (Z+)
    -1.0f, -1.0f, 1.0f, 0, 0, 1, 0.0f, 0.0f, // 0
    1.0f, -1.0f, 1.0f, 0, 0, 1, 1.0f, 0.0f,  // 1
    1.0f, 1.0f, 1.0f, 0, 0, 1, 1.0f, 1.0f,   // 2
    -1.0f, 1.0f, 1.0f, 0, 0, 1, 0.0f, 1.0f,  // 3

    // Back face (Z-)
    1.0f, -1.0f, -1.0f, 0, 0, -1, 0.0f, 0.0f,  // 4
    -1.0f, -1.0f, -1.0f, 0, 0, -1, 1.0f, 0.0f, // 5
    -1.0f, 1.0f, -1.0f, 0, 0, -1, 1.0f, 1.0f,  // 6
    1.0f, 1.0f, -1.0f, 0, 0, -1, 0.0f, 1.0f,   // 7

    // Left face (X-)
    -1.0f, -1.0f, -1.0f, -1, 0, 0, 0.0f, 0.0f, // 8
    -1.0f, -1.0f, 1.0f, -1, 0, 0, 1.0f, 0.0f,  // 9
    -1.0f, 1.0f, 1.0f, -1, 0, 0, 1.0f, 1.0f,   // 10
    -1.0f, 1.0f, -1.0f, -1, 0, 0, 0.0f, 1.0f,  // 11

    // Right face (X+)
    1.0f, -1.0f, 1.0f, 1, 0, 0, 0.0f, 0.0f,  // 12
    1.0f, -1.0f, -1.0f, 1, 0, 0, 1.0f, 0.0f, // 13
    1.0f, 1.0f, -1.0f, 1, 0, 0, 1.0f, 1.0f,  // 14
    1.0f, 1.0f, 1.0f, 1, 0, 0, 0.0f, 1.0f,   // 15

    // Top face (Y+)
    -1.0f, 1.0f, 1.0f, 0, 1, 0, 0.0f, 0.0f,  // 16
    1.0f, 1.0f, 1.0f, 0, 1, 0, 1.0f, 0.0f,   // 17
    1.0f, 1.0f, -1.0f, 0, 1, 0, 1.0f, 1.0f,  // 18
    -1.0f, 1.0f, -1.0f, 0, 1, 0, 0.0f, 1.0f, // 19

    // Bottom face (Y-)
    -1.0f, -1.0f, -1.0f, 0, -1, 0, 0.0f, 0.0f, // 20
    1.0f, -1.0f, -1.0f, 0, -1, 0, 1.0f, 0.0f,  // 21
    1.0f, -1.0f, 1.0f, 0, -1, 0, 1.0f, 1.0f,   // 22
    -1.0f, -1.0f, 1.0f, 0, -1, 0, 0.0f, 1.0f   // 23
};

uint16_t cubeIndices[] = {
    // Front face
    0, 1, 2, 0, 2, 3,

    // Back face
    4, 5, 6, 4, 6, 7,

    // Left face
    8, 9, 10, 8, 10, 11,

    // Right face
    12, 13, 14, 12, 14, 15,

    // Top face
    16, 17, 18, 16, 18, 19,

    // Bottom face
    20, 21, 22, 20, 22, 23};

// TODO: Clean up and move to env map resouce
cbz::VertexBufferHandle cubeVBH = {CBZ_INVALID_HANDLE};
cbz::IndexBufferHandle cubeIBH = {CBZ_INVALID_HANDLE};

cbz::ImageHandle hdriIMGH = {CBZ_INVALID_HANDLE};

cbz::ShaderHandle cubeMapSH = {CBZ_INVALID_HANDLE};
cbz::GraphicsProgramHandle cubeMapGPH = {CBZ_INVALID_HANDLE};
cbz::ImageHandle cubeMapIMGH = {CBZ_INVALID_HANDLE};

cbz::ShaderHandle irradianceConvolutionSH = {CBZ_INVALID_HANDLE};
cbz::GraphicsProgramHandle irradianceConvolutionGPH = {CBZ_INVALID_HANDLE};
cbz::ImageHandle irradianceCubeMapIMGH = {CBZ_INVALID_HANDLE};

cbz::ShaderHandle skyboxSH = {CBZ_INVALID_HANDLE};
cbz::GraphicsProgramHandle skyboxGPH = {CBZ_INVALID_HANDLE};

BuiltInRenderPipeline::BuiltInRenderPipeline(uint32_t targetWidth,
                                             uint32_t targetHeight) {
  rebuild(targetWidth, targetHeight);

  // Cube map
  cubeMapSH =
      cbz::ShaderCreate("assets/shaders/cube_map.wgsl", CBZ_SHADER_WGLSL);
  cubeMapGPH = cbz::GraphicsProgramCreate(cubeMapSH);

  cbz::VertexLayout cubeLayout = {};
  cubeLayout.begin(CBZ_VERTEX_STEP_MODE_VERTEX);
  cubeLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_POSITION,
                            CBZ_VERTEX_FORMAT_FLOAT32X3);
  cubeLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_NORMAL,
                            CBZ_VERTEX_FORMAT_FLOAT32X3);
  cubeLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_TEXCOORD0,
                            CBZ_VERTEX_FORMAT_FLOAT32X2);
  cubeLayout.end();

  cubeVBH = cbz::VertexBufferCreate(cubeLayout, 25, cubeVertices);
  cubeIBH = cbz::IndexBufferCreate(CBZ_INDEX_FORMAT_UINT16, 36, cubeIndices);

  // --- IBL ---
  // Convert HDR Image to cube map
  // TODO: Make asset that checks for nullptr load failure
  int w, h, channelCount;
  float *stbData =
      stbi_loadf(ASSET_DIR "/textures/citrus_orchard_road_puresky_4k.hdr", &w,
                 &h, &channelCount, 4);
  // float *stbData = stbi_loadf(ASSET_DIR "/textures/warm_bar_4k.hdr",
  //                             &w, &h, &channelCount, 4);
  channelCount = 4;

  spdlog::trace(" - w: {} h: {} channels : {}", w, h, channelCount);

  // Convert to float 16 bit hdr
  if (channelCount == 4) {
    std::vector<uint16_t> dataFloat16(w * h * 4);
    for (size_t i = 0; i < static_cast<size_t>(w * h * 4); i += 4) {
      dataFloat16[i] = glm::detail::toFloat16(stbData[i]);
      dataFloat16[i + 1] = glm::detail::toFloat16(stbData[i + 1]);
      dataFloat16[i + 2] = glm::detail::toFloat16(stbData[i + 2]);
      dataFloat16[i + 3] = glm::detail::toFloat16(stbData[i + 3]);
    }

    hdriIMGH = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA16FLOAT, w, h);
    cbz::Image2DUpdate(hdriIMGH, dataFloat16.data(), w * h);
  } else {
    spdlog::error("Unsupported Image format!");
  };

  stbi_image_free(stbData);

  cubeMapIMGH = cbz::Image2DCubeMapCreate(CBZ_TEXTURE_FORMAT_RGBA16FLOAT, 512,
                                          512, 6, CBZ_IMAGE_RENDER_ATTACHMENT);

  glm::mat4 captureProjection =
      glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

  glm::mat4 captureViews[] = {
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, -1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f))};

  for (uint32_t i = 0; i < 6; i++) {
    // Set to target
    cbz::AttachmentDescription faceAttachment = {};
    faceAttachment.imgh = cubeMapIMGH;
    faceAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
    faceAttachment.baseArrayLayer = i;
    cbz::RenderTargetSet(RENDER_PASS_TYPE_CAPTURE_0 + i, &faceAttachment, 1,
                         nullptr);

    // Draw to target
    cbz::ProjectionSet(glm::value_ptr(captureProjection));
    cbz::ViewSet(glm::value_ptr(captureViews[i]));

    cbz::TextureBindingDesc linearTexDesc = {};
    linearTexDesc.filterMode = CBZ_FILTER_MODE_LINEAR;
    cbz::TextureSet(CBZ_TEXTURE_0, hdriIMGH, linearTexDesc);

    cbz::VertexBufferSet(cubeVBH);
    cbz::IndexBufferSet(cubeIBH);
    cbz::Submit(RENDER_PASS_TYPE_CAPTURE_0 + i, cubeMapGPH);
  }

  // Convolute and render cube map to irradiance cube map
  irradianceConvolutionSH =
      cbz::ShaderCreate("assets/shaders/skybox.wgsl", CBZ_SHADER_WGLSL);

  irradianceConvolutionGPH =
      cbz::GraphicsProgramCreate(irradianceConvolutionSH);

  irradianceCubeMapIMGH = cbz::Image2DCubeMapCreate(
      CBZ_TEXTURE_FORMAT_RGBA16FLOAT, 32, 32, 6, CBZ_IMAGE_RENDER_ATTACHMENT);

  for (uint32_t i = 0; i < 6; i++) {
    // Set to target
    cbz::AttachmentDescription faceAttachment = {};
    faceAttachment.imgh = irradianceCubeMapIMGH;
    faceAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
    faceAttachment.baseArrayLayer = i;
    cbz::RenderTargetSet(RENDER_PASS_TYPE_CAPTURE_0 + 6 + i, &faceAttachment, 1,
                         nullptr);

    // Draw to target
    cbz::ProjectionSet(glm::value_ptr(captureProjection));
    cbz::ViewSet(glm::value_ptr(captureViews[i]));

    cbz::TextureBindingDesc textureCubeDesc = {};
    textureCubeDesc.viewDimension = CBZ_TEXTURE_VIEW_DIMENSION_CUBE;
    textureCubeDesc.filterMode = CBZ_FILTER_MODE_LINEAR;
    cbz::TextureSet(CBZ_TEXTURE_0, cubeMapIMGH, textureCubeDesc);

    cbz::VertexBufferSet(cubeVBH);
    cbz::IndexBufferSet(cubeIBH);
    cbz::Submit(RENDER_PASS_TYPE_CAPTURE_0 + 6 + i, irradianceConvolutionGPH);
  }

  skyboxSH = cbz::ShaderCreate("assets/shaders/skybox.wgsl", CBZ_SHADER_WGLSL);
  skyboxGPH = cbz::GraphicsProgramCreate(skyboxSH);
}

BuiltInRenderPipeline::~BuiltInRenderPipeline() { destroyAttachments(); }

void BuiltInRenderPipeline::configure([[maybe_unused]] RenderPipeline &_) {}

void BuiltInRenderPipeline::destroyAttachments() {
  // Destroy blit resources
  cbz::VertexBufferDestroy(mQuadVBH);
  cbz::IndexBufferDestroy(mQuadIBH);
  cbz::GraphicsProgramDestroy(mBlitPH);
  cbz::ShaderDestroy(mBlitSH);

  // Destroy gBuffer resources
  for (cbz::ImageHandle imgh : mGBuffer.attachments) {
    cbz::ImageDestroy(imgh);
  }

  // Destroy lighting pass resouce
  cbz::ImageDestroy(mHDRBrightAttachment);
  cbz::ImageDestroy(mHDRLightingAttachment);
  cbz::ImageDestroy(mLightingDepthAttachment);

  // Destroy Debug resources
  if (mHighlightAttachment) {
    cbz::ImageDestroy(mHighlightAttachment);
  }
}

void BuiltInRenderPipeline::onImGuiRender() {
  ImGui::Begin("Render pipeline");

  ImVec2 avail = ImGui::GetContentRegionAvail();

  cbz::imgui::Image(mGBuffer.attachments[GBUFFER_NORMAL_ATTACHMENT],
                    ImVec2(avail.y, avail.y));
  ImGui::SameLine();
  cbz::imgui::Image(mHDRLightingAttachment, ImVec2(avail.y, avail.y));
  ImGui::End();
}

void BuiltInRenderPipeline::submit(const Camera &camera) {
  // TODO: Move skybox to first so deph tested
  // --- Skybox ---
  {
    glm::mat4 matrix(1.0f);

    cbz::VertexBufferSet(cubeVBH);
    cbz::IndexBufferSet(cubeIBH);

    cbz::TextureBindingDesc textureDesc = {};
    textureDesc.viewDimension = CBZ_TEXTURE_VIEW_DIMENSION_CUBE;
    textureDesc.addressMode = CBZ_ADDRESS_MODE_CLAMPTOEDGE;
    textureDesc.filterMode = CBZ_FILTER_MODE_LINEAR;
    cbz::TextureSet(CBZ_TEXTURE_0, cubeMapIMGH, textureDesc);

    cbz::TransformSet(glm::value_ptr(matrix));

    // Remove translation
    glm::mat4 viewNoTranslation = glm::make_mat4(camera.view);
    viewNoTranslation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    cbz::ViewSet(glm::value_ptr(viewNoTranslation));
    cbz::ProjectionSet(camera.proj);

    cbz::Submit(RENDER_PASS_TYPE_LIGHTING, skyboxGPH);
  }

#ifdef CBZ_DEBUG
  if (sHighlightEntity) {
    // Highlight if visible
    if (sHighlightEntity.hasComponent<Primitive>()) {
      glm::mat4 matrix = sHighlightEntity.getComponent<Transform>().get();
      Scale scale = sHighlightEntity.getComponent<Scale>();

      matrix = glm::scale(matrix, glm::vec3(scale) + .025f);

      Primitive &primitive = sHighlightEntity.getComponent<Primitive>();
      cbz::VertexBufferSet(primitive.vbh);
      cbz::IndexBufferHandle(primitive.ibh);

      cbz::TransformSet(glm::value_ptr(matrix));

      cbz::ProjectionSet(camera.proj);
      cbz::ViewSet(camera.view);
      cbz::Submit(RENDER_PASS_TYPE_HIGHLIGHT, mHighlightProgram);
    }
  }
#endif // CBZ_DEBUG

  // TODO: Blit to editor if debug

  // Blit to swapchain surface
  cbz::VertexBufferSet(mQuadVBH);
  cbz::IndexBufferSet(mQuadIBH);

  cbz::TextureSet(CBZ_TEXTURE_0, mHDRLightingAttachment);

#ifdef CBZ_DEBUG
  cbz::TextureSet(CBZ_TEXTURE_1, mHighlightAttachment);
#endif // CBZ_DEBUG

  cbz::UniformSet(sBlitSettingsUH, &sBlitSettings);
  cbz::Submit(CBZ_DEFAULT_RENDER_TARGET, mBlitPH);
}

void BuiltInRenderPipeline::rebuild(uint32_t w, uint32_t h) {
  // --- GBuffer pass setup ---
  mGBuffer.attachments[GBUFFER_POSITION_ATTACHMENT] = cbz::Image2DCreate(
      CBZ_TEXTURE_FORMAT_RGBA16FLOAT, w, h, CBZ_IMAGE_RENDER_ATTACHMENT);

  mGBuffer.attachments[GBUFFER_NORMAL_ATTACHMENT] = cbz::Image2DCreate(
      CBZ_TEXTURE_FORMAT_RGBA16FLOAT, w, h, CBZ_IMAGE_RENDER_ATTACHMENT);

  mGBuffer.attachments[GBUFFER_ALBEDO_SPECULAR_ATTACHMENT] = cbz::Image2DCreate(
      CBZ_TEXTURE_FORMAT_RGBA8UNORM, w, h, CBZ_IMAGE_RENDER_ATTACHMENT);

  cbz::AttachmentDescription gBufferAttachments[] = {
      cbz::AttachmentDescription{
          {0.0f, 0.0f, 0.0f, 0.0f},
          mGBuffer.attachments[GBUFFER_POSITION_ATTACHMENT]},
      cbz::AttachmentDescription{
          {0.0f, 0.0f, 0.0f, 0.0f},
          mGBuffer.attachments[GBUFFER_NORMAL_ATTACHMENT]},
      cbz::AttachmentDescription{
          {0.0f, 0.0f, 0.0f, 0.0f},
          mGBuffer.attachments[GBUFFER_ALBEDO_SPECULAR_ATTACHMENT]},
  };

  cbz::RenderTargetSet(RENDER_PASS_TYPE_GEOMETRY, gBufferAttachments,
                       GBUFFER_ATTACHMENT_COUNT);

  // --- Lighting pass setup ---
  mHDRLightingAttachment = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA16FLOAT, w,
                                              h, CBZ_IMAGE_RENDER_ATTACHMENT);

  mHDRBrightAttachment = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA16FLOAT, w,
                                            h, CBZ_IMAGE_RENDER_ATTACHMENT);

  mLightingDepthAttachment = cbz::Image2DCreate(
      CBZ_TEXTURE_FORMAT_DEPTH24PLUS, w, h, CBZ_IMAGE_RENDER_ATTACHMENT);

  const cbz::AttachmentDescription lightColorAttachments[]{
      {{0.0f, 0.0f, 0.0f, 1.0f},
       mHDRLightingAttachment,
       0,
       1,
       CBZ_RENDER_ATTACHMENT_BLEND},
      {{0.0f, 0.0f, 0.0f, 1.0f},
       mHDRBrightAttachment,
       0,
       1,
       CBZ_RENDER_ATTACHMENT_BLEND},
  };

  const cbz::AttachmentDescription lightDepthAttachment{
      {0.0f, 0.0f, 0.0f, 1.0f}, mLightingDepthAttachment};
  cbz::RenderTargetSet(RENDER_PASS_TYPE_LIGHTING, lightColorAttachments, 2,
                       &lightDepthAttachment);

  // --- Post processs set up
  mHighlightShader =
      cbz::ShaderCreate("assets/shaders/highlight.wgsl", CBZ_SHADER_WGLSL);
  mHighlightProgram = cbz::GraphicsProgramCreate(mHighlightShader);
  mHighlightAttachment = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UNORM, w, h,
                                            CBZ_IMAGE_RENDER_ATTACHMENT);

  cbz::AttachmentDescription highlightColorAttachment = {};
  highlightColorAttachment.imgh = mHighlightAttachment;
  highlightColorAttachment.flags = CBZ_RENDER_ATTACHMENT_BLEND;
  highlightColorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

  cbz::RenderTargetSet(RENDER_PASS_TYPE_HIGHLIGHT, &highlightColorAttachment, 1,
                       nullptr);

  // --- Blit pass setup ---
  // Create blit program
  mBlitSH =
      cbz::ShaderCreate("assets/shaders/blit_texture.wgsl", CBZ_SHADER_WGLSL);
  mBlitPH = cbz::GraphicsProgramCreate(mBlitSH);

  // Create fs quad vertex and index buffers
  cbz::VertexLayout fsQuadVertexLayout = {};
  fsQuadVertexLayout.begin(CBZ_VERTEX_STEP_MODE_VERTEX);
  fsQuadVertexLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_POSITION,
                                    CBZ_VERTEX_FORMAT_FLOAT32X3);
  fsQuadVertexLayout.push_attribute(CBZ_VERTEX_ATTRIBUTE_TEXCOORD0,
                                    CBZ_VERTEX_FORMAT_FLOAT32X2);
  fsQuadVertexLayout.end();

  mQuadVBH = cbz::VertexBufferCreate(
      fsQuadVertexLayout, static_cast<uint32_t>(sFSQuadVertices.size()) / 5,
      sFSQuadVertices.data());

  mQuadIBH = cbz::IndexBufferCreate(
      CBZ_INDEX_FORMAT_UINT16, static_cast<uint32_t>(sFSQuadIndices.size()),
      sFSQuadIndices.data());

  sBlitSettingsUH = cbz::UniformCreate("uBlitSettings", CBZ_UNIFORM_TYPE_VEC4);
}

static std::vector<cbz::ecs::Entity> sWorldTransforms;
static std::unordered_map<cbz::ecs::EntityId, std::vector<cbz::ecs::Entity>>
    sEntityToChildren;

static cbz::ecs::Entity sFocusedEntity = {};

static void DrawRecursive(cbz::ecs::Entity e) {
  static ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                         ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_OpenOnArrow;

  ImGuiTreeNodeFlags node_flags = base_flags;

  if (sFocusedEntity.getId() == e.getId()) {
    node_flags |= ImGuiTreeNodeFlags_Selected;
  }

  bool node_open = false;
  if (sEntityToChildren[e.getId()].size() > 0) {
    node_open =
        ImGui::TreeNodeEx((void *)(intptr_t)(e.getId()), node_flags, "%s (%zu)",
                          e.getName(), sEntityToChildren[e.getId()].size());
  } else {
    node_flags |= ImGuiTreeNodeFlags_Leaf;
    node_open = ImGui::TreeNodeEx((void *)(intptr_t)(e.getId()), node_flags,
                                  "%s", e.getName());
  }

  if (ImGui::IsItemClicked()) {
    sFocusedEntity = e;
  }

  if (node_open) {
    for (cbz::ecs::Entity child : sEntityToChildren[e.getId()]) {
      DrawRecursive(child);
    }

    ImGui::TreePop();
  }
}

DebugRenderPipeline::DebugRenderPipeline(cbz::ecs::IWorld *world, uint32_t w,
                                         uint32_t h)
    : mWorld(world) {
  mStencilPickerShader =
      cbz::ShaderCreate("assets/shaders/stencilPicker.wgsl", CBZ_SHADER_WGLSL);
  mStencilPickerProgram = cbz::GraphicsProgramCreate(mStencilPickerShader);
  // TODO: Move to application
  [[maybe_unused]] static int _ = [this]() {
    static cbz::ecs::Entity skyBox = mWorld->instantiate("Skybox");
    skyBox.addComponent<Position>();
    skyBox.addComponent<Rotation>();
    skyBox.addComponent<Scale>();
    skyBox.addComponent<Transform>();

    Skybox &sb = skyBox.addComponent<Skybox>();
    sb.irradianceCubeMap = irradianceCubeMapIMGH;
    sb.skyboxCubeMap = cubeMapIMGH;
    sb.hdriMap = hdriIMGH;
    return 1;
  }();

  rebuild(w, h);
}

DebugRenderPipeline::~DebugRenderPipeline() {
  cbz::StructuredBufferDestroy(sPickableEntitiesSBH);
  if (sPickingAttachment) {
    cbz::ImageDestroy(sPickingAttachment);
  }
}

void DebugRenderPipeline::findPickables() {
  static int staggeredCtr = 24;

  if (staggeredCtr++ % 24 != 0) {
    return;
  }

  // Update visible entity storage buffer for picking
  static std::vector<cbz::ecs::EntityId> mVisible;
  mVisible.clear();

  mWorld->query<cbz::ecs::Entity, Primitive>(
      [](cbz::ecs::Entity e, [[maybe_unused]] const Primitive &_) {
        mVisible.push_back(e.getId());
      });

  if ((mVisible.capacity() * sizeof(cbz::ecs::EntityId) %
       CBZ_UNIFORM_SIZE_VEC4) != 0) {
    mVisible.reserve((mVisible.capacity() + (CBZ_UNIFORM_SIZE_VEC4 - 1)) &
                     ~((CBZ_UNIFORM_SIZE_VEC4 - 1)));
  }

  if (mVisible.size() > 0) {
    uint32_t bufferIdx =
        (mVisible.size() * sizeof(cbz::ecs::EntityId)) / CBZ_UNIFORM_SIZE_VEC4;

    uint32_t offset = mVisible.size() % CBZ_UNIFORM_SIZE_VEC4;
    uint32_t elemCount = bufferIdx + (offset > 0 ? 1 : 0);
    cbz::StructuredBufferUpdate(sPickableEntitiesSBH, elemCount,
                                mVisible.data());
  }

  cbz::MousePosition pos = cbz::GetMousePosition();
  if (pos.x != sPickOriginX || pos.y != sPickOriginY) {
    // TODO: Pick around mouse.
    // sPickOriginX = pos.x;
    // sPickOriginY = pos.y;
    // cbz::Origin3D origin{sPickOriginX, sPickOriginY, 0};
    // cbz::TextureExtent extent{256, 256, 1};

    cbz::Origin3D origin{sPickOriginX, sPickOriginY, 0};
    cbz::TextureExtent extent{static_cast<int>(sPickingTexelBufferW),
                              static_cast<int>(sPickingTexelBufferH), 1};

    cbz::TextureReadAsync(
        sPickingAttachment, &origin, &extent,
        [extent, this](const void *texels) {
          static const uint32_t offset =
              (sPickOriginY * sPickingTexelBufferW + sPickOriginX) * sTexelSize;

          memcpy(sPickingTexelBuffer.data() + offset, texels,
                 extent.width * extent.height * extent.layers * sTexelSize);
        });
  }
};

void DebugRenderPipeline::focusEntity(cbz::ecs::Entity e) {
  sHighlightEntity = e;

  // Find root level entity
  cbz::ecs::Entity parent = e.getComponent<Transform>().getParent();
  while (parent) {
    e = parent;
    parent = e.getComponent<Transform>().getParent();
  }

  sFocusedEntity = e;
  spdlog::info("Clicked on : {}, Focusing on {}", sHighlightEntity.getId(),
               sFocusedEntity.getId());
}

void DebugRenderPipeline::onImGuiRender() {
  if (ImGui::Begin("Pipeline Settings")) {
    static float fpsHistory[100] = {};
    static int fpsIndex = 0;
    static int fpsCount = 0;

    float fps = ImGui::GetIO().Framerate;
    fpsHistory[fpsIndex] = fps;
    fpsIndex = (fpsIndex + 1) % IM_ARRAYSIZE(fpsHistory);
    if (fpsCount < IM_ARRAYSIZE(fpsHistory))
      fpsCount++;

    float avgFps = 0.0f;
    for (int i = 0; i < fpsCount; i++)
      avgFps += fpsHistory[i];
    avgFps /= fpsCount;

    ImGui::Text("FPS: %.1f", avgFps);

    ImGui::DragFloat("exposure", &sBlitSettings.exposure, 0.0001f);
    ImGui::DragFloat("gamma", &sBlitSettings.gamma, 0.1f);
  }
  ImGui::End(); // Pipeline state

  if (ImGui::Begin("Entity Hierarchy")) {
    // Call on every add or remove entity
    static bool called = false;
    if (!called) {
      sWorldTransforms.clear(); // Clear rootss map on add or remove?

      called = true;
      mWorld->query<cbz::ecs::Entity, Transform>(
          [](cbz::ecs::Entity e, Transform &t) {
            if (!t.getParent()) {
              sWorldTransforms.push_back(e);
              return;
            }

            sEntityToChildren[t.getParent().getId()].push_back(e);
          });
    }

    for (cbz::ecs::Entity e : sWorldTransforms) {
      DrawRecursive(e);
    }
  }
  ImGui::End(); //  Entity Heirarchy

  // --- Inspected focused entity --
  if (ImGui::Begin("Inspector")) {
    if (sFocusedEntity) {
      static std::string sEntityLabel;
      sEntityLabel = "[" + std::to_string(sFocusedEntity.getId()) + "] " +
                     std::string(sFocusedEntity.getName());

      if (ImGui::CollapsingHeader(sEntityLabel.c_str(), nullptr,
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position",
                          &sFocusedEntity.getComponent<Position>().x);
        ImGui::DragFloat3("Scale", &sFocusedEntity.getComponent<Scale>().x);

        // Get current rotation quaternion
        glm::quat q = glm::quat(sFocusedEntity.getComponent<Rotation>());
        glm::vec3 euler =
            glm::eulerAngles(q); // XYZ = pitch, yaw, roll in radians

        // Extract pitch and yaw
        float pitch = glm::degrees(euler.x); // X axis
        float yaw = glm::degrees(euler.y);   // Y axis

        // Optionally persist across frames
        static float sPitch = pitch;
        static float sYaw = yaw;

        bool changed = false;
        changed |= ImGui::SliderFloat("Pitch", &sPitch, -180.0f, 180.0f);
        changed |= ImGui::SliderFloat("Yaw", &sYaw, -180.0f, 180.0f);
        ImGui::Text("%.2f %.2f %.2f", glm::degrees(euler.x),
                    glm::degrees(euler.y), glm::degrees(euler.z));

        if (changed) {
          // Convert pitch and yaw back to quaternion
          glm::quat rotX =
              glm::angleAxis(glm::radians(sPitch), glm::vec3(1, 0, 0));
          glm::quat rotY =
              glm::angleAxis(glm::radians(sYaw), glm::vec3(0, 1, 0));
          glm::quat rot = rotY * rotX; // Yaw then pitch

          sFocusedEntity.getComponent<Rotation>().w = rot.w;
          sFocusedEntity.getComponent<Rotation>().x = rot.x;
          sFocusedEntity.getComponent<Rotation>().y = rot.y;
          sFocusedEntity.getComponent<Rotation>().z = rot.z;
        }

        // Primitive
        if (sFocusedEntity.hasComponent<Primitive>()) {
          const Primitive &p = sFocusedEntity.getComponent<Primitive>();

          if (ImGui::CollapsingHeader("Primitive", nullptr,
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("VBH : %d IBH: %d", p.vbh.idx, p.ibh.idx);
          }

          if (ImGui::CollapsingHeader("Material", nullptr,
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            auto avail = ImGui::GetContentRegionAvail();

            for (uint8_t i = 0; i < p.materialRef.textureCount; i++) {
              cbz::imgui::Image(p.materialRef.textures[i].imgh,
                                {avail.x, avail.x});
            }
          }
        }

        // Camera
        if (sFocusedEntity.hasComponent<Camera>()) {
          if (ImGui::CollapsingHeader("Camera", nullptr,
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            Camera &camera = sFocusedEntity.getComponent<Camera>();
            ImGui::DragFloat("Aspect ratio", &camera.aspectRatio);
            ImGui::DragFloat("Near", &camera.near);
            ImGui::DragFloat("Far", &camera.far);

            const char *cameraTypeNames[] = {"Perspective", "Orthographic"};
            int currentType = static_cast<int>(camera.type);
            if (ImGui::Combo("Camera Type", &currentType, cameraTypeNames,
                             CAMERA_TYPE_COUNT)) {
              camera.type = static_cast<CameraType>(currentType);
            }
          }
        }

        // Lights
        if (sFocusedEntity.hasComponent<LightSource>()) {
          if (ImGui::CollapsingHeader("LightSource", nullptr,
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            LightSource &lightSource =
                sFocusedEntity.getComponent<LightSource>();

            const char *cameraTypeNames[] = {"Directional", "Point"};
            int currentType = static_cast<int>(lightSource.type);
            if (ImGui::Combo("Type", &currentType, cameraTypeNames,
                             LIGHT_TYPE_COUNT)) {
              lightSource.type = static_cast<LightType>(currentType);
            }

            ImGui::Text("Direction: %.2f %.2f %.2f",
                        lightSource.properties.direction[0],
                        lightSource.properties.direction[1],
                        lightSource.properties.direction[2]);

            if (ImGui::IsItemHovered()) {
              ImGui::BeginTooltip();
              ImGui::Text("Direction is set by a light source's rotation.");
              ImGui::EndTooltip();
            }

            ImGui::DragFloat3("Intensity", lightSource.properties.intensity);

            bool castShadows = lightSource.castShadows;
            if (ImGui::Checkbox("Cast shadows", &castShadows)) {
              lightSource.castShadows = castShadows;
            }
          }
        }

        if (sFocusedEntity.hasComponent<Skybox>()) {
          if (ImGui::CollapsingHeader("Skybox", nullptr,
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            const Skybox &skybox = sFocusedEntity.getComponent<Skybox>();
            auto avail = ImGui::GetContentRegionAvail();
            cbz::imgui::Image(skybox.hdriMap, {avail.x, avail.x});
          }
        }
      }
    }
  }
  ImGui::End(); // Inspector
}

void DebugRenderPipeline::rebuild(uint32_t w, uint32_t h) {
  // Pick pass setup
  sPickingTexelBufferW = w;
  sPickingTexelBufferH = h;

  const uint32_t textureSize =
      (sPickingTexelBufferW * sPickingTexelBufferH) * sTexelSize;
  sPickingTexelBuffer.resize(textureSize);

  sPickingAttachment =
      cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UINT, w, h,
                         CBZ_IMAGE_RENDER_ATTACHMENT | CBZ_IMAGE_COPY_SRC);

  cbz::AttachmentDescription pickingAttachment = {};
  pickingAttachment.imgh = sPickingAttachment;
  pickingAttachment.clearValue = {0.0f, 0.0f, 0.0f, 0.0f};
  cbz::RenderTargetSet(RENDER_PASS_TYPE_PICKING, &pickingAttachment, 1);

  // Buffer of entity Ids to send to pick pass
  sPickableEntitiesSBH = cbz::StructuredBufferCreate(
      CBZ_UNIFORM_TYPE_VEC4, (cbz::MAX_COMMAND_SUBMISSIONS * sizeof(uint32_t)) /
                                 CBZ_UNIFORM_SIZE_VEC4);
}

void DebugRenderPipeline::renderPickable(const Camera &camera,
                                         const Transform &transform,
                                         const Primitive &primitive) {
  glm::mat4 globalTransform = transform.get();

  cbz::VertexBufferSet(primitive.vbh);
  cbz::IndexBufferSet(primitive.ibh);

  cbz::TransformSet(glm::value_ptr(globalTransform));
  cbz::ViewSet(camera.view);
  cbz::ProjectionSet(camera.proj);

  cbz::StructuredBufferSet(CBZ_BUFFER_0, sPickableEntitiesSBH);
  cbz::Submit(RENDER_PASS_TYPE_PICKING, mStencilPickerProgram);
}
