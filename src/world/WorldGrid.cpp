#include "world/WorldGrid.hpp"
#include <cmath>
#include <algorithm>

WorldGrid::WorldGrid() {
    reset();
}

void WorldGrid::reset() {
    for (auto& t : m_tiles) {
        t = Tile{};
    }
}

void WorldGrid::tick(float dt) {
    for (auto& tile : m_tiles) {
        if (!tile.solid) {
            growFood(tile, dt);
            // Toxin decays slowly
            if (tile.toxin > 0.f) {
                tile.toxin -= 0.005f * dt;
                if (tile.toxin < 0.f) tile.toxin = 0.f;
            }
        }
    }
}

void WorldGrid::growFood(Tile& tile, float dt) {
    // Logistic growth: dF/dt = r * F * (1 - F/K)
    // K (carrying capacity) = fertility, r = 0.02/s base rate
    // Also allow regrowth from zero via a tiny seed term
    if (tile.fertility <= 0.f) return;
    const float r = 0.02f;
    float f = tile.food;
    float k = tile.fertility;
    float seed = 0.001f * k;  // tiny constant to allow regrowth from 0
    float delta = r * (f + seed) * (1.f - f / k) * dt;
    tile.food += delta;
    if (tile.food > tile.fertility) tile.food = tile.fertility;
    if (tile.food < 0.f)           tile.food = 0.f;
}

float WorldGrid::sampleFoodNearby(glm::vec2 worldPos, float radius) const {
    int tx0 = std::max(0, (int)(worldPos.x - radius));
    int ty0 = std::max(0, (int)(worldPos.y - radius));
    int tx1 = std::min(WIDTH  - 1, (int)(worldPos.x + radius));
    int ty1 = std::min(HEIGHT - 1, (int)(worldPos.y + radius));
    float sum = 0.f, count = 0.f;
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            float dx = tx + 0.5f - worldPos.x;
            float dy = ty + 0.5f - worldPos.y;
            if (dx*dx + dy*dy <= radius*radius) {
                sum += at(tx, ty).food;
                count += 1.f;
            }
        }
    }
    return count > 0.f ? sum / count : 0.f;
}

float WorldGrid::sampleToxinNearby(glm::vec2 worldPos, float radius) const {
    int tx0 = std::max(0, (int)(worldPos.x - radius));
    int ty0 = std::max(0, (int)(worldPos.y - radius));
    int tx1 = std::min(WIDTH  - 1, (int)(worldPos.x + radius));
    int ty1 = std::min(HEIGHT - 1, (int)(worldPos.y + radius));
    float sum = 0.f, count = 0.f;
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            float dx = tx + 0.5f - worldPos.x;
            float dy = ty + 0.5f - worldPos.y;
            if (dx*dx + dy*dy <= radius*radius) {
                sum += at(tx, ty).toxin;
                count += 1.f;
            }
        }
    }
    return count > 0.f ? sum / count : 0.f;
}

float WorldGrid::sampleOxygenAt(glm::vec2 worldPos) const {
    int tx = std::max(0, std::min(WIDTH  - 1, (int)worldPos.x));
    int ty = std::max(0, std::min(HEIGHT - 1, (int)worldPos.y));
    return at(tx, ty).oxygen;
}

uint8_t WorldGrid::zoneAt(glm::vec2 worldPos) const {
    int tx = std::max(0, std::min(WIDTH  - 1, (int)worldPos.x));
    int ty = std::max(0, std::min(HEIGHT - 1, (int)worldPos.y));
    return at(tx, ty).zoneId;
}

float WorldGrid::consumeFood(int x, int y, float amount) {
    if (!inBounds(x, y)) return 0.f;
    Tile& t = at(x, y);
    float consumed = std::min(t.food, amount);
    t.food -= consumed;
    return consumed;
}

void WorldGrid::setZone(int cx, int cy, int radius, uint8_t zoneId) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx*dx + dy*dy <= radius*radius) {
                int tx = cx + dx, ty = cy + dy;
                if (inBounds(tx, ty))
                    at(tx, ty).zoneId = zoneId;
            }
        }
    }
}

void WorldGrid::addFood(int cx, int cy, int radius, float amount) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx*dx + dy*dy <= radius*radius) {
                int tx = cx + dx, ty = cy + dy;
                if (inBounds(tx, ty)) {
                    at(tx, ty).food += amount;
                    if (at(tx, ty).food > at(tx, ty).fertility)
                        at(tx, ty).food = at(tx, ty).fertility;
                }
            }
        }
    }
}

void WorldGrid::addToxin(int cx, int cy, int radius, float amount) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx*dx + dy*dy <= radius*radius) {
                int tx = cx + dx, ty = cy + dy;
                if (inBounds(tx, ty)) {
                    at(tx, ty).toxin += amount;
                    if (at(tx, ty).toxin > 1.f) at(tx, ty).toxin = 1.f;
                }
            }
        }
    }
}

void WorldGrid::seedDefault(uint32_t seed) {
    // LCG random number generator
    uint32_t rng = seed;
    auto lcg = [&]() -> float {
        rng = rng * 1664525u + 1013904223u;
        return (float)(rng >> 16) / 65535.f;
    };

    // Set base fertility across the grid
    for (int ty = 0; ty < HEIGHT; ++ty) {
        for (int tx = 0; tx < WIDTH; ++tx) {
            float fert = lcg() * 0.6f + 0.2f;  // [0.2, 0.8]
            Tile& t = at(tx, ty);
            t.fertility = fert;
            t.food      = fert * (lcg() * 0.5f + 0.3f);  // start at 30-80% capacity
            t.oxygen    = 1.f;
            t.toxin     = 0.f;
            t.zoneId    = 0;
        }
    }

    // Place 3 fertile food patches (safe zones)
    int safePatchX[3] = {25, 75, 50};
    int safePatchY[3] = {25, 25, 75};
    for (int i = 0; i < 3; ++i) {
        int px = safePatchX[i], py = safePatchY[i];
        for (int dy = -8; dy <= 8; ++dy) {
            for (int dx = -8; dx <= 8; ++dx) {
                if (dx*dx + dy*dy <= 64) {
                    int tx = px + dx, ty = py + dy;
                    if (inBounds(tx, ty)) {
                        at(tx, ty).fertility = 0.9f;
                        at(tx, ty).food      = 0.85f;
                        at(tx, ty).zoneId    = 1;  // safe
                    }
                }
            }
        }
    }

    // Place 2 hazard zones
    int hazardX[2] = {15, 85};
    int hazardY[2] = {75, 75};
    for (int i = 0; i < 2; ++i) {
        int px = hazardX[i], py = hazardY[i];
        for (int dy = -5; dy <= 5; ++dy) {
            for (int dx = -5; dx <= 5; ++dx) {
                if (dx*dx + dy*dy <= 25) {
                    int tx = px + dx, ty = py + dy;
                    if (inBounds(tx, ty)) {
                        at(tx, ty).toxin     = 0.6f + lcg() * 0.3f;
                        at(tx, ty).fertility = 0.1f;
                        at(tx, ty).food      = 0.f;
                        at(tx, ty).zoneId    = 2;  // hazard
                    }
                }
            }
        }
    }
}
