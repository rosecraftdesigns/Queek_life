#pragma once
// =============================================================================
//  BiochemEngine.hpp  —  Orchestrates the complete biochemical simulation
//
//  Tick order per update:
//    1. External injections applied (eaten food, inhaled air, toxin events)
//    2. Emitters evaluated and fire into pool
//    3. Reactions evaluated and mutate pool
//    4. Receptors evaluated; fire brain signals or secondary chemicals
//    5. First-order decay applied to all chemicals
//    6. Death condition checked (ATP == 0)
//
//  The engine owns all reactions, emitters, and receptors for one creature.
//  Each creature instance owns one BiochemEngine.
// =============================================================================

#include "ChemicalPool.hpp"
#include "Reaction.hpp"
#include "Emitter.hpp"
#include "Receptor.hpp"

#include <vector>
#include <cstdint>
#include <functional>
#include <string>

// Pending chemical injection (applied at start of next tick).
struct ChemInjection
{
    uint8_t chemID;
    float   amount;
};

// Observation snapshot for Scientist Mode / logging.
struct BiochemSnapshot
{
    float atp;
    float adp;
    float glucose;
    float oxygen;
    float co2;
    float hungerCarb;
    float hungerProtein;
    float hungerFat;
    float sleepiness;
    float pain;
    float adrenaline;
    bool  alive;
    float ageSeconds;
};

class BiochemEngine
{
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    BiochemEngine();

    // Load a default healthy creature biochemistry (used for testing/prototyping).
    // In Phase 3 this will be replaced by genome-driven initialisation.
    void loadDefaults();

    // -----------------------------------------------------------------------
    // Simulation tick
    // -----------------------------------------------------------------------
    // Advance all biochemistry by dt seconds.
    void tick(float dt);

    // -----------------------------------------------------------------------
    // External events (called by game engine / CAOS scripts)
    // -----------------------------------------------------------------------
    // Queue a chemical injection (delivered at start of next tick).
    void inject(uint8_t chemID, float amount);

    // Convenience: creature ate food with known nutritional profile.
    void eatFood(float glucose, float aminoAcid, float fattyAcid, float water = 5.0f);

    // Creature is moving this tick — consume ATP proportional to effort.
    // effort: 0=still, 1=walking, 2=running
    void spendATP(float effort, float dt);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------
    bool  isAlive()  const { return m_alive; }
    float ageSeconds() const { return m_ageSeconds; }

    // Direct access to the pool for receptors, brain, etc.
    ChemicalPool&       pool()       { return m_pool; }
    const ChemicalPool& pool() const { return m_pool; }

    // Fill a snapshot struct for display.
    BiochemSnapshot snapshot() const;

    // -----------------------------------------------------------------------
    // Component registration
    //   (genome loading in Phase 3 will populate these lists)
    // -----------------------------------------------------------------------
    void addReaction(const Reaction& r);
    void addEmitter(const Emitter& e);
    void addReceptor(const Receptor& r);

    // Remove all reactions, emitters, and receptors.
    // Used in unit tests to isolate specific biochemical pathways.
    void clearComponents();

    // Label-based lookup for GenomePhenotype::apply() — returns nullptr if not found.
    Emitter*  findEmitter(const std::string& label);
    Reaction* findReaction(const std::string& label);

    // -----------------------------------------------------------------------
    // Debug
    // -----------------------------------------------------------------------
    void dumpSnapshot(const char* label = nullptr) const;

    // Callback invoked on death; receives cause string.
    std::function<void(const std::string&)> onDeath;

private:
    void applyInjections();
    void checkDeath();

    ChemicalPool              m_pool;
    std::vector<Reaction>     m_reactions;
    std::vector<Emitter>      m_emitters;
    std::vector<Receptor>     m_receptors;
    std::vector<ChemInjection> m_pendingInjections;

    bool  m_alive       = true;
    float m_ageSeconds  = 0.0f;

    // Minimum ATP before death is triggered.
    static constexpr float ATP_DEATH_THRESHOLD = 0.5f;
};
