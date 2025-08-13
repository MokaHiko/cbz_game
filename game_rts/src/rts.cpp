#include "rts/rts.h"

#define CBZ_ECS_IMPLEMENTATION
#include <cbz_ecs/cbz_ecs.h>

namespace rts {

static std::unique_ptr<cbz::ecs::IWorld> sWorld;
static std::vector<cbz::ecs::Entity> sUnits;

void Init() {
  sWorld = std::unique_ptr<cbz::ecs::IWorld>(cbz::ecs::InitWorld());
  // --- Server Systems ---
  // Movement
  sWorld->system_for_each<Position, Unit, UnitMoveState>(
      [](Position &position, Unit &_, UnitMoveState &moveState) {
        if (!moveState.isMoving) {
          return;
        }
        // TODO :find in flowfiled
        // position.x += moveState.direction[0];
        // position.y += moveState.direction[1];
        // position.z += moveState.direction[2];
        position.x += 5.0f * 0.001;
      });
}

unitId Spawn(UnitAssetId _, Position dst) {
  cbz::ecs::Entity e = sWorld->instantiate();
  e.addComponent<Position>(dst);
  e.addComponent<Rotation>();
  e.addComponent<Scale>();
  e.addComponent<Transform>();

  unitId id = sUnits.size();
  sUnits.push_back(e);

  return id;
}

void MoveTo(unitId unit, Position dst) {
  UnitMoveState &moveState = sUnits[unit].getComponent<UnitMoveState>();

  moveState.targetDst[0] = dst.x;
  moveState.targetDst[1] = dst.y;
  moveState.targetDst[2] = dst.z;
}

void Step() {}

void Shutdown() {}

}; // namespace rts
