#pragma once
// =============================================================================
//  Brain.hpp  —  Compartmentalised neural network with SVRule dendrites
//
//  Architecture
//  ------------
//  9 lobes totalling 1,000 neurons and ~5,000 dendrites.
//
//  Lobe     ID  Size  Role
//  -------- --  ----  -------------------------------------------------------
//  Percept   0   40   World sensor inputs (food dist, threat, ally, etc.)
//  Drive     1   16   Biochemical drive levels (hunger, pain, sleepiness …)
//  StimSrc   2   40   Stimulus source classifier (CAOS Family field)
//  Verb      3   16   Action being done to creature (hit, picked up, fed …)
//  Noun      4   40   Object of attention (CAOS Genus field)
//  General   5   32   Miscellaneous normalised inputs
//  Attention 6   16   Focus modulation (boosts perception of attended object)
//  Concept   7  789   Main associative + learning area (most dendrites here)
//  Decision  8   11   Winner-Takes-All → MotorAction
//  ─────────────────────────────────────────────────────────────────────────
//  Total         1000
//
//  Tick order: Percept/Drive/StimSrc/Verb/Noun/General set by external world
//  → Attention → Concept → Decision.
//
//  SVRule learning
//  ---------------
//  REINFORCE_POS / REINFORCE_NEG dendrites in the Concept lobe update their
//  learnedGain after each tick based on REWARD / PUNISH chemicals in the pool.
//  This is the only form of within-lifetime learning.  Cross-generation
//  learning happens via evolution (Phase 3).
//
//  ATP cost
//  --------
//  Neural activity costs ATP proportional to total neuron output sum.
//  This creates a trade-off: a highly-active brain drains the creature faster.
// =============================================================================

#include "MotorAction.hpp"
#include "SVRule.hpp"
#include "biochem/ChemicalPool.hpp"

#include <array>
#include <vector>
#include <string>
#include <cstdint>

// ---------------------------------------------------------------------------
// Dendrite — one incoming synapse to a neuron
// ---------------------------------------------------------------------------
struct Dendrite
{
    uint8_t  srcLobe;         // source lobe ID
    uint16_t srcNeuron;       // neuron index within source lobe
    SVRule   rule;            // how input transforms into accumulator delta
};

// ---------------------------------------------------------------------------
// Perception input slots (indices into LOBE_PERCEPT)
// ---------------------------------------------------------------------------
namespace Percept {
    static constexpr int FOOD_NEARBY      = 0;   // nearest food, 1=adjacent 0=absent
    static constexpr int THREAT_NEARBY    = 1;   // nearest predator/hazard
    static constexpr int ALLY_NEARBY      = 2;   // nearest same-species creature
    static constexpr int OBJECT_NEARBY    = 3;   // nearest interactable object
    static constexpr int FOOD_DISTANCE    = 4;   // 1-(dist/maxDist) for nearest food
    static constexpr int THREAT_DISTANCE  = 5;
    static constexpr int EDGE_NEAR        = 6;   // near world boundary
    static constexpr int IN_SAFE_ZONE     = 7;
    static constexpr int IN_HAZARD_ZONE   = 8;
    static constexpr int LIGHT_LEVEL      = 9;   // 0=dark,1=bright (day/night)
    static constexpr int SIGNAL_HUNGER    = 10;
    static constexpr int SIGNAL_FEAR      = 11;
    static constexpr int SIGNAL_COMFORT   = 12;
    static constexpr int SIGNAL_FOOD      = 13;
    // slots 14-39: reserved for CAOS classifier inputs
}

// ---------------------------------------------------------------------------
// Drive input slots (indices into LOBE_DRIVE) — map to ChemicalPool entries
// ---------------------------------------------------------------------------
namespace Drive {
    static constexpr int HUNGER_CARB      = 0;
    static constexpr int HUNGER_PROTEIN   = 1;
    static constexpr int HUNGER_FAT       = 2;
    static constexpr int SLEEPINESS       = 3;
    static constexpr int PAIN             = 4;
    static constexpr int SEX_DRIVE        = 5;
    static constexpr int LONELINESS       = 6;
    static constexpr int BOREDOM         = 7;
    static constexpr int FEAR             = 8;
    static constexpr int ANGER            = 9;
    static constexpr int COMFORT          = 10;
    static constexpr int CROWDING         = 11;
    static constexpr int ADRENALINE       = 12;
    static constexpr int TIREDNESS        = 13;
    static constexpr int REWARD           = 14;
    static constexpr int PUNISH           = 15;
}

namespace GeneralInput {
    static constexpr int TRAINER_SIGNAL   = 0;
    static constexpr int FOOD_CONTEXT     = 1;
    static constexpr int TIME_IN_SIM      = 3;
    static constexpr int SOCIAL_POSITIVE  = 4;
    static constexpr int SOCIAL_NEGATIVE  = 5;
}

// ---------------------------------------------------------------------------
// BrainDecision — returned each tick, feeds Scientist Mode and game systems
// ---------------------------------------------------------------------------
struct BrainDecision
{
    MotorAction action        = MotorAction::QUIESCENT;
    float       confidence    = 0.0f;   // winning neuron output [0,1]
    float       atpCost       = 0.0f;   // ATP units spent this tick

    struct Competitor
    {
        MotorAction action     = MotorAction::QUIESCENT;
        float       activation = 0.0f;
    };
    std::array<Competitor, 3> top3;   // [0]=winner, [1]=2nd, [2]=3rd

    std::string dominantDriveLabel;   // e.g. "hunger_carb=0.82"
    std::string reasonLine;           // one-line explanation for Scientist Mode
};

// ---------------------------------------------------------------------------
// Brain
// ---------------------------------------------------------------------------
class Brain
{
public:
    // Lobe identifiers
    enum LobeID : int
    {
        LOBE_PERCEPT   = 0,
        LOBE_DRIVE     = 1,
        LOBE_STIM_SRC  = 2,
        LOBE_VERB      = 3,
        LOBE_NOUN      = 4,
        LOBE_GENERAL   = 5,
        LOBE_ATTENTION = 6,
        LOBE_CONCEPT   = 7,
        LOBE_DECISION  = 8,
        NUM_LOBES      = 9
    };

    static constexpr int TOTAL_NEURONS = 1000;

    // Lobe sizes (must sum to TOTAL_NEURONS)
    static constexpr int LOBE_SIZES[NUM_LOBES] = {
        40,   // Percept
        16,   // Drive
        40,   // StimSrc
        16,   // Verb
        40,   // Noun
        32,   // General
        16,   // Attention
        789,  // Concept
        11    // Decision
    };

    // ATP cost per unit of total neural output per second
    static constexpr float ATP_COST_PER_ACTIVATION = 0.002f;

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    Brain();

    // Build default genome wiring: Decision lobe directly wired to Drive +
    // Percept; Concept lobe sparsely wired for associative learning.
    void loadDefaultWiring();

    // -----------------------------------------------------------------------
    // External input setters (called by world/biochem before tick)
    // -----------------------------------------------------------------------

    // Copy drive chemical levels from the pool into the Drive lobe.
    void setDriveInputs(const ChemicalPool& pool);

    // Set a single Percept lobe neuron (use Percept:: constants for idx).
    void setPerceptInput(int idx, float value);

    // Set a Noun lobe input (attended object classifier genus).
    void setNounInput(int idx, float value);

    // Set a Verb lobe input (what is being done to creature).
    void setVerbInput(int idx, float value);

    // Set a General input.
    void setGeneralInput(int idx, float value);

    // Inject a stimulus source (CAOS Family classifier).
    void setStimSrcInput(int idx, float value);

    // -----------------------------------------------------------------------
    // Simulation tick
    // -----------------------------------------------------------------------
    // Process one brain tick; reads reward/punish from pool for learning;
    // returns the motor decision and ATP cost.
    BrainDecision tick(float dt, const ChemicalPool& pool);

    // -----------------------------------------------------------------------
    // Queries (Scientist Mode)
    // -----------------------------------------------------------------------
    float getNeuronOutput(int lobeID, int localIdx) const;
    int   getLobeStart(int lobeID)                  const { return m_lobeStart[lobeID]; }
    int   getLobeSize (int lobeID)                  const { return LOBE_SIZES[lobeID]; }
    const BrainDecision& lastDecision()             const { return m_lastDecision; }

    // Print a formatted Scientist Mode panel for one lobe.
    void dumpLobe(int lobeID, const char* label = nullptr) const;

    // Print Decision lobe competition for Scientist Mode.
    void dumpDecision() const;

private:
    // -----------------------------------------------------------------------
    // Neuron storage (structure-of-arrays for cache efficiency)
    // -----------------------------------------------------------------------
    std::array<float, TOTAL_NEURONS> m_output;     // post-sigmoid  [0,1]
    std::array<float, TOTAL_NEURONS> m_state;      // pre-sigmoid accumulator
    std::array<float, TOTAL_NEURONS> m_restState;  // genome-defined baseline
    std::array<float, TOTAL_NEURONS> m_leakage;    // state decay per tick

    // Dendrites: one vector per neuron (indexed by global neuron index)
    std::vector<std::vector<Dendrite>> m_dendrites;

    // Lobe start indices into flat arrays
    std::array<int, NUM_LOBES> m_lobeStart;

    BrainDecision m_lastDecision;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    int   globalIdx(int lobeID, int localIdx) const;
    float lobeNeuronOutput(int lobeID, int localIdx) const;

    static float sigmoid(float x);
    static float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }

    // Process one lobe: accumulate dendrites, apply activation, store output.
    void processLobe(int lobeID, float rewardSig, float punishSig, bool learn);

    // Winner-Takes-All on the Decision lobe.
    BrainDecision evaluateDecision(float dt) const;

    // Build dominant-drive label for reasoning output.
    std::string buildDriveLabel() const;

    // Wire Decision lobe neuron `decisionNeuron` to receive from
    // lobe `srcLobe`, neuron `srcNeuron`, with given SVRule.
    void wireDecision(int decisionNeuron, int srcLobe, int srcNeuron,
                      SVRule::Op op, float gain, float threshold = 0.5f);

    // Wire a Concept lobe neuron to a source.
    void wireConcept(int conceptNeuron, int srcLobe, int srcNeuron,
                     SVRule::Op op, float gain, float threshold = 0.5f);
};
