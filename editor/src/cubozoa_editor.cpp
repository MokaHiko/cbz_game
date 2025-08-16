#include "cbz_gfx/cbz_gfx_defines.h"
#include "imgui.h"
#include "renderer/cubozoa_gltf.h"
#include "renderer/cubozoa_render_graph.h"
#include "renderer/cubozoa_render_types.h"

#include <cbz/cbz_time.h>

#include <cbz_gfx/cbz_gfx.h>
#include <cbz_gfx/cbz_gfx_imgui.h>

#include <cbz_ecs/cbz_ecs_types.h>
#include <cstdint>

// TODO: Make into plug-in system
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <spdlog/spdlog.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

static constexpr uint32_t mWidth = 1920;
static constexpr uint32_t mHeight = 1080;

// Application resources
static float sTime;
static float sLastTime;
static float sDeltaTime;
static uint32_t sFrameCtr;

static Camera sCamera;

// Level assets
static std::vector<std::unique_ptr<Asset<Gltf>>> sGltfs;
static std::unique_ptr<cbz::ecs::IWorld> sWorld;
static std::unique_ptr<AssetManager> sAssetManager;
// static std::vector<std::unique_ptr<Asset<LuaScript>>> sScripts;

// SOL LUA
static sol::state sLua;

// Systems
static std::unique_ptr<DebugRenderPipeline> sDebugRendererPipeline;
static std::unique_ptr<BuiltInRenderPipeline> sBuiltInRenderPipeline;
static std::unique_ptr<StaticMeshRenderer> sStaticMeshRenderer;

// --- Game ---
#include <rts/rts.h>

static std::vector<float> sQuadVertices = {
    // x,   y,   z,    uv
    -1.0f, 0.0f, 1.0f,  0.0f, 0.0f, // Vertex 1
    1.0f,  0.0f, 1.0f,  1.0f, 0.0f, // Vertex 2
    1.0f,  0.0f, -1.0f, 1.0f, 1.0f, // Vertex 4
    -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, // Vertex 3
};

static std::vector<uint16_t> sQuadIndices = {
    0, 1, 2, // Triangle #0 connects points #0, #1 and #2
    0, 2, 3  // Triangle #1 connects points #0, #2 and #3
};

// Flow field
cbz::ImageHandle costFieldTexture = {CBZ_INVALID_HANDLE};
cbz::ImageHandle integrationFieldTexture = {CBZ_INVALID_HANDLE};
cbz::ImageHandle flowFieldTexture = {CBZ_INVALID_HANDLE};
rts::IVec2 flowFieldTarget = {GRID_X / 2, GRID_Y / 2};

cbz::ShaderHandle flowFieldShader = {CBZ_INVALID_HANDLE};
cbz::GraphicsProgramHandle flowFieldProgram = {CBZ_INVALID_HANDLE};
cbz::VertexBufferHandle QuadVBH = {CBZ_INVALID_HANDLE};
cbz::IndexBufferHandle QuadIBH = {CBZ_INVALID_HANDLE};

static void OnImGuiRender() {
  sBuiltInRenderPipeline->onImGuiRender();
  sDebugRendererPipeline->onImGuiRender();

  ImGui::Begin("Flow field");
  ImGui::Text("Grid %d bu %d", GRID_X, GRID_Y);
  ImGui::DragInt2("Target", &flowFieldTarget.x, 1.0f, 0, GRID_X);
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float width = avail.x / 3.0f;
  cbz::imgui::Image(costFieldTexture, ImVec2(width, avail.y));
  ImGui::SameLine();
  cbz::imgui::Image(integrationFieldTexture, ImVec2(width, avail.y));
  ImGui::SameLine();
  cbz::imgui::Image(flowFieldTexture, ImVec2(width, avail.y));
  ImGui::End();
}

namespace cbz {

class EditorApplication {
public:
  EditorApplication(const char *name,
                    CBZNetworkStatus netStatus = CBZ_NETWORK_CLIENT);
  virtual ~EditorApplication();

  virtual void init() {};
  virtual void shutdown() {};

  void update();

  CBZ_NO_DISCARD cbz::ecs::Entity
  instantiate(const Position &position = {0.0f, 0.0f, 0.0f},
              const Scale &scale = {1.0f, 1.0f, 1.0f},
              const Rotation &rotation = {1.0f, 0.0f, 0.0f, 0.0f},
              const char *name = nullptr);
};

EditorApplication::EditorApplication(const char *name,
                                     CBZNetworkStatus netStatus) {
  if (cbz::Init({name, mWidth, mHeight, netStatus}) != cbz::Result::eSuccess) {
    exit(0);
  }

#ifdef CBZ_DEBUG
  cbz::SetImGuiRenderCallback(OnImGuiRender);
#endif

  // --- Reset time ---
  sLastTime = 0.0f;
  sTime = 1.0f;
  sDeltaTime = 0.001f;

  sAssetManager = std::make_unique<AssetManager>(sLua);

  // --- Client Systems ---
  sWorld = std::unique_ptr<cbz::ecs::IWorld>(cbz::ecs::InitWorld());

  // Tranform Hierarchy
  sWorld->system([](ecs::IWorld *world) {
    struct TransformData {
      Position *pos;
      Rotation *rot;
      Scale *scale;
      Transform *t;

      cbz::ecs::EntityId eId;
    };

    // TODO: not make static to be thread safe
    static std::vector<TransformData> sTransformData;
    sTransformData.clear();

    world->query<cbz::ecs::Entity, Position, Rotation, Scale, Transform>(
        [](cbz::ecs::Entity e, Position &pos, Rotation &rot, Scale &scale,
           Transform &t) {
          sTransformData.push_back({&pos, &rot, &scale, &t, e});
        });

    // Topological sort by ascending depth and parent
    std::sort(sTransformData.begin(), sTransformData.end(),
              [](const TransformData &a, const TransformData &b) {
                if (a.t->getDepth() != b.t->getDepth()) {
                  return a.t->getDepth() < b.t->getDepth();
                }

                return a.t->getParent().getId() < b.t->getParent().getId();
              });

    for (TransformData &tData : sTransformData) {
      glm::mat4 parentTransform = glm::mat4(1.0f);

      if (tData.t->getParent()) {
        parentTransform = cbz::ecs::Entity(tData.t->getParent())
                              .getComponent<Transform>()
                              .get();
      }

      glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(*tData.scale));
      glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(*tData.pos));
      glm::mat4 R = glm::mat4_cast(glm::quat(*tData.rot));
      tData.t->set(parentTransform * T * R * S);
    }
  });

  // Local Illumination
  static_assert(sizeof(LightSource) % CBZ_UNIFORM_SIZE_VEC4 == 0);
  sWorld->system_for_each<Position, Rotation, Transform, LightSource>(
      [](const Position &position, const Rotation &rotation,
         const Transform &transform, LightSource &lightSrc) {
        if (!lightSrc.uh) {
          if (lightSrc.castShadows) {
            // Make shadow map
          }

          switch (lightSrc.type) {
          case LIGHT_TYPE_DIRECTIONAL: {
            static_assert(
                sizeof(LightSource::properties) % CBZ_UNIFORM_SIZE_VEC4 == 0);
            lightSrc.uh = cbz::UniformCreate("uDirLight", CBZ_UNIFORM_TYPE_VEC4,
                                             sizeof(LightSource::properties) /
                                                 CBZ_UNIFORM_SIZE_VEC4);
          } break;
          case LIGHT_TYPE_POINT: {
          } break;
          case LIGHT_TYPE_COUNT:
            break;
          }
        }

        lightSrc.properties.position[0] = position.x;
        lightSrc.properties.position[1] = position.y;
        lightSrc.properties.position[2] = position.z;

        glm::vec3 forward = glm::quat(rotation) * glm::vec3(0.0f, 0.0f, -1.0f);
        lightSrc.properties.direction[0] = forward.x;
        lightSrc.properties.direction[1] = forward.y;
        lightSrc.properties.direction[2] = forward.z;

        memcpy(lightSrc.properties.lightSpaceMatrix,
               glm::value_ptr(transform.get()), sizeof(float) * 16);
      });

  sBuiltInRenderPipeline =
      std::make_unique<BuiltInRenderPipeline>(mWidth, mHeight);
#ifdef CBZ_DEBUG
  sDebugRendererPipeline =
      std::make_unique<DebugRenderPipeline>(sWorld.get(), mWidth, mHeight);

  // Render picking objects
  sWorld->system_for_each<Transform, Primitive>(
      [](Transform &transform, Primitive &mesh) {
        sDebugRendererPipeline->renderPickable(sCamera, transform, mesh);
      });
#endif //  CBZ_DEBUG

  // Renderables
  sStaticMeshRenderer = std::make_unique<StaticMeshRenderer>();

  // TODO: Light environment as ssbo and non static
  static LightSource lightEnv = {};
  static uint32_t lightCount = 1;
  static Skybox skybox = {};
  sWorld->system_for_each<LightSource>(
      [](LightSource &lightSource) { lightEnv = lightSource; });

  sWorld->system_for_each<Skybox>([](Skybox &sb) { skybox = sb; });

  sWorld->system_for_each<Transform, Primitive>(
      [](Transform &transform, Primitive &primitive) {
        sStaticMeshRenderer->render(&sCamera, &skybox, &lightEnv, lightCount,
                                    &transform, &primitive);
      });

  // RenderPipeline submission
  sWorld->system(
      [](ecs::IWorld *) { sBuiltInRenderPipeline->submit(sCamera); });

  // --- Define Lua bindings ---
  // Common libraries
  sLua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math);

  // Expose common types
  sLua.new_usertype<Position>("Position", "x", &Position::x, "y", &Position::y,
                              "z", &Position::z);

  sLua.new_usertype<Scale>("Scale", "x", &Scale::x, "y", &Scale::y, "z",
                           &Scale::z);

  sLua.new_usertype<Rotation>("Rotation", "w", &Rotation::w, "x", &Rotation::x,
                              "y", &Rotation::y, "z", &Rotation::z);

  sLua.new_usertype<Camera>("Camera", "aspectRatio", &Camera::aspectRatio,
                            "fov", &Camera::fov, "near", &Camera::near, "far",
                            &Camera::far);

  // Transform
  sLua["position"] = sLua.create_table();
  sLua["position"]["get"] = [](cbz::ecs::EntityId eId) {
    const Position &pos =
        cbz::ecs::Entity(eId, sWorld.get()).getComponent<Position>();
    return Position{pos.x, pos.y, pos.z};
  };
  sLua["position"]["set"] = [](cbz::ecs::EntityId eId, float x, float y,
                               float z) {
    Position &pos =
        cbz::ecs::Entity(eId, sWorld.get()).getComponent<Position>();
    pos.x = x;
    pos.y = y;
    pos.z = z;
  };

  sLua["scale"] = sLua.create_table();
  sLua["scale"]["get"] = [](cbz::ecs::EntityId eId) {
    const Scale &scale =
        cbz::ecs::Entity(eId, sWorld.get()).getComponent<Scale>();
    return Scale{scale.x, scale.y, scale.z};
  };
  sLua["scale"]["set"] = [](cbz::ecs::EntityId eId, float x, float y, float z) {
    Scale &scale = cbz::ecs::Entity(eId, sWorld.get()).getComponent<Scale>();
    scale.x = x;
    scale.y = y;
    scale.z = z;
  };

  sLua["rotation"] = sLua.create_table();
  sLua["rotation"]["get"] = [](cbz::ecs::EntityId eId) {
    const Rotation &rotation =
        cbz::ecs::Entity(eId, sWorld.get()).getComponent<Rotation>();
    return Rotation{rotation.x, rotation.y, rotation.z};
  };
  sLua["rotation"]["set"] = [](cbz::ecs::EntityId eId, float w, float x,
                               float y, float z) {
    Rotation &rotation =
        cbz::ecs::Entity(eId, sWorld.get()).getComponent<Rotation>();
    rotation.w = w;
    rotation.x = x;
    rotation.y = y;
    rotation.z = z;
  };

  sLua["rotation"]["set_euler"] = [](cbz::ecs::EntityId eId, float radX,
                                     float radY, float radZ) {
    Rotation &rotation =
        cbz::ecs::Entity(eId, sWorld.get()).getComponent<Rotation>();
    rotation.setEuler(radX, radY, radZ);
  };

  // Assets
  sLua["gltf"] = sLua.create_table();
  sLua["gltf"]["load"] = [&](const char *path) {
    uint32_t assetId = sGltfs.size();
    sGltfs.push_back(GltfAssetCreate(std::string(ASSET_DIR) + path));

    if (sGltfs[assetId]->load() != cbz::Result::eSuccess) {
      spdlog::error("Failed to load gltf at {}", std::string(ASSET_DIR) + path);
      return std::numeric_limits<uint32_t>::max();
    }

    return assetId;
  };

  sLua["gltf"]["spawn"] = [](uint32_t assetId) {
    if (assetId >= sGltfs.size()) {
      return cbz::ecs::INVALID_ENTITY_ID;
    }

    return EntityCreateFromGltf(sWorld.get(), sGltfs[assetId].get()).getId();
  };

  // Camera fns
  sLua["camera"] = sLua.create_table();
  sLua["camera"]["spawn"] = [this](const char *name) {
    cbz::ecs::Entity e = instantiate();
    e.setName(name);

    Camera &camera = e.addComponent<Camera>();
    camera.aspectRatio = 16.0 / 9.0;
    camera.fov = glm::radians(90.0f);
    camera.near = 0.1f;
    camera.far = 1000.0f;

    return e.getId();
  };

  sLua["is_camera"] = [](cbz::ecs::EntityId eId) {
    return cbz::ecs::Entity(eId, sWorld.get()).hasComponent<Camera>();
  };

  // Lights
  sLua["is_directional_light"] = [](cbz::ecs::EntityId eId) {
    if (!cbz::ecs::Entity(eId, sWorld.get()).hasComponent<LightSource>()) {
      return false;
    };

    return cbz::ecs::Entity(eId, sWorld.get())
               .getComponent<LightSource>()
               .type == LIGHT_TYPE_DIRECTIONAL;
  };

  sLua["lights"] = sLua.create_table();
  sLua["lights"]["spawn_directional"] = [this](const char *name) {
    cbz::ecs::Entity e = instantiate();
    e.setName(name);
    e.addComponent<LightSource>().type = LIGHT_TYPE_DIRECTIONAL;
    return e.getId();
  };

  sLua["directional_light_set_color"] = [](cbz::ecs::EntityId eId, float r,
                                           float g, float b) {
    cbz::ecs::Entity e(eId, sWorld.get());
    if (!e.hasComponent<LightSource>()) {
      return;
    }

    e.getComponent<LightSource>().properties.intensity[0] = {r};
    e.getComponent<LightSource>().properties.intensity[1] = {g};
    e.getComponent<LightSource>().properties.intensity[2] = {b};
  };

  sLua["point_light_spawn"] = [this](const char *name) {
    cbz::ecs::Entity e = instantiate();
    e.setName(name);
    e.addComponent<LightSource>().type = LIGHT_TYPE_POINT;
  };

  sLua["point_light_set_color"] = [](cbz::ecs::EntityId eId, float r, float g,
                                     float b) {
    cbz::ecs::Entity e(eId, sWorld.get());
    if (!e.hasComponent<LightSource>()) {
      return;
    }

    e.getComponent<LightSource>().properties.intensity[0] = {r};
    e.getComponent<LightSource>().properties.intensity[1] = {g};
    e.getComponent<LightSource>().properties.intensity[2] = {b};
  };

  // Laod scripts
  sol::load_result script = sLua.load_file(ASSET_DIR "scripts/init.lua");
  if (!script.valid()) {
    sol::error err = script;
    spdlog::error("Lua Load Error: {}", err.what());
    exit(0); // TODO: Recover
  }

  sol::protected_function_result res = script();
  if (!res.valid()) {
    sol::error err = script;
    spdlog::error("Lua Run Error: {}", err.what());
    exit(0); // TODO: Recover
  }

  // Call  function
  sol::protected_function init_fn = sLua["Init"];
  if (!init_fn.valid()) {
    sol::error err = script;
    spdlog::error("Lua Load Error: {}", err.what());
    exit(0); // TODO: Recover
  }

  res = init_fn();
  if (!res.valid()) {
    sol::error err = res;
    spdlog::error("Lua Run Error: {}", err.what());
    exit(0); // TODO: Recover
  }

  // --- Game ---
  rts::Init();

  // TODO: Clean up and destroy
  costFieldTexture = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UNORM,
                                        static_cast<uint32_t>(GRID_X),
                                        static_cast<uint32_t>(GRID_Y));

  integrationFieldTexture = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UNORM,
                                               static_cast<uint32_t>(GRID_X),
                                               static_cast<uint32_t>(GRID_Y));

  flowFieldTexture = cbz::Image2DCreate(CBZ_TEXTURE_FORMAT_RGBA8UNORM,
                                        static_cast<uint32_t>(GRID_X),
                                        static_cast<uint32_t>(GRID_Y));

  flowFieldShader =
      cbz::ShaderCreate("assets/shaders/flow_field.wgsl", CBZ_SHADER_WGLSL);
  cbz::ShaderSetName(flowFieldShader, "flow_field", strlen("flow_field"));
  flowFieldProgram = cbz::GraphicsProgramCreate(flowFieldShader);
  cbz::GraphicsProgramSetName(flowFieldProgram, "flow_program",
                              strlen("flow_program"));

  cbz::VertexLayout quadVL = {};
  quadVL.begin(CBZ_VERTEX_STEP_MODE_VERTEX);
  quadVL.push_attribute(CBZ_VERTEX_ATTRIBUTE_POSITION,
                        CBZ_VERTEX_FORMAT_FLOAT32X3);
  quadVL.push_attribute(CBZ_VERTEX_ATTRIBUTE_TEXCOORD0,
                        CBZ_VERTEX_FORMAT_FLOAT32X2);
  quadVL.end();

  QuadVBH = cbz::VertexBufferCreate(quadVL, 4, sQuadVertices.data());
  QuadIBH =
      cbz::IndexBufferCreate(CBZ_INDEX_FORMAT_UINT16, 6, sQuadIndices.data());

  cbz::ecs::Entity flowField = instantiate();
  flowField.setName("flowField");
  Primitive &primitive = flowField.addComponent<Primitive>();
  primitive.vbh = QuadVBH;
  primitive.ibh = QuadIBH;
  primitive.materialRef.textureCount = 1;
  primitive.materialRef.ph = flowFieldProgram;
  primitive.materialRef.textures[MATERIAL_TEXTURE_PRIMARY] = {
      NULL, integrationFieldTexture};
}

EditorApplication::~EditorApplication() {
  // --- Game ---
  cbz::ShaderDestroy(flowFieldShader);
  cbz::GraphicsProgramDestroy(flowFieldProgram);

  cbz::VertexBufferDestroy(QuadVBH);
  cbz::IndexBufferDestroy(QuadIBH);

  cbz::ImageDestroy(flowFieldTexture);
  cbz::ImageDestroy(integrationFieldTexture);
  cbz::ImageDestroy(costFieldTexture);

  // Clean up
  rts::Shutdown();
  cbz::Shutdown();
  sDeltaTime = 0;
}

cbz::ecs::Entity EditorApplication::instantiate(const Position &position,
                                                const Scale &scale,
                                                const Rotation &rotation,
                                                const char *name) {
  cbz::ecs::Entity out = sWorld->instantiate(name);
  out.addComponent<Position>(position);
  out.addComponent<Scale>(scale);
  out.addComponent<Rotation>(rotation);

  glm::mat4 matrix = {};
  matrix = glm::translate(matrix, glm::vec3(position));
  matrix = glm::mat4_cast(glm::quat(rotation)) * matrix;
  matrix = glm::scale(matrix, glm::vec3(scale));

  out.addComponent<Transform>().set(matrix);
  return out;
}

void EditorApplication::update() {
  // Update cameras
  bool hasCamera = false;
  sWorld->query<Position, Rotation, Camera>(
      [&](Position &pos, Rotation &rot, Camera &camera) {
        hasCamera = true;
        camera.position[0] = pos.x;
        camera.position[1] = pos.y;
        camera.position[2] = pos.z;

        glm::mat4 proj = glm::perspective(camera.fov, camera.aspectRatio,
                                          camera.near, camera.far);
        memcpy(camera.proj, glm::value_ptr(proj), sizeof(float) * 16);

        glm::vec3 forward = glm::quat(rot) * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(pos), glm::vec3(pos) + forward,
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        memcpy(camera.view, glm::value_ptr(view), sizeof(float) * 16);

        // Store camera data
        sCamera = camera;
      });

  if (!hasCamera) {
    spdlog::warn("Game has no camera!");
  }

  // Run scripts
  sol::protected_function update_fn = sLua["Update"];
  if (update_fn.valid()) {
    sol::protected_function_result res = update_fn(sDeltaTime);
    if (!res.valid()) {
      sol::error err = res;
      spdlog::error("Error on lua 'update' fn {}", err.what());
      exit(0); // TODO: Recover
    }
  }

  // Editor
  sDebugRendererPipeline->findPickables();
  if (cbz::IsMouseButtonPressed(cbz::MouseButton::eLeft)) {
    MousePosition mousePos = cbz::GetMousePosition();
    cbz::ecs::EntityId eId = sDebugRendererPipeline->getEntityAtMousePosition(
        mousePos.x, mousePos.y);

    // TODO: Set invalid id as clear value instead of hard coded.
    if (eId != 16777216) {
      sDebugRendererPipeline->focusEntity({eId, sWorld.get()});
    }
  }

  // Update game systems
  sWorld->step(sDeltaTime);

  // -- GAME --

  // Visualize flow field
  if (cbz::IsMouseButtonPressed(cbz::MouseButton::eRight)) {
    const glm::ivec2 gridDimensions(GRID_X, GRID_Y);
    const int dstIdx = flowFieldTarget.y * gridDimensions.x + flowFieldTarget.x;

    int maxCost = GRID_X;

    // TODO: Each field as colors

    // Create Generate map
    std::vector<int> costField(GRID_X * GRID_Y);
    for (int i = 0; i < GRID_X * GRID_Y; i++) {
      costField[i] = 1;
    }
    costField[dstIdx] = 0;

    std::vector<cbz::ColorRGBA> costFieldColors(GRID_X * GRID_Y);
    for (int i = 0; i < GRID_X * GRID_Y; i++) {
      float t = costField[i] / static_cast<float>(maxCost);
      costFieldColors[i].r = static_cast<uint8_t>(std::ceil(t * 255.0f));
      costFieldColors[i].g = 255 - costFieldColors[i].r;
      costFieldColors[i].b = 0;
      costFieldColors[i].a = 255;

      if (i == dstIdx) {
        costFieldColors[i] = {0, 0, 255, 255};
      }
    }

    cbz::Image2DUpdate(costFieldTexture, costFieldColors.data(),
                       GRID_X * GRID_Y);

    // Create Integration field
    std::vector<cbz::ColorRGBA> integrationFieldColors(GRID_X * GRID_Y);
    std::vector<int> integrationField(GRID_X * GRID_Y);
    rts::IntegrationFieldCreate(costField.data(), flowFieldTarget, GRID_X,
                                integrationField.data());
    for (int i = 0; i < GRID_X * GRID_Y; i++) {
      float t = integrationField[i] / static_cast<float>(maxCost);
      integrationFieldColors[i].r = static_cast<uint8_t>(std::ceil(t * 255.0f));
      integrationFieldColors[i].g = 255 - integrationFieldColors[i].r;
      integrationFieldColors[i].b = 0;
      integrationFieldColors[i].a = 255;

      if (i == dstIdx) {
        integrationFieldColors[i] = {0, 0, 255, 255};
      }
    }
    cbz::Image2DUpdate(integrationFieldTexture, integrationFieldColors.data(),
                       GRID_X * GRID_Y);

    // Create flow field
    std::vector<cbz::ColorRGBA> flowFieldColors(GRID_X * GRID_Y);
    std::vector<rts::Vec2> flowField(GRID_X * GRID_Y);
    rts::FlowFieldCreate(integrationField.data(), flowFieldTarget, GRID_X,
                         flowField.data());

    for (int i = 0; i < GRID_X * GRID_Y; i++) {
      flowFieldColors[i].r = (flowField[i].x * 0.5f + 1.0f) * 255;
      flowFieldColors[i].g = (flowField[i].y * 0.5f + 1.0f) * 255;
      flowFieldColors[i].b = 0;
      flowFieldColors[i].a = 255;

      if (i == dstIdx) {
        flowFieldColors[i] = {0, 0, 255, 255};
      }
    }

    cbz::Image2DUpdate(flowFieldTexture, flowFieldColors.data(),
                       GRID_X * GRID_Y);
  }

  // Draw
  sFrameCtr = cbz::Frame();
}

}; // namespace cbz

int main(int argc, char **argv) {
  for (int i = 0; i < argc; i++) {
    printf("%s ", argv[i]);
  }

  cbz::EditorApplication app("Editor");

#ifndef __EMSCRIPTEN__
  while (true) {
    app.update();
  }
#else
  emscripten_set_main_loop_arg(
      [](void *userData) {
        cbz::EditorApplication *app =
            reinterpret_cast<cbz::EditorApplication *>(userData);
        app->update();
      },
      (void *)&app, // value sent to the 'userData' arg of the callback
      0, true);
#endif
}
