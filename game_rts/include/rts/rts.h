#ifndef RTS_GAME_H_
#define RTS_GAME_H_

#include <cbz/cbz_defines.h>
#include <cbz_ecs/cbz_ecs_types.h>

#ifdef RTS_DOUBLE_PRECISION
typedef double real_t;
#else
typedef float real_t;
#endif

typedef enum : uint32_t {
  CELL_TYPE_NONE = 0,
  CELL_TYPE_GROUND,
  CELL_TYPE_MUD,
  CELL_TYPE_WATER,
  CELL_TYPE_COUNT,
} CellType;

typedef enum : uint32_t {
  CELL_PROPERTY_NONE = 0,
  CELL_PROPERTY_BURNING = 1 << 0,
  CELL_PROPERTY_COUNT,
} CellPropertyFlags;

CBZ_API struct Cell {
  CellType type;
  CellPropertyFlags properties;
};

CBZ_API struct Unit {
  const char *name;
  real_t health;
  real_t mana;
  real_t damage;
  real_t armor;
  real_t movement_speed;
};

CBZ_API struct UnitMoveState {
  float targetDst[3];
  CBZBool32 isMoving;
};

CBZ_API struct UnitAssetId {
  uint32_t idx;
};

#include <cbz/cbz_asset.h>
CBZ_API class UnitAsset : Asset<Unit> {};
namespace rts {

typedef uint32_t unitId;

CBZ_API struct CellPosition {
  uint8_t x;
  uint8_t y;
  uint8_t z;
};

// --- Simulation Functions ---
CBZ_API void Init();
CBZ_API void Step();
CBZ_API void Shutdown();

// --- Unit Functions ---
CBZ_API [[nodiscard]] unitId Spawn(UnitAssetId unitAssetId, Position dst);
CBZ_API void MoveTo(unitId unit, Position dst);
CBZ_API void Attack(unitId attacker, unitId target);

// --- Util ---

}; // namespace rts

#endif // RTS_GAME_H_
