#pragma once
#include <SDL2/SDL.h>
#include "render/Camera.hpp"
#include "render/DebugOverlay.hpp"
#include "world/WorldGrid.hpp"

class TileRenderer {
public:
    // Render the world grid using colored rects scaled by camera zoom
    void render(SDL_Renderer* r, const WorldGrid& grid,
                const Camera& cam, const DebugOverlayFlags& flags) const;

private:
    SDL_Color tileColor(const Tile& t, const DebugOverlayFlags& flags) const;
};
