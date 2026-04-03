#pragma once
#include <SDL2/SDL.h>
#include "Camera.hpp"

class Renderer {
public:
    Renderer() = default;
    ~Renderer() { shutdown(); }

    // Returns false if SDL init or window creation fails
    bool init(const char* title, int w, int h);
    void shutdown();

    void beginFrame();
    void endFrame();

    SDL_Renderer* sdlRenderer() { return m_renderer; }
    const Camera& camera()      const { return m_camera; }
    Camera&       camera()            { return m_camera; }

    int screenW() const { return m_w; }
    int screenH() const { return m_h; }

    // Helper: draw a filled circle
    static void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int radius);

    // Helper: draw a hollow circle (Bresenham)
    static void drawCircle(SDL_Renderer* r, int cx, int cy, int radius);

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    Camera        m_camera;
    int           m_w = 1280;
    int           m_h = 720;
};
