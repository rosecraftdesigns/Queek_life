#pragma once
#include <glm/glm.hpp>

struct Camera {
    glm::vec2 position = {50.f, 50.f};  // world-space center (tile units)
    float     zoom     = 8.f;           // pixels per tile
    int       screenW  = 1280;
    int       screenH  = 720;

    // Convert a world position (tile units) to screen pixels
    glm::vec2 worldToScreen(glm::vec2 worldPos) const {
        glm::vec2 rel = worldPos - position;
        return {
            screenW * 0.5f + rel.x * zoom,
            screenH * 0.5f + rel.y * zoom
        };
    }

    // Convert screen pixels to world position (tile units)
    glm::vec2 screenToWorld(glm::vec2 screenPos) const {
        return position + glm::vec2{
            (screenPos.x - screenW * 0.5f) / zoom,
            (screenPos.y - screenH * 0.5f) / zoom
        };
    }

    // Pan camera by screen-pixel delta
    void pan(glm::vec2 screenDelta) {
        position -= screenDelta / zoom;
    }

    // Zoom in/out around a screen-space pivot point
    void zoomAt(glm::vec2 screenPivot, float factor) {
        glm::vec2 worldPivot = screenToWorld(screenPivot);
        zoom *= factor;
        if (zoom < 2.f)  zoom = 2.f;
        if (zoom > 64.f) zoom = 64.f;
        // Re-anchor so pivot stays fixed on screen
        glm::vec2 newScreen = worldToScreen(worldPivot);
        position += (newScreen - screenPivot) / zoom;
    }
};
