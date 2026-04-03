#pragma once
#include "sim/Creature.hpp"
#include "sim/RewardSystem.hpp"
#include "sim/Species.hpp"
#include "world/WorldGrid.hpp"
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

// Player role — gates which tools are accessible
enum class PlayerRole { SCIENTIST, CARETAKER, GOD, EVIL };

// Scenario mode — configuration presets
enum class ScenarioMode { SANDBOX, SURVIVAL, PREDATOR_PREY, PARADISE, PANDEMIC };

class SimSystem {
public:
    SimSystem();

    void init(uint32_t seed, int initialCount = 5);
    void tick(float dt);

    // Accessors for renderer
    const std::vector<std::unique_ptr<Creature>>& creatures() const { return m_creatures; }
    WorldGrid&        grid()           { return m_grid; }
    const WorldGrid&  grid()    const  { return m_grid; }
    SpeciesTracker&   speciesTracker() { return m_species; }
    const std::vector<SignalPulse>& activeSignals() const { return m_activeSignals; }
    float             simTime()  const { return m_simTime; }
    uint32_t          seed()     const { return m_seed; }

    // Player agency
    uint64_t spawnCreature(glm::vec2 pos, const Genome& g = Genome::makeDefault(),
                           uint64_t parentId = 0);
    void killCreature(uint64_t id);
    void feedCreature(uint64_t id, float amount);
    void queuePlayerSignal(glm::vec2 pos, SignalType type, float intensity = 1.f);

    // Player role
    PlayerRole  playerRole()                 const { return m_role; }
    void        setPlayerRole(PlayerRole r)        { m_role = r; }

    // Scenario mode
    ScenarioMode  scenarioMode()              const { return m_scenario; }
    void          setScenarioMode(ScenarioMode s);

    // Event log callback: (simTime, creatureName, eventText)
    std::function<void(float, const std::string&, const std::string&)> onEvent;

    // Find creature by id
    Creature* findCreature(uint64_t id);

private:
    struct SignalPerception {
        std::array<float, SIGNAL_TYPE_COUNT> strengths = {};
        std::array<uint64_t, SIGNAL_TYPE_COUNT> emitters = {};
    };

    struct SocialContext {
        float allyNearby     = 0.f;
        float threatNearby   = 0.f;
        float socialPositive = 0.f;
        float socialNegative = 0.f;
    };

    struct BirthRequest {
        uint64_t parentAId = 0;
        uint64_t parentBId = 0;
        glm::vec2 pos{50.f, 50.f};
        Genome genome;
    };

    void tickBiochemistry(Creature& c, float dt);
    void tickCreature(Creature& c, const std::vector<SignalPulse>& tickSignals, float dt);
    void applyMotorAction(Creature& c, MotorAction action, float dt);
    float handleEat(Creature& c);
    void emitCreatureSignals(Creature& c, std::vector<SignalPulse>& tickSignals);
    SignalPerception perceiveSignals(const Creature& c, const std::vector<SignalPulse>& tickSignals) const;
    SocialContext computeSocialContext(const Creature& c) const;
    void rememberSignalObservations(Creature& c, const SignalPerception& perception);
    void applySignalChemistry(Creature& c, const SignalPerception& perception, float dt);
    void resolveSocialInteractions(float dt);
    void resolveReproduction();
    void updateOutcomeLearning(Creature& c);
    void decaySocialState(Creature& c, float dt);
    void refreshRenderSignals(const std::vector<SignalPulse>& tickSignals, float dt);
    float learnedSignalValence(const Creature& c, SignalType type) const;
    bool isReadyToReproduce(const Creature& c) const;
    float reproductionScore(const Creature& seeker, const Creature& candidate) const;
    float localSurvivalPressure(const Creature& c) const;
    Genome buildChildGenome(const Creature& parentA, const Creature& parentB);
    SocialMemory* findOrCreateSocialMemory(Creature& c, uint64_t otherId);
    const SocialMemory* findSocialMemory(const Creature& c, uint64_t otherId) const;
    uint32_t nextRandom();
    float randomUnit();
    void cullDead();

    WorldGrid                              m_grid;
    std::vector<std::unique_ptr<Creature>> m_creatures;
    std::vector<SignalPulse>               m_activeSignals;
    std::vector<SignalPulse>               m_playerSignalQueue;
    SpeciesTracker                         m_species;
    RewardSystem                           m_rewardSystem;
    uint64_t                               m_nextId   = 1;
    uint32_t                               m_seed     = 0;
    uint32_t                               m_rngState = 0;
    float                                  m_simTime  = 0.f;
    PlayerRole                             m_role     = PlayerRole::SCIENTIST;
    ScenarioMode                           m_scenario = ScenarioMode::SANDBOX;
};
