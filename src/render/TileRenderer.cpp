#include "render/TileRenderer.hpp"
#include <algorithm>
#include <cmath>

SDL_Color TileRenderer::tileColor(const Tile& t, const DebugOverlayFlags& flags) const {
    if (flags.showChemGrid) {
        // F1: food heatmap — dark to bright green
        Uint8 g = (Uint8)(t.food * 200.f);
        Uint8 r = (Uint8)(t.toxin * 180.f);
        return {r, g, 20, 255};
    }
    if (flags.showBrainState) {
        // F2: fear/toxin heatmap — dark to red
        Uint8 r = (Uint8)(t.toxin * 220.f + 15.f);
        Uint8 b = t.zoneId == 1 ? 160 : 20;
        return {r, 20, b, 255};
    }
    if (flags.showGenome) {
        // F3: food density (fertility) — green shades
        Uint8 g = (Uint8)(t.fertility * 180.f + 20.f);
        Uint8 b = (Uint8)(t.food * 120.f);
        return {15, g, b, 255};
    }

    // Default: natural-looking ground
    // Zone tinting
    Uint8 rb = 18, gb = 30, bb = 18;
    if (t.zoneId == 1) { rb = 15; gb = 40; bb = 25; }       // safe  = teal tint
    if (t.zoneId == 2) { rb = 45; gb = 18; bb = 18; }       // hazard = red tint

    // Overlay food as slight brightness increase
    Uint8 foodBoost = (Uint8)(t.food * 30.f);
    Uint8 toxBoost  = (Uint8)(t.toxin * 25.f);

    return {
        static_cast<Uint8>(std::min(255, rb + toxBoost)),
        static_cast<Uint8>(std::min(255, gb + foodBoost)),
        static_cast<Uint8>(bb),
        255
    };
}

void TileRenderer::render(SDL_Renderer* r, const WorldGrid& grid,
                           const Camera& cam, const DebugOverlayFlags& flags) const {
    // Compute visible tile range with 1-tile margin
    glm::vec2 topLeft  = cam.screenToWorld({0.f, 0.f});
    glm::vec2 botRight = cam.screenToWorld({(float)cam.screenW, (float)cam.screenH});

    int x0 = std::max(0,                  (int)topLeft.x);
    int y0 = std::max(0,                  (int)topLeft.y);
    int x1 = std::min(WorldGrid::WIDTH  - 1, (int)botRight.x + 1);
    int y1 = std::min(WorldGrid::HEIGHT - 1, (int)botRight.y + 1);

    int tilePixels = (int)cam.zoom;
    if (tilePixels < 1) tilePixels = 1;

    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            const Tile& tile = grid.at(tx, ty);
            glm::vec2 screenPos = cam.worldToScreen({(float)tx, (float)ty});
            SDL_Rect rect{(int)screenPos.x, (int)screenPos.y,
                          tilePixels, tilePixels};

            SDL_Color col = tileColor(tile, flags);
            SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
            SDL_RenderFillRect(r, &rect);
        }
    }

    // Grid lines (only when zoomed in enough)
    if (cam.zoom >= 6.f) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 60);
        for (int tx = x0; tx <= x1 + 1; ++tx) {
            glm::vec2 s = cam.worldToScreen({(float)tx, (float)y0});
            glm::vec2 e = cam.worldToScreen({(float)tx, (float)(y1 + 1)});
            SDL_RenderDrawLine(r, (int)s.x, (int)s.y, (int)e.x, (int)e.y);
        }
        for (int ty = y0; ty <= y1 + 1; ++ty) {
            glm::vec2 s = cam.worldToScreen({(float)x0, (float)ty});
            glm::vec2 e = cam.worldToScreen({(float)(x1 + 1), (float)ty});
            SDL_RenderDrawLine(r, (int)s.x, (int)s.y, (int)e.x, (int)e.y);
        }
    }
}
