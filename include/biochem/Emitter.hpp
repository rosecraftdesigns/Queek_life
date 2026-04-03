#pragma once
// =============================================================================
//  Emitter.hpp  —  Organ that injects a chemical into the pool
//
//  An Emitter models a physiological source of a chemical — e.g.
//    • The stomach releasing glucose after digesting food
//    • Muscles releasing lactate under exertion
//    • Adrenal glands spiking adrenaline on threat detection
//
//  Firing condition:
//      emit when  threshold_lo <= sense_value <= threshold_hi
//
//  The "sense_value" can be one of three sources (EmitterSource):
//    SOURCE_CHEMICAL   — another chemical's current concentration
//    SOURCE_CONSTANT   — always fires (unconditional drip)
//    SOURCE_EXTERNAL   — set externally each tick by game engine / organ code
//
//  Output:
//      pool[outputChem] += gain * dt   (clamped)
// =============================================================================

#include "ChemID.hpp"
#include "ChemicalPool.hpp"
#include <cstdint>
#include <string>

enum class EmitterSource : uint8_t
{
    SOURCE_CHEMICAL  = 0,  // sense the concentration of another chemical
    SOURCE_CONSTANT  = 1,  // always active (unconditional trickle)
    SOURCE_EXTERNAL  = 2   // game code sets m_externalValue each tick
};

class Emitter
{
public:
    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------

    // Emitter that fires based on another chemical's level.
    static Emitter makeChemTriggered(
        uint8_t sense_chemID,
        float   threshold_lo,
        float   threshold_hi,
        uint8_t output_chemID,
        float   gain_per_second,
        const std::string& label = "");

    // Unconditional constant trickle (e.g. baseline oxygen supply).
    static Emitter makeConstant(
        uint8_t output_chemID,
        float   gain_per_second,
        const std::string& label = "");

    // Externally triggered (game sets senseValue before tick).
    static Emitter makeExternal(
        float   threshold_lo,
        float   threshold_hi,
        uint8_t output_chemID,
        float   gain_per_second,
        const std::string& label = "");

    // -----------------------------------------------------------------------
    // External sense value (used when source == SOURCE_EXTERNAL)
    // -----------------------------------------------------------------------
    void setSenseValue(float v) { m_externalValue = v; }
    float getSenseValue()  const { return m_externalValue; }

    // -----------------------------------------------------------------------
    // Simulation
    // -----------------------------------------------------------------------
    // Evaluate firing condition; if met, inject chemical for dt seconds.
    void tick(ChemicalPool& pool, float dt) const;

    // Is the emitter currently active? (last evaluated state)
    bool  isFiring()          const { return m_lastFired; }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    uint8_t    outputChem()   const { return m_outputChem; }
    float      gain()         const { return m_gain; }
    void       setGain(float g)     { m_gain = g; }
    const std::string& label()const { return m_label; }

private:
    EmitterSource m_source       = EmitterSource::SOURCE_CONSTANT;

    uint8_t   m_senseChem        = Chem::NULL_CHEM;
    float     m_thresholdLo      = 0.0f;
    float     m_thresholdHi      = 255.0f;
    float     m_externalValue    = 0.0f;

    uint8_t   m_outputChem       = Chem::NULL_CHEM;
    float     m_gain             = 0.0f;   // units/second emitted when firing

    mutable bool m_lastFired     = false;
    std::string  m_label;
};
