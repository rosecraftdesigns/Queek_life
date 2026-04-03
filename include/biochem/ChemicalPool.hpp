#pragma once
// =============================================================================
//  ChemicalPool.hpp  —  256-slot floating-point chemical concentration store
//
//  All concentrations are 32-bit floats clamped to [0, 255].
//  Decay is modelled as first-order kinetics:
//      C(t+dt) = C(t) * exp(-k * dt)   ≈   C(t) * (1 - k*dt)  for small dt
//  where k is the per-second decay rate stored per chemical.
// =============================================================================

#include "ChemID.hpp"
#include <array>
#include <cstdint>
#include <cstdio>

class ChemicalPool
{
public:
    static constexpr int   NUM_CHEMS        = 256;
    static constexpr float CONCENTRATION_MAX = 255.0f;
    static constexpr float CONCENTRATION_MIN = 0.0f;

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    ChemicalPool();

    // Zero all concentrations and reset decay rates to defaults.
    void reset();

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    inline float get(uint8_t id) const { return m_levels[id]; }

    // Set concentration; clamped to [0, 255].
    void set(uint8_t id, float value);

    // Add delta (may be negative); result clamped to [0, 255].
    // Returns the amount actually added (respects clamping).
    float add(uint8_t id, float delta);

    // Attempt to remove 'amount'; returns amount actually removed.
    // Never makes concentration go negative.
    float consume(uint8_t id, float amount);

    // -----------------------------------------------------------------------
    // Decay
    // -----------------------------------------------------------------------
    // Apply first-order decay to every chemical for elapsed time dt (seconds).
    void decay(float dt);

    // Set the per-second decay rate for chemical 'id'.
    // 0.0f = no decay, 1.0f = gone in ~1 s, typical hormones ~0.05–0.3f.
    void setDecayRate(uint8_t id, float ratePerSecond);
    float getDecayRate(uint8_t id) const { return m_decayRates[id]; }

    // -----------------------------------------------------------------------
    // Debug / Scientist Mode
    // -----------------------------------------------------------------------
    // Print all non-zero chemicals (and any that match a forced list).
    void dump(const char* label = nullptr) const;

    // Print a single chemical's concentration with its name.
    void dumpOne(uint8_t id) const;

private:
    // Clamp helper
    static float clamp(float v, float lo, float hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    std::array<float, NUM_CHEMS> m_levels;      // current concentrations
    std::array<float, NUM_CHEMS> m_decayRates;  // per-second first-order rate
};
