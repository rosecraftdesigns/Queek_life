#include "biochem/Emitter.hpp"

// =============================================================================
//  Static construction helpers
// =============================================================================

Emitter Emitter::makeChemTriggered(
    uint8_t sense_chemID,
    float   threshold_lo,
    float   threshold_hi,
    uint8_t output_chemID,
    float   gain_per_second,
    const std::string& label)
{
    Emitter e;
    e.m_source      = EmitterSource::SOURCE_CHEMICAL;
    e.m_senseChem   = sense_chemID;
    e.m_thresholdLo = threshold_lo;
    e.m_thresholdHi = threshold_hi;
    e.m_outputChem  = output_chemID;
    e.m_gain        = gain_per_second;
    e.m_label       = label;
    return e;
}

Emitter Emitter::makeConstant(
    uint8_t output_chemID,
    float   gain_per_second,
    const std::string& label)
{
    Emitter e;
    e.m_source     = EmitterSource::SOURCE_CONSTANT;
    e.m_outputChem = output_chemID;
    e.m_gain       = gain_per_second;
    e.m_label      = label;
    return e;
}

Emitter Emitter::makeExternal(
    float   threshold_lo,
    float   threshold_hi,
    uint8_t output_chemID,
    float   gain_per_second,
    const std::string& label)
{
    Emitter e;
    e.m_source      = EmitterSource::SOURCE_EXTERNAL;
    e.m_thresholdLo = threshold_lo;
    e.m_thresholdHi = threshold_hi;
    e.m_outputChem  = output_chemID;
    e.m_gain        = gain_per_second;
    e.m_label       = label;
    return e;
}

// =============================================================================
//  tick
// =============================================================================
void Emitter::tick(ChemicalPool& pool, float dt) const
{
    float senseValue = 0.0f;

    switch (m_source)
    {
        case EmitterSource::SOURCE_CHEMICAL:
            senseValue = pool.get(m_senseChem);
            break;
        case EmitterSource::SOURCE_CONSTANT:
            // Always fires — emit unconditionally.
            pool.add(m_outputChem, m_gain * dt);
            m_lastFired = true;
            return;
        case EmitterSource::SOURCE_EXTERNAL:
            senseValue = m_externalValue;
            break;
    }

    // Check threshold condition
    bool inBand = (senseValue >= m_thresholdLo && senseValue <= m_thresholdHi);
    m_lastFired = inBand;

    if (inBand)
    {
        pool.add(m_outputChem, m_gain * dt);
    }
}
