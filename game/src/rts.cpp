#include "rts/rts.h"
#include "spdlog/spdlog.h"
#include <limits>

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

void IntegrationFieldCreate(const int *costField, IVec2 center,
                            [[maybe_unused]] int searchRadius,
                            int *integrationField) {
  const glm::ivec2 gridDimensions(GRID_X, GRID_Y);
  const int dstIdx = center.y * gridDimensions.x + center.x;

  for (int i = 0; i < GRID_X * GRID_Y; i++) {
    integrationField[i] = std::numeric_limits<int>::max();
  }
  integrationField[dstIdx] = 0;

  std::vector<bool> seenList(GRID_X * GRID_Y);
  std::fill(seenList.begin(), seenList.end(), false);

  std::vector<int> openList(GRID_X * GRID_Y);
  std::fill(openList.begin(), openList.end(), 0);

  int openListTail = 0;
  int openListHead = 1;

  // BFS cost
  openList[0] = dstIdx;
  seenList[dstIdx] = true;

  while (openListTail < openListHead) {
    int cellIdx = openList[openListTail++];

    int neighbors[8] = {
        cellIdx + 1,                    // right
        cellIdx - 1,                    // left
        cellIdx - gridDimensions.x,     // top
        cellIdx + gridDimensions.x,     // bottom
        cellIdx - gridDimensions.x + 1, // top right
        cellIdx - gridDimensions.x - 1, // top left
        cellIdx + gridDimensions.x + 1, // bottom right
        cellIdx + gridDimensions.x - 1, // bottom left
    };

    // Add newly seen neighbors to list
    for (int directionIdx = 0; directionIdx < 8; directionIdx++) {
      int neighborCellIdx = neighbors[directionIdx];

      // Skip if out of bounds
      if (neighborCellIdx < 0 ||
          neighborCellIdx >= (gridDimensions.x * gridDimensions.y)) {
        continue;
      }

      // Skip if out of bounds
      if (neighborCellIdx >= gridDimensions.x * gridDimensions.y) {
        continue;
      }

      // Skip seen neighbors
      if (seenList[neighborCellIdx]) {
        continue;
      }

      // Add neighbor to open list
      openList[openListHead] = neighborCellIdx;
      openListHead++;

      // Mark neighbor cell as seen
      seenList[neighborCellIdx] = true;

      // Accumulate min costs
      int newCost = integrationField[cellIdx] + costField[neighborCellIdx];
      if (newCost >= integrationField[neighborCellIdx]) {
        continue;
      }

      // Debug Checks
      if (integrationField[cellIdx] == std::numeric_limits<int>::max()) {
        spdlog::error("Out of order access!");
      }
      if (integrationField[neighborCellIdx] != 0 &&
          integrationField[neighborCellIdx] !=
              std::numeric_limits<int>::max()) {
        spdlog::error("neighbor {} {}", neighborCellIdx, dstIdx);
      }

      integrationField[neighborCellIdx] = newCost;

      if (newCost == 0 && neighborCellIdx != dstIdx) {
        spdlog::error("{} has 0 cost despite not being {}", neighborCellIdx,
                      dstIdx);
      }
    }
  }
}

void FlowFieldCreate(const int *integrationField, IVec2 center,
                     [[maybe_unused]] int searchRadius, Vec2 *out) {
  const glm::ivec2 gridDimensions(GRID_X, GRID_Y);
  const int dstIdx = center.y * gridDimensions.x + center.x;

  std::vector<int> costField(GRID_X * GRID_Y);
  for (int i = 0; i < GRID_X * GRID_Y; i++) {
    costField[i] = 1;
  }
  costField[dstIdx] = 0;

  for (int y = 0; y < GRID_Y; y++) {
    for (int x = 0; x < GRID_X; x++) {
      int cellIdx = y * GRID_X + x;

      int neighbors[8] = {
          cellIdx + 1,                    // right
          cellIdx - 1,                    // left
          cellIdx - gridDimensions.x,     // top
          cellIdx + gridDimensions.x,     // bottom
          cellIdx - gridDimensions.x + 1, // top right
          cellIdx - gridDimensions.x - 1, // top left
          cellIdx + gridDimensions.x + 1, // bottom right
          cellIdx + gridDimensions.x - 1, // bottom left
      };

      constexpr Vec2 neighborDirections[] = {
          {1.0f, 0.0f},   // right
          {-1.0f, 0.0f},  // left
          {0.0f, 1.0f},   // top
          {0.0f, -1.0f},  // bottom
          {1.0f, 1.0f},   // top right
          {-1.0f, 1.0f},  // top left
          {1.0f, -1.0f},  // bottom right
          {-1.0f, -1.0f}, // bottom left
      };

      // Checked neighbors for least
      Vec2 minCostNeighborDirection = {0.0f, 0.0f};
      int minCost = std::numeric_limits<int>::max();
      for (int directionIdx = 0; directionIdx < 8; directionIdx++) {
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

unitId Spawn(UnitAssetId _, Position dst) {
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
