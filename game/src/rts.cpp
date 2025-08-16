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
      [](Position &position, [[maybe_unused]] Unit &_, UnitMoveState &moveState) {
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
//lowFieldCreate(int centerX, int centerY, int s, uint8_t out) {
void FlowFieldCreate(IVec2 center, int searchRadius, Vec2* out) {
  const glm::ivec2 gridDimensions(GRID_X, GRID_Y);
  const int dstIdx = center.y * gridDimensions.x + center.x;

  std::vector<uint8_t> costField(GRID_X * GRID_Y);
  for (int i = 0; i < GRID_X * GRID_Y; i++) {
    costField[i] = 1;
  }

  costField[dstIdx] = 0;

  std::vector<uint8_t> integrationField(GRID_X * GRID_Y);
  std::fill(integrationField.begin(), integrationField.end(), 255);
  integrationField[dstIdx] = 0;

  std::vector<bool> seenList(GRID_X * GRID_Y);
  std::fill(seenList.begin(), seenList.end(), false);

  std::vector<int> openList(GRID_X * GRID_Y);
  std::fill(openList.begin(), openList.end(), 0);

  int openListTail = 0;
  int openListHead = 1;

  // DFS cost
  openList[0] = dstIdx;
  seenList[dstIdx] = true;

  while (openListTail < openListHead) {
    int cellIdx = openList[openListTail++];

    int neighbors[] = {
        cellIdx + 1,                // right
        cellIdx - 1,                // left
        cellIdx - gridDimensions.x, // top
        cellIdx + gridDimensions.x  // bottom
    };

    bool skipNeighbor[] = {false, false, false, false};

    // Checked neighbors
    for (int directionIdx = 0; directionIdx < 4; directionIdx++) {
      int neighborCellIdx = neighbors[directionIdx];

      // Skip if out of bounds
      if (neighborCellIdx < 0 ||
          neighborCellIdx >= (gridDimensions.x * gridDimensions.y)) {
        skipNeighbor[directionIdx] = true;
        continue;
      }

      // Skip if out of bounds
      if (neighborCellIdx >= gridDimensions.x * gridDimensions.y) {
        skipNeighbor[directionIdx] = true;
        continue;
      }

      // Skip seen neighbors
      if (seenList[neighborCellIdx]) {
        skipNeighbor[directionIdx] = true;
        continue;
      }
    }

    // Add newly seen neighbors to list
    for (int directionIdx = 0; directionIdx < 4; directionIdx++) {
      if (!skipNeighbor[directionIdx]) {
        openList[openListHead] = neighbors[directionIdx];
        openListHead++;

        // Mark neighbor cell as seen
        seenList[neighbors[directionIdx]] = true;

        // Accumulate min costs
        uint8_t newCost =
            integrationField[cellIdx] + costField[neighbors[directionIdx]];
        if (newCost < integrationField[neighbors[directionIdx]]) {
          integrationField[neighbors[directionIdx]] = newCost;
          if (newCost == 0 && cellIdx != dstIdx) {
            spdlog::error("{} 0 cost despite not being {}", cellIdx, dstIdx);
          }
        }
      }
    }
  }

  for (int y = 0; y < searchRadius; y++) {
    for (int x = 0; x < searchRadius; x++) {
      int cellIdx = y * searchRadius + x;

      int neighbors[] = {
          cellIdx + 1,                // right
          cellIdx - 1,                // left
          cellIdx - gridDimensions.x, // top
          cellIdx + gridDimensions.x  // bottom
      };

      [[maybe_unused]] bool skipNeighbor[] = {false, false, false, false};

      constexpr Vec2 neighborDirections[] = {
          { 0.0f,  1.0f},  // right
          { 0.0f, -1.0f},  // left
          { 1.0f,  0.0f},  // top
          {-1.0f,  0.0f},  // bottom
      };

      // Checked neighbors for least
      Vec2 minCostNeighborDirection = {0.0f, 0.0f};
	  int minCost = std::numeric_limits<int>::max();
      for (int directionIdx = 0; directionIdx < 4; directionIdx++) {
        int neighborCellIdx = neighbors[directionIdx];

        // Skip if out of bounds left
        if (neighborCellIdx < 0 ||
            neighborCellIdx >= (gridDimensions.x * gridDimensions.y)) {
          continue;
        }

        // Skip if out of bounds right
        if (neighborCellIdx >= gridDimensions.x * gridDimensions.y) {
          continue;
        }

        if (integrationField[neighborCellIdx] < minCost) {
          minCostNeighborDirection = neighborDirections[directionIdx];
          minCost = integrationField[neighborCellIdx];
        }
      }

      out[cellIdx] = minCostNeighborDirection;
    }
  }
}

unitId Spawn([[maybe_unused]] UnitAssetId _, Position dst) {
  cbz::ecs::Entity e = sWorld->instantiate();
  e.addComponent<Position>(dst);
  e.addComponent<Rotation>();
  e.addComponent<Scale>();
  e.addComponent<Transform>();
  e.addComponent<UnitMoveState>();

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
