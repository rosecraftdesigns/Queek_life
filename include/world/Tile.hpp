#pragma once
#include <cstdint>

struct Tile {
    float   food      = 0.f;    // [0, 1] food resource level
    float   toxin     = 0.f;    // [0, 1] toxin level
    float   oxygen    = 1.f;    // [0, 1] local oxygen availability
    float   fertility = 0.5f;   // [0, 1] controls food regrowth rate
    uint8_t zoneId    = 0;      // 0=neutral, 1=safe, 2=hazard
    bool    solid     = false;  // wall/obstacle
};
