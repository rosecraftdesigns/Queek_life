// =============================================================================
//  main.cpp  —  ALife Sim v0.4
//
//  SDL2 window + SimSystem (creatures, world, biochem, brain, genome).
//  F1-F4: debug overlays  |  Arrows/scroll: pan/zoom
//  Space: pause  |  C: center camera  |  E: next scenario mode
// =============================================================================

#include "sim/SimSystem.hpp"
#include "render/Renderer.hpp"
#include "render/DebugOverlay.hpp"
#include "render/TileRenderer.hpp"
#include "biochem/ChemID.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

static float clamp01(float value) {
    return value < 0.f ? 0.f : (value > 1.f ? 1.f : value);
}

static SDL_Color signalColor(SignalType type) {
    switch (type) {
    case SignalType::HUNGER:  return {255, 180, 60, 220};
    case SignalType::FEAR:    return {255, 80, 80, 220};
    case SignalType::COMFORT: return {90, 230, 200, 220};
    case SignalType::FOOD:    return {180, 255, 90, 220};
    default:                  return {220, 220, 220, 220};
    }
}

static SDL_Color hsvToRgb(float hue, float saturation, float value) {
    hue = std::fmod(std::fmod(hue, 360.f) + 360.f, 360.f);
    saturation = clamp01(saturation);
    value = clamp01(value);

    float c = value * saturation;
    float x = c * (1.f - std::fabs(std::fmod(hue / 60.f, 2.f) - 1.f));
    float m = value - c;

    float r = 0.f, g = 0.f, b = 0.f;
    if (hue < 60.f)      { r = c; g = x; }
    else if (hue < 120.f){ r = x; g = c; }
    else if (hue < 180.f){ g = c; b = x; }
    else if (hue < 240.f){ g = x; b = c; }
    else if (hue < 300.f){ r = x; b = c; }
    else                 { r = c; b = x; }

    auto toU8 = [m](float component) {
        return (Uint8)std::round((component + m) * 255.f);
    };
    return {toU8(r), toU8(g), toU8(b), 255};
}

static void renderSignals(SDL_Renderer* r, const Camera& cam,
                          const std::vector<SignalPulse>& pulses) {
    for (const SignalPulse& pulse : pulses) {
        if (pulse.intensity <= 0.f) continue;

        glm::vec2 sc = cam.worldToScreen(pulse.pos);
        int radius = (int)(pulse.radius * cam.zoom * 1.08f);
        if (radius < 6) radius = 6;

        SDL_Color color = signalColor(pulse.type);
        float ttlNorm = clamp01(pulse.ttl / (pulse.playerAuthored ? 0.35f : 0.28f));
        Uint8 alpha = (Uint8)std::round((75.f + 180.f * ttlNorm) * std::max(0.35f, pulse.intensity));
        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, alpha);
        Renderer::drawCircle(r, (int)sc.x, (int)sc.y, radius);
        Renderer::drawCircle(r, (int)sc.x, (int)sc.y, std::max(4, radius - 2));
        if (cam.zoom >= 4.f) {
            SDL_SetRenderDrawColor(r, color.r, color.g, color.b, (Uint8)std::min(255, alpha + 25));
            Renderer::drawFilledCircle(r, (int)sc.x, (int)sc.y, std::max(2, radius / 9));
        }
    }
}

static std::string uppercaseCopy(const char* text) {
    std::string out = text ? text : "";
    for (char& ch : out)
        ch = (char)std::toupper((unsigned char)ch);
    return out;
}

static std::function<void(float, const std::string&, const std::string&)>
makeEventLogger(bool logRoutineFood) {
    return [logRoutineFood,
            lastFoodLogs = std::vector<std::pair<std::string, float>>{}]
           (float t, const std::string& name, const std::string& ev) mutable {
        if (ev == "ate food") {
            if (!logRoutineFood) return;

            auto it = std::find_if(
                lastFoodLogs.begin(), lastFoodLogs.end(),
                [&](const auto& entry) { return entry.first == name; });
            if (it != lastFoodLogs.end()) {
                if (t - it->second < 2.5f) return;
                it->second = t;
            } else {
                lastFoodLogs.push_back({name, t});
            }
        }

        std::printf("[t=%.1fs] %s: %s\n", t, name.c_str(), ev.c_str());
    };
}

static void renderRuntimePanel(SDL_Renderer* r, int screenW, bool paused,
                               float realDt, float simDt, int simTicksThisFrame,
                               float simTime, const Creature* creature) {
    SDL_SetRenderDrawColor(r, 20, 20, 28, 210);
    SDL_Rect panel{screenW - 286, 28, 276, creature ? 132 : 72};
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, 90, 90, 105, 255);
    SDL_RenderDrawRect(r, &panel);

    char line1[64];
    char line2[64];
    char line3[64];
    std::snprintf(line1, sizeof(line1), "PAUSE %s", paused ? "ON" : "OFF");
    std::snprintf(line2, sizeof(line2), "DT %.3f/%.3f T%d", realDt, simDt, simTicksThisFrame);
    std::snprintf(line3, sizeof(line3), "SIM %.2f", simTime);

    DebugOverlay::drawText(r, panel.x + 8, panel.y + 8, line1, {230, 230, 235, 255}, 2);
    DebugOverlay::drawText(r, panel.x + 8, panel.y + 28, line2, {170, 210, 255, 255}, 2);
    DebugOverlay::drawText(r, panel.x + 8, panel.y + 48, line3, {210, 210, 140, 255}, 2);

    if (!creature) return;

    std::string action = uppercaseCopy(motorActionName(creature->lastAction));
    char line4[64];
    char line5[64];
    char line6[64];
    std::snprintf(line4, sizeof(line4), "ACT %s", action.c_str());
    std::snprintf(line5, sizeof(line5), "POS %.1f %.1f", creature->pos.x, creature->pos.y);
    std::snprintf(line6, sizeof(line6), "VEL %.1f %.1f", creature->vel.x, creature->vel.y);

    DebugOverlay::drawText(r, panel.x + 8, panel.y + 68, line4, {160, 255, 170, 255}, 2);
    DebugOverlay::drawText(r, panel.x + 8, panel.y + 88, line5, {255, 210, 150, 255}, 2);
    DebugOverlay::drawText(r, panel.x + 8, panel.y + 108, line6, {255, 170, 170, 255}, 2);
}

// ---------------------------------------------------------------------------
// Headless demo (--headless flag)
// ---------------------------------------------------------------------------
static void demo_headless() {
    std::printf("\n=== ALife Sim — Headless Demo ===\n");
    SimSystem sim;
    sim.init(12345, 3);
    sim.onEvent = makeEventLogger(true);
    for (int i = 0; i < 2000; ++i) sim.tick(0.05f);  // 100s
    std::printf("Survived creatures: %zu\n\n", sim.creatures().size());
}

// ---------------------------------------------------------------------------
// Render a single creature
// ---------------------------------------------------------------------------
static void renderCreature(SDL_Renderer* r, const Camera& cam,
                            const Creature& c, bool selected,
                            const DebugOverlayFlags& flags)
{
    glm::vec2 sc = cam.worldToScreen(c.pos);
    int cx = (int)sc.x, cy = (int)sc.y;
    int radius = (int)(cam.zoom * c.visualTraits.size / 10.f);
    if (radius < 3) radius = 3;

    float atpRatio = clamp01(c.biochem.pool().get(Chem::ATP) / 200.f);
    float brightness = c.visualTraits.brightness * (0.65f + 0.35f * atpRatio);
    SDL_Color body = hsvToRgb(c.visualTraits.hue, 0.72f, brightness);

    SDL_SetRenderDrawColor(r, body.r, body.g, body.b, 255);
    Renderer::drawFilledCircle(r, cx, cy, radius);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 160);
    switch (c.visualTraits.pattern) {
    case 1:
        SDL_RenderDrawLine(r, cx - radius + 1, cy, cx + radius - 1, cy);
        break;
    case 2:
        Renderer::drawCircle(r, cx, cy, std::max(2, radius / 2));
        break;
    case 3:
        Renderer::drawFilledCircle(r, cx - radius / 2, cy - radius / 3, 1);
        Renderer::drawFilledCircle(r, cx + radius / 3, cy - radius / 4, 1);
        Renderer::drawFilledCircle(r, cx - radius / 4, cy + radius / 3, 1);
        Renderer::drawFilledCircle(r, cx + radius / 4, cy + radius / 4, 1);
        break;
    default:
        break;
    }

    switch (c.visualTraits.appendage) {
    case 1:
        SDL_SetRenderDrawColor(r, 220, 240, 255, 220);
        SDL_RenderDrawLine(r, cx - radius / 3, cy - radius, cx - radius / 2, cy - radius - 4);
        SDL_RenderDrawLine(r, cx + radius / 3, cy - radius, cx + radius / 2, cy - radius - 4);
        break;
    case 2:
        SDL_SetRenderDrawColor(r, 255, 120, 120, 220);
        SDL_RenderDrawLine(r, cx, cy - radius, cx, cy - radius - 4);
        SDL_RenderDrawLine(r, cx + radius, cy, cx + radius + 4, cy);
        SDL_RenderDrawLine(r, cx, cy + radius, cx, cy + radius + 4);
        SDL_RenderDrawLine(r, cx - radius, cy, cx - radius - 4, cy);
        break;
    case 3:
        SDL_SetRenderDrawColor(r, 120, 255, 220, 120);
        Renderer::drawCircle(r, cx, cy, radius + 3);
        break;
    default:
        break;
    }

    // Outline (selection or normal)
    SDL_SetRenderDrawColor(r, selected ? 255 : 160, selected ? 255 : 160,
                              selected ? 80  : 80,  255);
    Renderer::drawCircle(r, cx, cy, radius + 1);

    // F1: ATP glow
    if (flags.showChemGrid) {
        int glowR = (int)((0.75f + c.visualTraits.glowIntensity * 1.5f) * cam.zoom);
        SDL_SetRenderDrawColor(r, 80, 180, 255, (Uint8)std::round(60.f + 90.f * c.visualTraits.glowIntensity * atpRatio));
        Renderer::drawCircle(r, cx, cy, glowR);
    }

    // F4: perception radius
    if (flags.showPaths) {
        int percR = (int)(c.phenotype.perceptionRadius * cam.zoom);
        SDL_SetRenderDrawColor(r, 255, 220, 80, 60);
        Renderer::drawCircle(r, cx, cy, percR);
        // Velocity arrow
        if (glm::length(c.vel) > 0.01f) {
            glm::vec2 tip = cam.worldToScreen(c.pos + glm::normalize(c.vel) * 1.5f);
            SDL_SetRenderDrawColor(r, 200, 200, 80, 200);
            SDL_RenderDrawLine(r, cx, cy, (int)tip.x, (int)tip.y);
        }
    }

    if (cam.zoom >= 4.f && c.dominantSignalIntensity > 0.05f) {
        SDL_Color dot = signalColor(c.dominantSignal);
        SDL_SetRenderDrawColor(r, dot.r, dot.g, dot.b, 220);
        Renderer::drawFilledCircle(r, cx, cy - radius - 4, 2);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--headless") {
        demo_headless();
        return 0;
    }

    Renderer renderer;
    if (!renderer.init("ALife Sim v0.4  [F1-F4: overlays | Arrows: pan | +/-: zoom | Space: pause]",
                       1280, 720)) return 1;

    TileRenderer  tileRender;
    DebugOverlay  overlay;

    SimSystem sim;
    sim.init(12345, 6);
    sim.onEvent = makeEventLogger(false);

    // Center camera on world
    renderer.camera().position = {50.f, 50.f};
    renderer.camera().zoom     = 7.f;

    uint64_t selectedId = 0;
    bool     paused     = false;
    float    playerSignalCooldown = 0.f;
    float    lastRealDt = 0.f;
    float    lastAppliedSimDt = 0.f;
    int      lastSimTicksThisFrame = 0;

    const float SIM_DT    = 0.05f;
    float       accumulator = 0.f;
    Uint64      lastTime  = SDL_GetPerformanceCounter();
    Uint64      freq      = SDL_GetPerformanceFrequency();

    bool panning = false;
    glm::vec2 panStart{0.f, 0.f};

    bool quit = false;
    SDL_Event event;

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: quit = true; break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: quit = true; break;
                case SDLK_F1: overlay.toggle(1); break;
                case SDLK_F2: overlay.toggle(2); break;
                case SDLK_F3: overlay.toggle(3); break;
                case SDLK_F4: overlay.toggle(4); break;
                case SDLK_SPACE: paused = !paused; break;
                case SDLK_EQUALS: case SDLK_KP_PLUS:
                    renderer.camera().zoomAt(
                        {renderer.screenW()*0.5f, renderer.screenH()*0.5f}, 1.25f); break;
                case SDLK_MINUS: case SDLK_KP_MINUS:
                    renderer.camera().zoomAt(
                        {renderer.screenW()*0.5f, renderer.screenH()*0.5f}, 0.8f);  break;
                case SDLK_LEFT:  renderer.camera().pan({-30.f, 0.f}); break;
                case SDLK_RIGHT: renderer.camera().pan({ 30.f, 0.f}); break;
                case SDLK_UP:    renderer.camera().pan({0.f, -30.f}); break;
                case SDLK_DOWN:  renderer.camera().pan({0.f,  30.f}); break;
                case SDLK_c:     case SDLK_HOME:
                    renderer.camera().position = {50.f, 50.f};
                    renderer.camera().zoom     = 7.f; break;
                case SDLK_e: {
                    int next = ((int)sim.scenarioMode() + 1) % 5;
                    sim.setScenarioMode((ScenarioMode)next);
                    std::printf("Scenario: %d\n", next);
                    break;
                }
                // Spawn creature at world center
                case SDLK_s:
                    sim.spawnCreature({50.f + (float)(rand() % 10 - 5),
                                       50.f + (float)(rand() % 10 - 5)});
                    break;
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                    if (!event.key.repeat && playerSignalCooldown <= 0.f) {
                        SignalType type = SignalType::HUNGER;
                        if (event.key.keysym.sym == SDLK_2) type = SignalType::FEAR;
                        if (event.key.keysym.sym == SDLK_3) type = SignalType::COMFORT;
                        if (event.key.keysym.sym == SDLK_4) type = SignalType::FOOD;

                        glm::vec2 origin;
                        if (Creature* sel = selectedId != 0 ? sim.findCreature(selectedId) : nullptr) {
                            origin = sel->pos;
                        } else {
                            int mx, my; SDL_GetMouseState(&mx, &my);
                            origin = renderer.camera().screenToWorld({(float)mx, (float)my});
                        }
                        sim.queuePlayerSignal(origin, type, 1.f);
                        playerSignalCooldown = 0.25f;
                    }
                    break;
                default: break;
                }
                break;
            case SDL_MOUSEWHEEL: {
                int mx, my; SDL_GetMouseState(&mx, &my);
                float f = event.wheel.y > 0 ? 1.15f : 0.87f;
                renderer.camera().zoomAt({(float)mx, (float)my}, f);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_MIDDLE ||
                    event.button.button == SDL_BUTTON_RIGHT) {
                    panning = true;
                    panStart = {(float)event.button.x, (float)event.button.y};
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int overlayButton = overlay.buttonAt(
                        event.button.x, event.button.y,
                        renderer.screenW(), renderer.screenH());
                    if (overlayButton != 0) {
                        overlay.toggle(overlayButton);
                        break;
                    }
                    // Click to select nearest creature
                    glm::vec2 worldClick = renderer.camera().screenToWorld(
                        {(float)event.button.x, (float)event.button.y});
                    float bestDist = 3.f;  // max selection radius in tiles
                    selectedId = 0;
                    for (const auto& c : sim.creatures()) {
                        float d = glm::length(c->pos - worldClick);
                        if (d < bestDist) { bestDist = d; selectedId = c->id; }
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_MIDDLE ||
                    event.button.button == SDL_BUTTON_RIGHT) panning = false;
                break;
            case SDL_MOUSEMOTION:
                if (panning) {
                    glm::vec2 now{(float)event.motion.x, (float)event.motion.y};
                    renderer.camera().pan(panStart - now);
                    panStart = now;
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    renderer.camera().screenW = event.window.data1;
                    renderer.camera().screenH = event.window.data2;
                }
                break;
            default: break;
            }
        }

        // Sim ticks
        Uint64 now = SDL_GetPerformanceCounter();
        float real = (float)(now - lastTime) / (float)freq;
        lastTime   = now;
        if (real > 0.1f) real = 0.1f;
        lastRealDt = real;
        accumulator += real;
        if (playerSignalCooldown > 0.f) playerSignalCooldown -= real;
        lastAppliedSimDt = 0.f;
        lastSimTicksThisFrame = 0;

        if (!paused) {
            while (accumulator >= SIM_DT) {
                sim.tick(SIM_DT);
                accumulator -= SIM_DT;
                lastAppliedSimDt = SIM_DT;
                ++lastSimTicksThisFrame;
            }
        } else {
            accumulator = 0.f;
        }

        // Render
        renderer.beginFrame();

        tileRender.render(renderer.sdlRenderer(), sim.grid(),
                          renderer.camera(), overlay.flags);
        renderSignals(renderer.sdlRenderer(), renderer.camera(), sim.activeSignals());

        for (const auto& c : sim.creatures()) {
            renderCreature(renderer.sdlRenderer(), renderer.camera(),
                           *c, c->id == selectedId, overlay.flags);
        }

        // F2: top drive bars
        if (overlay.flags.showBrainState && selectedId != 0) {
            if (const Creature* sel = sim.findCreature(selectedId)) {
                float atp  = sel->biochem.pool().get(Chem::ATP) / 200.f;
                float fear = sel->biochem.pool().get(Chem::FEAR) / 255.f;
                SDL_SetRenderDrawColor(renderer.sdlRenderer(), 40, 160, 255, 200);
                SDL_Rect a{0,0,(int)(atp*renderer.screenW()),6}; SDL_RenderFillRect(renderer.sdlRenderer(),&a);
                SDL_SetRenderDrawColor(renderer.sdlRenderer(), 200, 40, 40, 200);
                SDL_Rect f{0,7,(int)(fear*renderer.screenW()),6}; SDL_RenderFillRect(renderer.sdlRenderer(),&f);
            }
        }

        // F3: hunger bar
        if (overlay.flags.showGenome && selectedId != 0) {
            if (const Creature* sel = sim.findCreature(selectedId)) {
                float hunger = sel->biochem.pool().get(Chem::HUNGER_CARB) / 255.f;
                SDL_SetRenderDrawColor(renderer.sdlRenderer(), 255, 160, 40, 200);
                SDL_Rect h{0,14,(int)(hunger*renderer.screenW()),6}; SDL_RenderFillRect(renderer.sdlRenderer(),&h);
            }
        }

        // Population count (top-left indicator)
        {
            SDL_SetRenderDrawColor(renderer.sdlRenderer(), 20, 20, 30, 180);
            SDL_Rect bg{4, 4, 80, 14}; SDL_RenderFillRect(renderer.sdlRenderer(), &bg);
            // Draw a bar proportional to population (max 50)
            int pop = (int)sim.creatures().size();
            SDL_SetRenderDrawColor(renderer.sdlRenderer(), 100, 200, 100, 220);
            SDL_Rect popBar{5, 5, std::min(78, pop * 78 / 50), 12};
            SDL_RenderFillRect(renderer.sdlRenderer(), &popBar);
        }

        overlay.renderHUD(renderer.sdlRenderer(), renderer.screenW(), renderer.screenH());

        const Creature* debugCreature = nullptr;
        if (selectedId != 0)
            debugCreature = sim.findCreature(selectedId);
        if (!debugCreature && !sim.creatures().empty())
            debugCreature = sim.creatures().front().get();
        renderRuntimePanel(renderer.sdlRenderer(), renderer.screenW(), paused,
                           lastRealDt, lastAppliedSimDt, lastSimTicksThisFrame,
                           sim.simTime(), debugCreature);

        if (paused) {
            SDL_SetRenderDrawColor(renderer.sdlRenderer(), 255, 200, 50, 180);
            SDL_Rect pbar{renderer.screenW()/2 - 40, 4, 80, 14};
            SDL_RenderFillRect(renderer.sdlRenderer(), &pbar);
            DebugOverlay::drawText(renderer.sdlRenderer(), pbar.x + 12, pbar.y + 3,
                                   "PAUSED", {35, 25, 12, 255}, 1);
        }

        renderer.endFrame();
    }

    return 0;
}
