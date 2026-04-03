#pragma once
#include <cstdint>
#include <glm/glm.hpp>

enum class SignalType : uint8_t {
    HUNGER = 0,
    FEAR = 1,
    COMFORT = 2,
    FOOD = 3,
};

static constexpr int SIGNAL_TYPE_COUNT = 4;
static constexpr int MAX_RECENT_SIGNAL_OBSERVATIONS = 8;
static constexpr int MAX_SOCIAL_MEMORY_ENTRIES = 16;

inline constexpr int signalTypeIndex(SignalType type) {
    return static_cast<int>(type);
}

struct SignalPulse {
    uint64_t    emitterId      = 0;
    SignalType  type           = SignalType::HUNGER;
    glm::vec2   pos            = {0.f, 0.f};
    float       radius         = 0.f;
    float       intensity      = 0.f;
    float       ttl            = 0.f;
    bool        playerAuthored = false;
};

struct SignalAssociation {
    float positive = 0.f;
    float negative = 0.f;
};

struct RecentSignalObservation {
    SignalType type      = SignalType::HUNGER;
    uint64_t   emitterId = 0;
    float      strength  = 0.f;
    float      simTime   = 0.f;
};

struct SocialMemory {
    uint64_t otherId             = 0;
    float    valence             = 0.f;
    float    lastInteractionTime = 0.f;
};

struct VisualTraits {
    float size          = 4.f;
    float hue           = 0.f;
    float brightness    = 0.7f;
    int   pattern       = 0;
    int   appendage     = 0;
    float glowIntensity = 0.f;
};
