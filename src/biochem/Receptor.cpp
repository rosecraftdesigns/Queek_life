#include "biochem/Receptor.hpp"
#include <algorithm>

// =============================================================================
//  Static construction helpers
// =============================================================================

Receptor Receptor::makeChemEmitter(
    uint8_t   sense_chemID,
    float     threshold_lo,
    float     threshold_hi,
    ReceptorTrigger trigger,
    uint8_t   effect_chemID,
    float     gain_per_second,
    const std::string& label)
{
    Receptor r;
    r.m_senseChem   = sense_chemID;
    r.m_thresholdLo = threshold_lo;
    r.m_thresholdHi = threshold_hi;
    r.m_trigger     = trigger;
    r.m_effect      = ReceptorEffect::EMIT_CHEM;
    r.m_effectChem  = effect_chemID;
    r.m_gain        = gain_per_second;
    r.m_label       = label;
    return r;
}

Receptor Receptor::makeBrainDriver(
    uint8_t   sense_chemID,
    float     threshold_lo,
    float     threshold_hi,
    ReceptorTrigger trigger,
    uint8_t   brain_slot,
    float     gain,
    BrainCallback callback,
    const std::string& label)
{
    Receptor r;
    r.m_senseChem   = sense_chemID;
    r.m_thresholdLo = threshold_lo;
    r.m_thresholdHi = threshold_hi;
    r.m_trigger     = trigger;
    r.m_effect      = ReceptorEffect::DRIVE_BRAIN;
    r.m_brainSlot   = brain_slot;
    r.m_gain        = gain;
    r.m_callback    = callback;
    r.m_label       = label;
    return r;
}

// =============================================================================
//  tick
// =============================================================================
void Receptor::tick(ChemicalPool& pool, float dt) const
{
    float sense = pool.get(m_senseChem);

    // Evaluate trigger condition
    bool fires = false;
    switch (m_trigger)
    {
        case ReceptorTrigger::ABOVE_THRESHOLD:
            fires = (sense >= m_thresholdLo);
            break;
        case ReceptorTrigger::BELOW_THRESHOLD:
            fires = (sense <= m_thresholdHi);
            break;
        case ReceptorTrigger::IN_RANGE:
            fires = (sense >= m_thresholdLo && sense <= m_thresholdHi);
            break;
    }

    m_lastFired = fires;
    if (!fires) return;

    // Compute normalised signal strength within the active band
    float range = m_thresholdHi - m_thresholdLo;
    float signal = 0.0f;
    if (range > 0.0f)
    {
        signal = (sense - m_thresholdLo) / range;
        signal = std::max(0.0f, std::min(1.0f, signal));
    }
    else
    {
        signal = 1.0f;  // degenerate: single threshold, fire at full strength
    }

    float magnitude = signal * m_gain;

    switch (m_effect)
    {
        case ReceptorEffect::EMIT_CHEM:
            pool.add(m_effectChem, magnitude * dt);
            break;

        case ReceptorEffect::DRIVE_BRAIN:
            if (m_callback)
                m_callback(m_brainSlot, magnitude);
            break;
    }
}
