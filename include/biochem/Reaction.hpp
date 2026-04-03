#pragma once
// =============================================================================
//  Reaction.hpp  —  A single biochemical transformation
//
//  Models the generic equation:
//      s0*R0  +  s1*R1  -->  p0*P0  +  p1*P1
//
//  Rate law (bimolecular Michaelis-Menten-inspired):
//      v = kcat * ([R0]/255) * ([R1]/255)           (if both reactants set)
//      v = kcat * ([R0]/255)                        (if only R0 set)
//
//  Per-tick delta:
//      consumed_R0 = v * dt * s0 * 255
//      produced_P0 = v * dt * p0 * 255
//
//  Inhibitors: a single inhibitor chemical can reduce kcat multiplicatively:
//      effective_kcat = kcat * (1 - inhibitor_strength * [INH]/255)
//
//  All IDs default to Chem::NULL_CHEM (0) meaning "not used".
// =============================================================================

#include "ChemID.hpp"
#include "ChemicalPool.hpp"
#include <cstdint>
#include <string>

class Reaction
{
public:
    // -----------------------------------------------------------------------
    // Per-reactant / per-product descriptor
    // -----------------------------------------------------------------------
    struct Slot
    {
        uint8_t chemID   = Chem::NULL_CHEM;  // which chemical
        float   stoich   = 1.0f;             // stoichiometric coefficient
        bool    active() const { return chemID != Chem::NULL_CHEM; }
    };

    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------

    // Two-reactant, two-product reaction with optional inhibitor.
    static Reaction make(
        uint8_t r0, float s_r0,
        uint8_t r1, float s_r1,
        uint8_t p0, float s_p0,
        uint8_t p1, float s_p1,
        float   kcat,
        const std::string& label = "");

    // One-reactant, two-product (catabolism).
    static Reaction makeUnary(
        uint8_t reactant, float s_r,
        uint8_t p0, float s_p0,
        uint8_t p1, float s_p1,
        float   kcat,
        const std::string& label = "");

    // One-reactant, one-product (simple conversion).
    static Reaction makeSimple(
        uint8_t reactant,
        uint8_t product,
        float   kcat,
        const std::string& label = "");

    // -----------------------------------------------------------------------
    // Inhibitor
    // -----------------------------------------------------------------------
    // Set a chemical that reduces this reaction's rate.
    // strength 0.0 = no effect, 1.0 = full block at [INH]=255.
    void setInhibitor(uint8_t chemID, float strength);

    // -----------------------------------------------------------------------
    // Simulation
    // -----------------------------------------------------------------------
    // Advance reaction by dt seconds; mutates pool concentrations.
    void tick(ChemicalPool& pool, float dt) const;

    // -----------------------------------------------------------------------
    // Accessors (for genome serialisation)
    // -----------------------------------------------------------------------
    const std::string& label() const { return m_label; }
    float kcat()               const { return m_kcat;  }
    void  setKcat(float k)           { m_kcat = k;     }

    Slot reactant(int i) const { return (i == 0) ? m_r0 : m_r1; }
    Slot product (int i) const { return (i == 0) ? m_p0 : m_p1; }

private:
    Slot        m_r0, m_r1;        // up to 2 reactants
    Slot        m_p0, m_p1;        // up to 2 products

    float       m_kcat = 0.0f;     // catalytic rate constant (units/s at full conc)

    uint8_t     m_inhibChemID = Chem::NULL_CHEM;
    float       m_inhibStrength = 0.0f;

    std::string m_label;           // human-readable name for debugging
};
