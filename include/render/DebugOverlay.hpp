#pragma once
#include <SDL2/SDL.h>
#include "Camera.hpp"

struct DebugOverlayFlags {
    bool showChemGrid   = false;  // F1: ATP glow / chemical heatmap
    bool showBrainState = false;  // F2: fear heatmap / brain decisions
    bool showGenome     = false;  // F3: food density
    bool showPaths      = false;  // F4: perception radius / velocity
};

class DebugOverlay {
public:
    DebugOverlayFlags flags;

    // Toggle a layer by F-key number (1-4)
    void toggle(int fKey) {
        switch (fKey) {
            case 1: flags.showChemGrid   = !flags.showChemGrid;   break;
            case 2: flags.showBrainState = !flags.showBrainState; break;
            case 3: flags.showGenome     = !flags.showGenome;     break;
            case 4: flags.showPaths      = !flags.showPaths;      break;
            default: break;
        }
    }

    int buttonAt(int x, int y, int /*screenW*/, int screenH) const {
        constexpr int buttonWidth = 52;
        constexpr int buttonHeight = 24;
        constexpr int buttonPad = 6;
        constexpr int startX = 10;
        constexpr int marginBottom = 10;

        for (int i = 0; i < 4; ++i) {
            int bx = startX + i * (buttonWidth + buttonPad);
            int by = screenH - buttonHeight - marginBottom;
            if (x >= bx && x < bx + buttonWidth &&
                y >= by && y < by + buttonHeight) {
                return i + 1;
            }
        }
        return 0;
    }

    static void drawText(SDL_Renderer* renderer, int x, int y, const char* text,
                         SDL_Color color, int scale = 2);

    // Render the active HUD overlay (key legend in corner)
    void renderHUD(SDL_Renderer* renderer, int screenW, int screenH) const;
};
