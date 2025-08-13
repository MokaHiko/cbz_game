#ifndef CBZ_RENDER_GRAPH_H_
#define CBZ_RENDER_GRAPH_H_

#include "cubozoa_render_types.h"

#include <cbz_ecs/cbz_ecs.h>
#include <cbz_ecs/cbz_ecs_types.h>

// --- Built-in Render Pipeline --
typedef enum {
  RENDER_PASS_TYPE_SHADOW = 0,

#ifdef CBZ_DEBUG
  RENDER_PASS_TYPE_PICKING,
  RENDER_PASS_TYPE_HIGHLIGHT,
// RENDER_PASS_TYPE_SHADER_HEAT_MAP,
#endif // CBZ_DEBUG

#ifdef CBZ_PROFILE
  RENDER_PASS_TYPE_SHADER_HEAT_MAP,
#endif // CBZ_PROFILE

  RENDER_PASS_TYPE_GEOMETRY,

  RENDER_PASS_TYPE_LIGHTING,
  RENDER_PASS_TYPE_POST_PROCESS,

  RENDER_PASS_TYPE_BLIT,
  RENDER_PASS_TYPE_COUNT,
} RenderPassType;

typedef enum {
  GBUFFER_POSITION_ATTACHMENT = 0,
  GBUFFER_NORMAL_ATTACHMENT,
  GBUFFER_ALBEDO_SPECULAR_ATTACHMENT,
  GBUFFER_ATTACHMENT_COUNT,
} GBufferAttachments;

struct GBuffer {
  cbz::ImageHandle attachments[GBUFFER_ATTACHMENT_COUNT];
};

typedef enum {
  POST_PROCESSING_FLAGS_BLOOM = 0,
  POST_PROCESSING_FLAGS_BLUR = 1 << 0,
  POST_PROCESSING_FLAGS_CHROMATIC_ABBERATION = 1 << 2,
} PostProcessingFlags;

typedef enum {
  RENDERER_LIGHTING_TYPE_FORWARD = 0,
  RENDERER_LIGHTING_TYPE_DEFFERED,
} RendererLightingType;

struct RenderPipeline {
  RendererLightingType rendereType;
};

class BuiltInRenderPipeline {
public:
  BuiltInRenderPipeline(uint32_t w, uint32_t h);
  ~BuiltInRenderPipeline();

  void submit(const Camera &camera);

  void rebuild(uint32_t w, uint32_t h);

  void configure(RenderPipeline &rp);
  void destroyAttachments();

  void onImGuiRender();

private:
  // GBuffer resources
  GBuffer mGBuffer;

  // Lighting pass resouce
  // @note Lighting pass has depth attachment for forward rendered
  // objects skipping the g buffer.
  cbz::ImageHandle mHDRLightingAttachment;
  cbz::ImageHandle mHDRBrightAttachment;
  cbz::ImageHandle mLightingDepthAttachment;

  // Blit process resources
  cbz::ShaderHandle mBlitSH;
  cbz::GraphicsProgramHandle mBlitPH;
  cbz::VertexBufferHandle mQuadVBH;
  cbz::IndexBufferHandle mQuadIBH;

  cbz::ShaderHandle mHighlightShader;
  cbz::GraphicsProgramHandle mHighlightProgram;
  cbz::ImageHandle mHighlightAttachment;
};

class DebugRenderPipeline {
public:
  DebugRenderPipeline(cbz::ecs::IWorld *world, uint32_t w, uint32_t h);
  ~DebugRenderPipeline();

  void focusEntity(cbz::ecs::Entity e);

  void findPickables();
  void renderPickable(const Camera &camera, const Transform &transform,
                      const Primitive &mesh);

  cbz::ecs::EntityId getEntityAtMousePosition(uint32_t w, uint32_t h);

  void rebuild(uint32_t w, uint32_t h);

  void onImGuiRender();

protected:
  cbz::ShaderHandle mStencilPickerShader;
  cbz::GraphicsProgramHandle mStencilPickerProgram;
  cbz::ImageHandle sPickingAttachment;
  cbz::StructuredBufferHandle sPickableEntitiesSBH;

  // TODO: Ring of images for gizmos
  // cbz::ImageHandle mPreviewColorAttachment;

  uint32_t sPickingTexelBufferW;
  uint32_t sPickingTexelBufferH;
  uint32_t sTexelSize = 4;
  std::vector<uint8_t> sPickingTexelBuffer;

  uint32_t sPickOriginX = 0;
  uint32_t sPickOriginY = 0;

  cbz::ecs::IWorld *mWorld;
};

#endif // CBZ_RENDER_GRAPH_H_
