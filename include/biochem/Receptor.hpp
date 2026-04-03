#pragma once
// =============================================================================
//  Receptor.hpp  —  Monitors a chemical and translates it into an effect
//
//  A Receptor is the read-side counterpart to an Emitter. It watches a
//  chemical's concentration and, when inside its sensitivity band, produces
//  one of two possible effects:
//
//    EFFECT_EMIT_CHEM   — injects another chemical into the pool
//                         (e.g. high GLUCOSE → emit INSULIN)
//    EFFECT_DRIVE_BRAIN — writes a normalised [0,1] signal to a named
//                         brain-input slot (used in Phase 2)
//
//  Signal equation:
//      normalised = clamp((sense - threshold_lo) / (threshold_hi - threshold_lo), 0, 1)
//      effect_amount = normalised * gain
//
//  Receptors are directional:
//      INCREASING — fires only when concentration is above threshold_lo
//      DECREASING — fires only when concentration is below threshold_hi
//      RANGE      — fires only when inside [threshold_lo, threshold_hi]
// =============================================================================

#include "ChemID.hpp"
#include "ChemicalPool.hpp"
#include <cstdint>
#include <functional>
#include <string>

enum class ReceptorTrigger : uint8_t
{
    ABOVE_THRESHOLD = 0,  // fires when [chem] >= threshold_lo
    BELOW_THRESHOLD = 1,  // fires when [chem] <= threshold_hi
    IN_RANGE        = 2   // fires when threshold_lo <= [chem] <= threshold_hi
};

enum class ReceptorEffect : uint8_t
{
    EMIT_CHEM   = 0,  // inject m_effectChem into pool at computed rate
    DRIVE_BRAIN = 1   // call the registered brain callback with signal [0,1]
};

class Receptor
{
public:
    // Callback type for brain-drive signals: (brain_slot_id, signal 0–1)
    using BrainCallback = std::function<void(uint8_t slot, float signal)>;

    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------

    // Receptor that emits a secondary chemical when primary is high.
    static Receptor makeChemEmitter(
        uint8_t   sense_chemID,
        float     threshold_lo,
        float     threshold_hi,
        ReceptorTrigger trigger,
        uint8_t   effect_chemID,
        float     gain_per_second,
        const std::string& label = "");

    // Receptor that drives a brain input slot.
    static Receptor makeBrainDriver(
        uint8_t   sense_chemID,
        float     threshold_lo,
        float     threshold_hi,
        ReceptorTrigger trigger,
        uint8_t   brain_slot,
        float     gain,
        BrainCallback callback,
        const std::string& label = "");

    // -----------------------------------------------------------------------
    // Simulation
    // -----------------------------------------------------------------------
    void tick(ChemicalPool& pool, float dt) const;

    bool isFiring() const { return m_lastFired; }

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    uint8_t senseChem()   const { return m_senseChem; }
    float   gainValue()   const { return m_gain; }
    void    setGain(float g)    { m_gain = g; }
    const std::string& label()  const { return m_label; }

private:
    // What to watch
    uint8_t         m_senseChem    = Chem::NULL_CHEM;
    float           m_thresholdLo  = 0.0f;
    float           m_thresholdHi  = 255.0f;
    ReceptorTrigger m_trigger      = ReceptorTrigger::ABOVE_THRESHOLD;

    // What to do
    ReceptorEffect  m_effect       = ReceptorEffect::EMIT_CHEM;
    uint8_t         m_effectChem   = Chem::NULL_CHEM;  // for EMIT_CHEM
    uint8_t         m_brainSlot    = 0;                // for DRIVE_BRAIN
    float           m_gain         = 0.0f;

    BrainCallback   m_callback;

    mutable bool    m_lastFired    = false;
    std::string     m_label;
};
