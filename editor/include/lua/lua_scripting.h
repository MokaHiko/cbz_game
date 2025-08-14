#ifndef CBZ_LUA_SCRIPT_H_
#define CBZ_LUA_SCRIPT_H_

#include <cubozoa_ecs/cubozoa_ecs.h>

namespace cbz {

struct LuaScript {
  const char *path;
};

class LuaScripting : public cbz::ecs::System<LuaScript> {
public:
  LuaScripting();
  ~LuaScripting();

  // void onFrameStart() override;

protected:
  // void render(Transform &transform, Primitive &mesh);
  //
  // cbz::ShaderHandle mPBRForwardShader;
  // cbz::GraphicsProgramHandle mPBRForwardProgram;
  //
  // cbz::ShaderHandle mGBufferShader;
  // cbz::GraphicsProgramHandle mGBufferProgram;
  //
  // float mView[16];
  // float mProj[16];
  // float mCameraPos[4];
};

}; // namespace cbz

#endif
