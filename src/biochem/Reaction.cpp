#include "biochem/Reaction.hpp"
#include <algorithm>
#include <cmath>

// =============================================================================
//  Static construction helpers
// =============================================================================

Reaction Reaction::make(
    uint8_t r0, float s_r0,
    uint8_t r1, float s_r1,
    uint8_t p0, float s_p0,
    uint8_t p1, float s_p1,
    float   kcat,
    const std::string& label)
{
    Reaction rxn;
    rxn.m_r0    = {r0, s_r0};
    rxn.m_r1    = {r1, s_r1};
    rxn.m_p0    = {p0, s_p0};
    rxn.m_p1    = {p1, s_p1};
    rxn.m_kcat  = kcat;
    rxn.m_label = label;
    return rxn;
}

Reaction Reaction::makeUnary(
    uint8_t reactant, float s_r,
    uint8_t p0, float s_p0,
    uint8_t p1, float s_p1,
    float   kcat,
    const std::string& label)
{
    Reaction rxn;
    rxn.m_r0    = {reactant, s_r};
    rxn.m_r1    = {Chem::NULL_CHEM, 0.0f};   // no second reactant
    rxn.m_p0    = {p0, s_p0};
    rxn.m_p1    = {p1, s_p1};
    rxn.m_kcat  = kcat;
    rxn.m_label = label;
    return rxn;
}

Reaction Reaction::makeSimple(
    uint8_t reactant,
    uint8_t product,
    float   kcat,
    const std::string& label)
{
    Reaction rxn;
    rxn.m_r0    = {reactant, 1.0f};
    rxn.m_r1    = {Chem::NULL_CHEM, 0.0f};
    rxn.m_p0    = {product, 1.0f};
    rxn.m_p1    = {Chem::NULL_CHEM, 0.0f};
    rxn.m_kcat  = kcat;
    rxn.m_label = label;
    return rxn;
}

void Reaction::setInhibitor(uint8_t chemID, float strength)
{
    m_inhibChemID    = chemID;
    m_inhibStrength  = std::max(0.0f, std::min(1.0f, strength));
}

// =============================================================================
//  tick — advance the reaction by dt seconds
//
//  Rate calculation (normalised to 0-255 scale):
//      v_raw = kcat * norm(R0) * norm(R1)   — bimolecular
//      v_raw = kcat * norm(R0)              — unimolecular
//      where norm(x) = x / 255.0f
//
//  Inhibition:
//      v = v_raw * (1 - inhibStrength * norm(INH))
//
//  Amount of primary reactant consumed per dt:
//      delta_r0 = v * dt * s_r0 * 255
//  But we can only consume what is available, so we clamp and scale:
//      scale = min(1, pool[R0] / delta_r0_needed)
// =============================================================================
void Reaction::tick(ChemicalPool& pool, float dt) const
{
    if (m_kcat <= 0.0f) return;

    // Normalise reactant concentrations to [0,1]
    float norm_r0 = pool.get(m_r0.chemID) / 255.0f;
    float norm_r1 = m_r1.active() ? (pool.get(m_r1.chemID) / 255.0f) : 1.0f;

    // Nothing to react with
    if (norm_r0 <= 0.0f) return;

    // Raw velocity (fraction of max per second)
    float v = m_kcat * norm_r0 * norm_r1;

    // Apply inhibitor
    if (m_inhibChemID != Chem::NULL_CHEM && m_inhibStrength > 0.0f)
    {
        float norm_inh = pool.get(m_inhibChemID) / 255.0f;
        float suppression = 1.0f - m_inhibStrength * norm_inh;
        v *= (suppression < 0.0f ? 0.0f : suppression);
    }

    // Amount to consume from each reactant (in concentration units 0-255)
    float consume_r0 = v * dt * m_r0.stoich * 255.0f;
    float consume_r1 = m_r1.active() ? (v * dt * m_r1.stoich * 255.0f) : 0.0f;

    // Availability check — if not enough reactant, scale down the reaction
    float scale = 1.0f;
    if (consume_r0 > 0.0f)
    {
        float avail0 = pool.get(m_r0.chemID);
        if (avail0 < consume_r0)
            scale = std::min(scale, avail0 / consume_r0);
    }
    if (m_r1.active() && consume_r1 > 0.0f)
    {
        float avail1 = pool.get(m_r1.chemID);
        if (avail1 < consume_r1)
            scale = std::min(scale, avail1 / consume_r1);
    }

    // Apply scaled consumption and production
    consume_r0 *= scale;
    consume_r1 *= scale;

    pool.consume(m_r0.chemID, consume_r0);
    if (m_r1.active()) pool.consume(m_r1.chemID, consume_r1);

    // Products produced proportional to what was actually consumed
    if (consume_r0 > 0.0f)
    {
        // Production ratio relative to primary reactant stoichiometry
        float ratio = consume_r0 / (m_r0.stoich * 255.0f);

        if (m_p0.active())
            pool.add(m_p0.chemID, ratio * m_p0.stoich * 255.0f);
        if (m_p1.active())
            pool.add(m_p1.chemID, ratio * m_p1.stoich * 255.0f);
    }
}
