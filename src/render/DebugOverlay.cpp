#include "render/DebugOverlay.hpp"

#include <algorithm>
#include <cctype>

namespace {

constexpr int kButtonWidth = 52;
constexpr int kButtonHeight = 24;
constexpr int kButtonPad = 6;
constexpr int kButtonStartX = 10;
constexpr int kButtonMarginBottom = 10;

SDL_Rect buttonRect(int index, int screenH) {
    return SDL_Rect{
        kButtonStartX + index * (kButtonWidth + kButtonPad),
        screenH - kButtonHeight - kButtonMarginBottom,
        kButtonWidth,
        kButtonHeight
    };
}

const uint8_t* glyphRows(char ch) {
    static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t minus[7] = {0, 0, 0, 0x0E, 0, 0, 0};
    static const uint8_t period[7] = {0, 0, 0, 0, 0, 0x0C, 0x0C};
    static const uint8_t colon[7] = {0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0};

    static const uint8_t digits[10][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, // 3
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
        {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}, // 5
        {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}  // 9
    };

    static const uint8_t letters[26][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
        {0x0E, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0F}, // G
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
        {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}, // J
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, // S
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, // W
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}  // Z
    };

    if (ch == ' ') return space;
    if (ch == '-') return minus;
    if (ch == '.') return period;
    if (ch == ':') return colon;
    if (ch >= '0' && ch <= '9') return digits[ch - '0'];
    if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'];
    return space;
}

void drawGlyph(SDL_Renderer* renderer, int x, int y, char ch, SDL_Color color, int scale) {
    const uint8_t* rows = glyphRows((char)std::toupper((unsigned char)ch));
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((rows[row] >> (4 - col)) & 1) {
                SDL_Rect pixel{x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }
}

} // namespace

void DebugOverlay::drawText(SDL_Renderer* renderer, int x, int y, const char* text,
                            SDL_Color color, int scale) {
    if (!text || scale <= 0) return;

    int penX = x;
    for (const char* ch = text; *ch != '\0'; ++ch) {
        drawGlyph(renderer, penX, y, *ch, color, scale);
        penX += 6 * scale;
    }
}

void DebugOverlay::renderHUD(SDL_Renderer* renderer, int screenW, int screenH) const {
    const char* labels[4] = {"ATP", "FEAR", "FOOD", "PATH"};
    SDL_Color colors[4] = {
        {80,  200, 255, 255},
        {255, 80,  80,  255},
        {80,  255, 80,  255},
        {255, 220, 80,  255}
    };
    bool active[4] = {
        flags.showChemGrid,
        flags.showBrainState,
        flags.showGenome,
        flags.showPaths
    };

    for (int i = 0; i < 4; ++i) {
        SDL_Rect rect = buttonRect(i, screenH);
        SDL_Color base = colors[i];
        SDL_SetRenderDrawColor(renderer,
            active[i] ? base.r : 36,
            active[i] ? base.g : 36,
            active[i] ? base.b : 36,
            210);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 190, 190, 200, 255);
        SDL_RenderDrawRect(renderer, &rect);

        SDL_Color textColor = active[i] ? SDL_Color{20, 20, 24, 255} : SDL_Color{200, 200, 210, 255};
        drawText(renderer, rect.x + 6, rect.y + 5, labels[i], textColor, 2);
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 180);
    SDL_Rect statusBar{screenW - 132, 4, 128, 18};
    SDL_RenderFillRect(renderer, &statusBar);
    drawText(renderer, statusBar.x + 6, statusBar.y + 4, "F1 F2 F3 F4", {210, 210, 220, 255}, 1);
}
