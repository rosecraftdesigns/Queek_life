#include "biochem/ChemicalPool.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

// =============================================================================
//  Default decay rates per second for named chemicals.
//  These are the fallback values; genomes will override them per-creature.
//  0.0  = does not decay (must be consumed explicitly)
//  0.01 = very slow (toxins, stored fat)
//  0.05 = slow      (drives: hunger, sleepiness)
//  0.15 = medium    (hormones: adrenaline, cortisol)
//  0.5  = fast      (reward/punish pulses, attention spikes)
//  1.0  = very fast (transient signals, ~gone in 1s)
// =============================================================================
static const float kDefaultDecayRates[256] = {
    /* 0  NULL        */ 0.0f,
    /* 1  GLUCOSE     */ 0.0f,   // consumed by reactions, not decay
    /* 2  OXYGEN      */ 0.0f,
    /* 3  AMINO_ACID  */ 0.0f,
    /* 4  FATTY_ACID  */ 0.0f,
    /* 5  WATER       */ 0.0f,
    /* 6  (unused)    */ 0.0f,
    /* 7  (unused)    */ 0.0f,
    /* 8  (unused)    */ 0.0f,
    /* 9  (unused)    */ 0.0f,
    /* 10 ATP         */ 0.0f,   // consumed by ATPase activity, not decay
    /* 11 ADP         */ 0.0f,
    /* 12 (unused)    */ 0.0f,
    /* 13 (unused)    */ 0.0f,
    /* 14 (unused)    */ 0.0f,
    /* 15 CO2         */ 0.08f,  // exhaled
    /* 16 LACTATE     */ 0.04f,  // cleared by liver
    /* 17 UREA        */ 0.02f,  // excreted slowly
    /* 18 (unused)    */ 0.0f,
    /* 19 (unused)    */ 0.0f,
    /* 20 HUNGER_CARB    */ 0.03f,
    /* 21 HUNGER_PROTEIN */ 0.03f,
    /* 22 HUNGER_FAT     */ 0.02f,
    /* 23 SLEEPINESS     */ 0.02f,
    /* 24 PAIN           */ 0.05f,
    /* 25 SEX_DRIVE      */ 0.01f,
    /* 26 LONELINESS     */ 0.02f,
    /* 27 CROWDING       */ 0.10f,
    /* 28 BOREDOM        */ 0.02f,
    /* 29 ANGER          */ 0.08f,
    /* 30 FEAR           */ 0.10f,
    /* 31 COMFORT        */ 0.03f,
    /* 32 (unused)       */ 0.0f,
    /* 33 (unused)       */ 0.0f,
    /* 34 (unused)       */ 0.0f,
    /* 35 (unused)       */ 0.0f,
    /* 36 ADRENALINE     */ 0.20f,
    /* 37 OESTROGEN      */ 0.05f,
    /* 38 TESTOSTERONE   */ 0.05f,
    /* 39 CORTISOL       */ 0.10f,
    /* 40 SEROTONIN      */ 0.04f,
    /* 41 DOPAMINE       */ 0.15f,
    /* 42 MELATONIN      */ 0.08f,
    /* 43 OXYTOCIN       */ 0.12f,
    /* 44 INSULIN        */ 0.12f,
    /* 45 GLUCAGON       */ 0.12f,
    /* 46 CYANIDE        */ 0.005f, // very persistent toxin
    /* 47 ALCOHOL        */ 0.04f,
    /* 48 HISTAMINE      */ 0.08f,
    /* 49 ANTITOXIN      */ 0.15f,
    /* 50 ARSENIC        */ 0.003f,
    /* 51 ANTIHISTAMINE  */ 0.10f,
    /* 52 (unused)       */ 0.0f,
    /* 53 (unused)       */ 0.0f,
    /* 54 (unused)       */ 0.0f,
    /* 55 (unused)       */ 0.0f,
    /* 56 REWARD         */ 0.80f,
    /* 57 PUNISH         */ 0.80f,
    /* 58 TIREDNESS      */ 0.01f,  // builds slowly, cleared by sleep
    /* 59 WAKEFULNESS    */ 0.05f,
    /* 60 ATTENTION      */ 0.20f,
    /* 61-255: genome-defined, default 0 */ 0.0f
    // (remaining slots are zero-initialised by value-initialisation)
};

// =============================================================================
ChemicalPool::ChemicalPool()
{
    reset();
}

void ChemicalPool::reset()
{
    m_levels.fill(0.0f);
    // Copy named defaults; genome-defined chemicals default to 0 decay.
    for (int i = 0; i < NUM_CHEMS; ++i)
    {
        m_decayRates[i] = (i < 61) ? kDefaultDecayRates[i] : 0.0f;
    }
}

void ChemicalPool::set(uint8_t id, float value)
{
    m_levels[id] = clamp(value, CONCENTRATION_MIN, CONCENTRATION_MAX);
}

float ChemicalPool::add(uint8_t id, float delta)
{
    float before = m_levels[id];
    float after  = clamp(before + delta, CONCENTRATION_MIN, CONCENTRATION_MAX);
    m_levels[id] = after;
    return after - before;   // actual amount added
}

float ChemicalPool::consume(uint8_t id, float amount)
{
    float available = m_levels[id];
    float consumed  = (amount < available) ? amount : available;
    m_levels[id]   -= consumed;
    return consumed;
}

void ChemicalPool::setDecayRate(uint8_t id, float ratePerSecond)
{
    m_decayRates[id] = (ratePerSecond < 0.0f) ? 0.0f : ratePerSecond;
}

void ChemicalPool::decay(float dt)
{
    for (int i = 0; i < NUM_CHEMS; ++i)
    {
        if (m_decayRates[i] > 0.0f && m_levels[i] > 0.0f)
        {
            // Exact solution to dC/dt = -k*C:  C(t+dt) = C(t)*exp(-k*dt)
            // Use fast approximation for small k*dt; switch to exp for large.
            float k  = m_decayRates[i];
            float kdt = k * dt;
            float factor = (kdt < 0.1f)
                           ? (1.0f - kdt + 0.5f*kdt*kdt)   // Taylor order-2
                           : std::exp(-kdt);
            m_levels[i] *= factor;
            if (m_levels[i] < 0.001f) m_levels[i] = 0.0f; // snap to zero
        }
    }
}

void ChemicalPool::dump(const char* label) const
{
    if (label) std::printf("=== ChemicalPool: %s ===\n", label);
    else        std::printf("=== ChemicalPool ===\n");

    for (int i = 0; i < NUM_CHEMS; ++i)
    {
        if (m_levels[i] > 0.001f)
        {
            const char* n = Chem::name(static_cast<uint8_t>(i));
            std::printf("  [%3d] %-18s = %7.3f\n", i, n, m_levels[i]);
        }
    }
    std::printf("\n");
}

void ChemicalPool::dumpOne(uint8_t id) const
{
    const char* n = Chem::name(id);
    std::printf("  [%3d] %-18s = %7.3f\n", id, n, m_levels[id]);
}
