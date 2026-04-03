#pragma once
#include "sim/Genome.hpp"
#include "sim/GenomePhenotype.hpp"
#include "sim/SimTypes.hpp"
#include "biochem/BiochemEngine.hpp"
#include "brain/Brain.hpp"
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct Creature {
    uint64_t  id        = 0;
    uint64_t  parentId  = 0;
    uint64_t  speciesId = 0;

    glm::vec2 pos       = {50.f, 50.f};
    glm::vec2 vel       = {0.f,  0.f};
    float     facingAngle = 0.f;

    Genome          genome;
    GenomePhenotype phenotype;
    BiochemEngine   biochem;
    Brain           brain;

    float   age                  = 0.f;
    float   reproductionCooldown = 0.f;
    bool    alive                = true;

    // Auto-generated display name e.g. "Queek_019"
    std::string name;

    std::array<SignalAssociation, SIGNAL_TYPE_COUNT> signalAssociations{};
    std::vector<RecentSignalObservation>             recentSignalObservations;
    std::vector<SocialMemory>                        socialMemory;
    std::array<float, SIGNAL_TYPE_COUNT>             perceivedSignals = {};
    VisualTraits                                     visualTraits;
    SignalType                                       dominantSignal = SignalType::HUNGER;
    float                                            dominantSignalIntensity = 0.f;
    MotorAction                                      lastAction = MotorAction::QUIESCENT;

    float   tickStartATP       = 0.f;
    float   tickStartComfort   = 0.f;
    float   tickStartPain      = 0.f;
    float   tickStartFear      = 0.f;
    bool    ateFoodThisTick    = false;
    bool    gotComfortThisTick = false;
    bool    tookDamageThisTick = false;

    // Initialize from a genome; calls loadDefaults + phenotype.apply
    void init(const Genome& g, uint64_t newId, uint64_t parentId = 0);

    // Non-copyable (BiochemEngine owns vectors), move-only
    Creature()                          = default;
    Creature(const Creature&)           = delete;
    Creature& operator=(const Creature&)= delete;
    Creature(Creature&&)                = default;
    Creature& operator=(Creature&&)     = default;
};
