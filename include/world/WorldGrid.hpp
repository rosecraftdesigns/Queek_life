#pragma once
#include "world/Tile.hpp"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

class WorldGrid {
public:
    static constexpr int WIDTH  = 100;
    static constexpr int HEIGHT = 100;

    WorldGrid();

    void reset();

    // Advance food growth and toxin decay
    void tick(float dt);

    // Tile access
    Tile&       at(int x, int y)       { return m_tiles[idx(x, y)]; }
    const Tile& at(int x, int y) const { return m_tiles[idx(x, y)]; }
    bool inBounds(int x, int y)  const { return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT; }

    // Tile coordinates from world position
    int tileX(float wx) const { return static_cast<int>(wx); }
    int tileY(float wy) const { return static_cast<int>(wy); }

    // Sample food level in a radius around world position (returns [0,1])
    float sampleFoodNearby(glm::vec2 worldPos, float radius) const;

    // Sample toxin level in a radius
    float sampleToxinNearby(glm::vec2 worldPos, float radius) const;

    // Oxygen at a tile coordinate
    float sampleOxygenAt(glm::vec2 worldPos) const;

    // Zone id at world position
    uint8_t zoneAt(glm::vec2 worldPos) const;

    // Consume food at tile (x,y), returns amount actually consumed
    float consumeFood(int x, int y, float amount);

    // Paint a circular zone
    void setZone(int cx, int cy, int radius, uint8_t zoneId);

    // Add food to a circular region
    void addFood(int cx, int cy, int radius, float amount);

    // Add toxin to a circular region
    void addToxin(int cx, int cy, int radius, float amount);

    // Procedural seed
    void seedDefault(uint32_t seed = 12345);

private:
    int idx(int x, int y) const { return y * WIDTH + x; }

    // Logistic food growth per tile
    void growFood(Tile& tile, float dt);

    std::array<Tile, WIDTH * HEIGHT> m_tiles;
};
