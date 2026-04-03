#include "sim/SimSystem.hpp"
#include "biochem/ChemID.hpp"
#include "brain/MotorAction.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kReproductionSearchRadius = 3.25f;
constexpr float kMinimumReproductionAge = 20.f;
constexpr float kMinimumReproductionATP = 125.f;
constexpr float kMinimumReproductionComfort = 110.f;
constexpr float kMaximumReproductionFear = 90.f;
constexpr float kMaximumReproductionPain = 60.f;
constexpr float kMaximumReproductionPressure = 0.55f;
constexpr float kMinimumReproductionFood = 0.08f;
constexpr float kFounderComfortThreshold = 96.f;
constexpr float kFounderFearThreshold = 72.f;
constexpr float kFounderHungerThreshold = 88.f;

float clamp01(float value) {
    return value < 0.f ? 0.f : (value > 1.f ? 1.f : value);
}

float wrapDegrees(float degrees) {
    while (degrees < 0.f) degrees += 360.f;
    while (degrees >= 360.f) degrees -= 360.f;
    return degrees;
}

float distanceFalloff(glm::vec2 a, glm::vec2 b, float radius) {
    if (radius <= 0.f) return 0.f;
    float dist = glm::length(a - b);
    return std::max(0.f, 1.f - dist / radius);
}

float signalBaseRadius(SignalType type) {
    switch (type) {
    case SignalType::HUNGER:  return 3.5f;
    case SignalType::FEAR:    return 4.5f;
    case SignalType::COMFORT: return 4.0f;
    case SignalType::FOOD:    return 4.0f;
    default:                  return 3.5f;
    }
}

}  // namespace

SimSystem::SimSystem() = default;

uint32_t SimSystem::nextRandom() {
    m_rngState = m_rngState * 1664525u + 1013904223u;
    return m_rngState;
}

float SimSystem::randomUnit() {
    return (float)(nextRandom() >> 8) / (float)(1 << 24);
}

void SimSystem::init(uint32_t seed, int initialCount) {
    m_seed = seed;
    m_rngState = seed == 0 ? 0x12345678u : seed;
    m_simTime = 0.f;
    m_nextId = 1;
    m_grid.seedDefault(seed);
    m_creatures.clear();
    m_activeSignals.clear();
    m_playerSignalQueue.clear();
    m_species = SpeciesTracker{};

    const glm::vec2 spawnCenters[] = {{25.f, 25.f}, {75.f, 25.f}, {50.f, 75.f}};
    const float spawnAngles[] = {0.55f, 2.65f, 4.55f};
    for (int i = 0; i < initialCount; ++i) {
        int cluster = i % 3;
        int memberInCluster = i / 3;
        glm::vec2 center = spawnCenters[cluster];
        float angle = spawnAngles[cluster] +
            (memberInCluster - 1) * 0.22f +
            (randomUnit() - 0.5f) * 0.28f;
        float radius = 11.5f + memberInCluster * 1.2f + randomUnit() * 1.8f;
        glm::vec2 pos = center + glm::vec2{
            std::cos(angle) * radius,
            std::sin(angle) * radius
        };
        pos.x = std::max(1.f, std::min((float)WorldGrid::WIDTH - 1.f, pos.x));
        pos.y = std::max(1.f, std::min((float)WorldGrid::HEIGHT - 1.f, pos.y));

        Genome g = Genome::makeDefault();
        for (float& gene : g.genes) gene = randomUnit() * 0.4f + 0.3f;
        g.lineageId = m_nextId;
        g.lineageHueDegrees = randomUnit() * 360.f;
        uint64_t newId = spawnCreature(pos, g);

        if (Creature* c = findCreature(newId)) {
            c->biochem.pool().set(Chem::ATP, 96.f + randomUnit() * 28.f);

            if ((memberInCluster % 2) == 0) {
                c->biochem.pool().set(Chem::HUNGER_CARB,
                    kFounderHungerThreshold - 12.f + randomUnit() * 18.f);
                c->biochem.pool().set(Chem::BOREDOM, 112.f + randomUnit() * 42.f);
                c->biochem.pool().set(Chem::LONELINESS, 110.f + randomUnit() * 36.f);
                c->biochem.pool().set(Chem::COMFORT,
                    kFounderComfortThreshold - 10.f + randomUnit() * 22.f);
            } else {
                c->biochem.pool().set(Chem::HUNGER_CARB,
                    kFounderHungerThreshold - 2.f + randomUnit() * 28.f);
                c->biochem.pool().set(Chem::BOREDOM, 82.f + randomUnit() * 34.f);
                c->biochem.pool().set(Chem::LONELINESS, 72.f + randomUnit() * 28.f);
                c->biochem.pool().set(Chem::COMFORT,
                    kFounderComfortThreshold + 4.f + randomUnit() * 30.f);
            }

            if ((cluster == 2 && memberInCluster == 0) || (i % 5) == 0) {
                c->biochem.pool().set(Chem::FEAR,
                    kFounderFearThreshold - 4.f + randomUnit() * 24.f);
            } else {
                c->biochem.pool().set(Chem::FEAR, 10.f + randomUnit() * 18.f);
            }
        }
    }
}

uint64_t SimSystem::spawnCreature(glm::vec2 pos, const Genome& g, uint64_t parentId) {
    auto c = std::make_unique<Creature>();
    Genome genome = g;

    if (genome.lineageId == 0) {
        genome.lineageId = m_nextId;
        if (genome.lineageHueDegrees == 0.f)
            genome.lineageHueDegrees = randomUnit() * 360.f;
    }

    uint64_t newId = m_nextId++;
    c->init(genome, newId, parentId);
    c->pos = pos;
    c->facingAngle = randomUnit() * 6.2831853f;

    c->speciesId = m_species.classify(genome, c->id);

    const auto& sps = m_species.allSpecies();
    std::string prefix = "Queek";
    for (const auto& sp : sps) {
        if (sp.id == c->speciesId) {
            prefix = sp.name.substr(0, 5);
            break;
        }
    }

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s_%03llu",
        prefix.c_str(), (unsigned long long)(c->id % 1000));
    c->name = buf;

    if (onEvent) onEvent(m_simTime, c->name, parentId != 0 ? "born" : "spawned");
    m_creatures.push_back(std::move(c));
    return newId;
}

void SimSystem::queuePlayerSignal(glm::vec2 pos, SignalType type, float intensity) {
    SignalPulse pulse;
    pulse.emitterId = 0;
    pulse.type = type;
    pulse.pos = pos;
    pulse.radius = signalBaseRadius(type);
    pulse.intensity = clamp01(intensity);
    pulse.ttl = 0.35f;
    pulse.playerAuthored = true;
    m_playerSignalQueue.push_back(pulse);
}

void SimSystem::tick(float dt) {
    m_grid.tick(dt);

    for (auto& c : m_creatures) {
        if (!c->alive) continue;
        c->dominantSignalIntensity = 0.f;
        c->perceivedSignals.fill(0.f);
        c->ateFoodThisTick = false;
        c->gotComfortThisTick = false;
        c->tookDamageThisTick = false;
        tickBiochemistry(*c, dt);
    }

    std::vector<SignalPulse> tickSignals;
    tickSignals.reserve(m_creatures.size() * 4 + m_playerSignalQueue.size());
    for (const SignalPulse& playerPulse : m_playerSignalQueue)
        tickSignals.push_back(playerPulse);
    m_playerSignalQueue.clear();

    for (auto& c : m_creatures) {
        if (c->alive) emitCreatureSignals(*c, tickSignals);
    }

    for (auto& c : m_creatures) {
        if (c->alive) tickCreature(*c, tickSignals, dt);
    }

    resolveSocialInteractions(dt);
    resolveReproduction();

    for (auto& c : m_creatures) {
        if (!c->alive) continue;
        decaySocialState(*c, dt);
        updateOutcomeLearning(*c);
    }

    refreshRenderSignals(tickSignals, dt);
    cullDead();
    m_simTime += dt;
}

void SimSystem::tickBiochemistry(Creature& c, float dt) {
    c.age += dt;
    if (c.reproductionCooldown > 0.f) c.reproductionCooldown -= dt;

    c.tickStartATP = c.biochem.pool().get(Chem::ATP);
    c.tickStartComfort = c.biochem.pool().get(Chem::COMFORT);
    c.tickStartPain = c.biochem.pool().get(Chem::PAIN);
    c.tickStartFear = c.biochem.pool().get(Chem::FEAR);

    float toxinNearby = m_grid.sampleToxinNearby(c.pos, c.phenotype.perceptionRadius * 1.2f);
    if (toxinNearby > 0.05f)
        c.biochem.inject(Chem::CYANIDE, toxinNearby * 8.f * dt);

    auto before = m_rewardSystem.captureSnapshot(c.biochem.pool());
    float effort = (c.vel.x != 0.f || c.vel.y != 0.f) ? 1.f : 0.f;
    c.biochem.spendATP(effort, dt);
    c.biochem.tick(dt);

    if (!c.biochem.isAlive()) {
        c.alive = false;
        return;
    }

    auto after = m_rewardSystem.captureSnapshot(c.biochem.pool());
    m_rewardSystem.apply(c.biochem, before, after);
}

void SimSystem::emitCreatureSignals(Creature& c, std::vector<SignalPulse>& tickSignals) {
    c.dominantSignalIntensity = 0.f;

    auto emit = [&](SignalType type, float intensity, float radius) {
        intensity = clamp01(intensity);
        if (intensity <= 0.f) return;

        SignalPulse pulse;
        pulse.emitterId = c.id;
        pulse.type = type;
        pulse.pos = c.pos;
        pulse.radius = radius;
        pulse.intensity = intensity;
        pulse.ttl = 0.28f;
        pulse.playerAuthored = false;
        tickSignals.push_back(pulse);

        if (intensity > c.dominantSignalIntensity) {
            c.dominantSignalIntensity = intensity;
            c.dominantSignal = type;
        }
    };

    const ChemicalPool& pool = c.biochem.pool();
    float hunger = pool.get(Chem::HUNGER_CARB);
    if (hunger >= kFounderHungerThreshold) {
        emit(SignalType::HUNGER,
             (hunger - kFounderHungerThreshold) / (255.f - kFounderHungerThreshold),
             3.0f + 1.8f * c.phenotype.socialAffinity);
    }

    float fear = pool.get(Chem::FEAR);
    if (fear >= kFounderFearThreshold) {
        emit(SignalType::FEAR,
             (fear - kFounderFearThreshold) / (255.f - kFounderFearThreshold),
             4.0f + 0.35f * c.phenotype.perceptionRadius);
    }

    float comfort = pool.get(Chem::COMFORT);
    if (comfort >= kFounderComfortThreshold) {
        emit(SignalType::COMFORT,
             (comfort - kFounderComfortThreshold) / (255.f - kFounderComfortThreshold),
             3.0f + 2.2f * c.phenotype.socialAffinity);
    }

    float foodNearby = m_grid.sampleFoodNearby(c.pos, 1.5f);
    if (foodNearby >= 0.28f) {
        emit(SignalType::FOOD,
             clamp01(foodNearby),
             3.0f + 0.25f * c.phenotype.perceptionRadius);
    }
}

SimSystem::SignalPerception SimSystem::perceiveSignals(
    const Creature& c,
    const std::vector<SignalPulse>& tickSignals) const {
    SignalPerception perception;

    for (const SignalPulse& pulse : tickSignals) {
        if (!pulse.playerAuthored && pulse.emitterId == c.id)
            continue;

        float strength = pulse.intensity * distanceFalloff(c.pos, pulse.pos, pulse.radius);
        int idx = signalTypeIndex(pulse.type);
        if (strength > perception.strengths[idx]) {
            perception.strengths[idx] = strength;
            perception.emitters[idx] = pulse.emitterId;
        }
    }

    return perception;
}

SimSystem::SocialContext SimSystem::computeSocialContext(const Creature& c) const {
    SocialContext context;
    float maxRadius = c.phenotype.perceptionRadius * 1.1f;

    for (const auto& otherPtr : m_creatures) {
        const Creature& other = *otherPtr;
        if (!other.alive || other.id == c.id) continue;

        float dist = glm::length(other.pos - c.pos);
        if (dist > maxRadius) continue;

        float proximity = std::max(0.f, 1.f - dist / std::max(1.f, maxRadius));
        float valence = 0.f;
        if (const SocialMemory* memory = findSocialMemory(c, other.id))
            valence = memory->valence;

        context.socialPositive = std::max(
            context.socialPositive,
            proximity * std::max(0.f, valence));
        context.socialNegative = std::max(
            context.socialNegative,
            proximity * std::max(0.f, -valence));
        context.threatNearby = std::max(context.threatNearby, proximity * std::max(0.f, -valence));

        if (other.speciesId == c.speciesId) {
            float allyWeight = proximity * (0.35f + 0.65f * std::max(0.f, valence));
            context.allyNearby = std::max(context.allyNearby, allyWeight);
        }
    }

    return context;
}

void SimSystem::rememberSignalObservations(Creature& c, const SignalPerception& perception) {
    for (int idx = 0; idx < SIGNAL_TYPE_COUNT; ++idx) {
        float strength = perception.strengths[idx];
        uint64_t emitterId = perception.emitters[idx];
        if (strength < 0.20f) continue;
        if (emitterId == c.id && emitterId != 0) continue;

        SignalType type = static_cast<SignalType>(idx);
        auto it = std::find_if(
            c.recentSignalObservations.begin(),
            c.recentSignalObservations.end(),
            [&](const RecentSignalObservation& obs) {
                return obs.type == type && obs.emitterId == emitterId;
            });

        if (it != c.recentSignalObservations.end()) {
            it->strength = std::max(it->strength, strength);
            it->simTime = m_simTime;
        } else {
            c.recentSignalObservations.push_back({type, emitterId, strength, m_simTime});
        }
    }

    std::sort(
        c.recentSignalObservations.begin(),
        c.recentSignalObservations.end(),
        [](const RecentSignalObservation& a, const RecentSignalObservation& b) {
            if (a.simTime != b.simTime) return a.simTime > b.simTime;
            if (a.strength != b.strength) return a.strength > b.strength;
            if (a.emitterId != b.emitterId) return a.emitterId < b.emitterId;
            return signalTypeIndex(a.type) < signalTypeIndex(b.type);
        });

    if ((int)c.recentSignalObservations.size() > MAX_RECENT_SIGNAL_OBSERVATIONS)
        c.recentSignalObservations.resize(MAX_RECENT_SIGNAL_OBSERVATIONS);
}

float SimSystem::learnedSignalValence(const Creature& c, SignalType type) const {
    const SignalAssociation& assoc = c.signalAssociations[signalTypeIndex(type)];
    float value = assoc.positive - assoc.negative;
    if (value < -1.f) value = -1.f;
    if (value > 1.f) value = 1.f;
    return value;
}

float SimSystem::localSurvivalPressure(const Creature& c) const {
    float toxinNearby = m_grid.sampleToxinNearby(c.pos, c.phenotype.perceptionRadius * 0.8f);
    float foodNearby = m_grid.sampleFoodNearby(c.pos, 1.5f);
    uint8_t zone = m_grid.zoneAt(c.pos);

    float pressure = 0.f;
    if (zone == 2) pressure += 0.50f;
    pressure += 0.35f * clamp01(toxinNearby / 0.30f);
    pressure += 0.15f * clamp01((kMinimumReproductionFood - foodNearby) / kMinimumReproductionFood);
    pressure += 0.10f * clamp01((c.biochem.pool().get(Chem::FEAR) - 64.f) / 128.f);
    return clamp01(pressure);
}

bool SimSystem::isReadyToReproduce(const Creature& c) const {
    if (!c.alive) return false;
    if (c.reproductionCooldown > 0.f) return false;
    if (c.age < std::max(kMinimumReproductionAge, c.phenotype.reproductionRate * 0.4f)) return false;

    const ChemicalPool& pool = c.biochem.pool();
    if (pool.get(Chem::ATP) < kMinimumReproductionATP) return false;
    if (pool.get(Chem::COMFORT) < kMinimumReproductionComfort) return false;
    if (pool.get(Chem::FEAR) > kMaximumReproductionFear) return false;
    if (pool.get(Chem::PAIN) > kMaximumReproductionPain) return false;
    if (m_grid.zoneAt(c.pos) == 2) return false;
    if (localSurvivalPressure(c) > kMaximumReproductionPressure) return false;

    float foodNearby = m_grid.sampleFoodNearby(c.pos, 1.5f);
    return foodNearby >= kMinimumReproductionFood || m_grid.zoneAt(c.pos) == 1;
}

float SimSystem::reproductionScore(const Creature& seeker, const Creature& candidate) const {
    if (!isReadyToReproduce(candidate)) return -1.f;
    if (!candidate.alive || candidate.id == seeker.id) return -1.f;
    if (candidate.speciesId != seeker.speciesId) return -1.f;

    float dist = glm::length(candidate.pos - seeker.pos);
    if (dist > kReproductionSearchRadius) return -1.f;

    float proximity = std::max(0.f, 1.f - dist / kReproductionSearchRadius);
    float atpScore = clamp01((candidate.biochem.pool().get(Chem::ATP) - kMinimumReproductionATP) / 100.f);
    float comfortScore = clamp01((candidate.biochem.pool().get(Chem::COMFORT) - kMinimumReproductionComfort) / 145.f);
    float safetyScore = 1.f - localSurvivalPressure(candidate);
    float socialScore = 0.f;
    if (const SocialMemory* memory = findSocialMemory(seeker, candidate.id))
        socialScore = memory->valence * 0.35f;

    return proximity * 3.0f + atpScore + comfortScore + safetyScore + socialScore;
}

Genome SimSystem::buildChildGenome(const Creature& parentA, const Creature& parentB) {
    Genome child = Genome::crossover(parentA.genome, parentB.genome, m_rngState);
    uint32_t childGeneration = std::max(parentA.genome.generation, parentB.genome.generation) + 1;

    float mutationRate =
        0.05f + 0.05f * (1.f - 0.5f * (parentA.phenotype.learningRate + parentB.phenotype.learningRate - 1.f));
    if (mutationRate < 0.03f) mutationRate = 0.03f;
    if (mutationRate > 0.12f) mutationRate = 0.12f;

    child = child.mutate(mutationRate, m_rngState);
    child.generation = childGeneration;
    for (auto& entry : child.mutationLog)
        entry.generation = childGeneration;

    if (child.mutationLog.empty()) {
        int geneIndex = (int)(nextRandom() % Genome::GENE_COUNT);
        float oldValue = child.genes[geneIndex];
        float delta = (randomUnit() - 0.5f) * 0.30f;
        if (std::fabs(delta) < 0.02f)
            delta = delta < 0.f ? -0.06f : 0.06f;
        float newValue = oldValue + delta;
        if (newValue < 0.f) newValue = 0.f;
        if (newValue > 1.f) newValue = 1.f;
        if (newValue == oldValue)
            newValue = oldValue > 0.5f ? oldValue - 0.05f : oldValue + 0.05f;
        if (newValue < 0.f) newValue = 0.f;
        if (newValue > 1.f) newValue = 1.f;
        child.genes[geneIndex] = newValue;
        child.mutationLog.push_back({(uint8_t)geneIndex, oldValue, newValue, childGeneration});
    }

    child.lineageId = parentA.genome.lineageId;
    child.lineageHueDegrees = wrapDegrees(child.lineageHueDegrees + (randomUnit() * 12.f - 6.f));
    return child;
}

void SimSystem::applySignalChemistry(Creature& c,
                                     const SignalPerception& perception,
                                     float dt) {
    c.perceivedSignals = perception.strengths;

    float fearStrength = std::min(0.7f, perception.strengths[signalTypeIndex(SignalType::FEAR)]);
    if (fearStrength > 0.f) {
        float scale = 1.f + 0.5f * std::max(0.f, -learnedSignalValence(c, SignalType::FEAR));
        c.biochem.inject(Chem::FEAR, 18.f * fearStrength * dt * scale);
        c.biochem.inject(Chem::ADRENALINE, 10.f * fearStrength * dt * scale);
    }

    float comfortStrength = perception.strengths[signalTypeIndex(SignalType::COMFORT)];
    if (comfortStrength > 0.f) {
        float scale = 1.f + 0.5f * std::max(0.f, learnedSignalValence(c, SignalType::COMFORT));
        c.biochem.inject(Chem::COMFORT, 14.f * comfortStrength * dt * scale);
        c.biochem.pool().consume(Chem::LONELINESS, 12.f * comfortStrength * dt * scale);
        c.biochem.inject(Chem::OXYTOCIN, 4.f * comfortStrength * dt * scale);
        if (comfortStrength >= 0.25f) c.gotComfortThisTick = true;
    }

    float foodStrength = perception.strengths[signalTypeIndex(SignalType::FOOD)];
    if (foodStrength > 0.f) {
        float valence = learnedSignalValence(c, SignalType::FOOD);
        float rewardScale = 1.f + 0.5f * std::max(0.f, valence) - 0.5f * std::max(0.f, -valence);
        rewardScale = std::max(0.5f, std::min(1.5f, rewardScale));
        c.biochem.inject(Chem::REWARD, 8.f * foodStrength * dt * rewardScale);
    }

    float hungerStrength = perception.strengths[signalTypeIndex(SignalType::HUNGER)];
    if (hungerStrength > 0.f && c.biochem.pool().get(Chem::HUNGER_CARB) >= 64.f) {
        c.biochem.inject(Chem::HUNGER_CARB, 6.f * hungerStrength * dt);
    }
}

void SimSystem::tickCreature(Creature& c,
                             const std::vector<SignalPulse>& tickSignals,
                             float dt) {
    float foodNearby = m_grid.sampleFoodNearby(c.pos, c.phenotype.perceptionRadius);
    float toxinNearby = m_grid.sampleToxinNearby(c.pos, c.phenotype.perceptionRadius * 1.2f);
    float foodHere = m_grid.sampleFoodNearby(c.pos, 1.5f);
    uint8_t zone = m_grid.zoneAt(c.pos);

    float edgeX = std::min(c.pos.x, (float)WorldGrid::WIDTH - c.pos.x) / (WorldGrid::WIDTH * 0.1f);
    float edgeY = std::min(c.pos.y, (float)WorldGrid::HEIGHT - c.pos.y) / (WorldGrid::HEIGHT * 0.1f);
    float edgeProx = 1.f - std::min(1.f, std::min(edgeX, edgeY));

    SocialContext socialContext = computeSocialContext(c);
    SignalPerception perception = perceiveSignals(c, tickSignals);
    rememberSignalObservations(c, perception);
    applySignalChemistry(c, perception, dt);

    c.brain.setDriveInputs(c.biochem.pool());
    c.brain.setPerceptInput(Percept::FOOD_NEARBY, foodNearby);
    c.brain.setPerceptInput(Percept::THREAT_NEARBY, std::max(toxinNearby, socialContext.threatNearby));
    c.brain.setPerceptInput(Percept::ALLY_NEARBY, socialContext.allyNearby);
    c.brain.setPerceptInput(Percept::OBJECT_NEARBY, 0.f);
    c.brain.setPerceptInput(Percept::FOOD_DISTANCE, 1.f - foodNearby);
    c.brain.setPerceptInput(Percept::THREAT_DISTANCE, std::max(toxinNearby, socialContext.threatNearby));
    c.brain.setPerceptInput(Percept::EDGE_NEAR, edgeProx);
    c.brain.setPerceptInput(Percept::IN_SAFE_ZONE, zone == 1 ? 1.f : 0.f);
    c.brain.setPerceptInput(Percept::IN_HAZARD_ZONE, zone == 2 ? 1.f : 0.f);
    c.brain.setPerceptInput(Percept::LIGHT_LEVEL, 1.f);
    c.brain.setPerceptInput(Percept::SIGNAL_HUNGER, perception.strengths[signalTypeIndex(SignalType::HUNGER)]);
    c.brain.setPerceptInput(Percept::SIGNAL_FEAR, perception.strengths[signalTypeIndex(SignalType::FEAR)]);
    c.brain.setPerceptInput(Percept::SIGNAL_COMFORT, perception.strengths[signalTypeIndex(SignalType::COMFORT)]);
    c.brain.setPerceptInput(Percept::SIGNAL_FOOD, perception.strengths[signalTypeIndex(SignalType::FOOD)]);

    c.brain.setGeneralInput(GeneralInput::TRAINER_SIGNAL, 0.f);
    c.brain.setGeneralInput(GeneralInput::FOOD_CONTEXT,
        std::max(foodHere, perception.strengths[signalTypeIndex(SignalType::FOOD)]));
    c.brain.setGeneralInput(GeneralInput::TIME_IN_SIM, std::min(1.f, c.age / 60.f));
    c.brain.setGeneralInput(GeneralInput::SOCIAL_POSITIVE, socialContext.socialPositive);
    c.brain.setGeneralInput(GeneralInput::SOCIAL_NEGATIVE, socialContext.socialNegative);

    BrainDecision decision = c.brain.tick(dt, c.biochem.pool());
    c.biochem.pool().consume(Chem::ATP, decision.atpCost);
    c.lastAction = decision.action;
    applyMotorAction(c, decision.action, dt);
}

void SimSystem::applyMotorAction(Creature& c, MotorAction action, float dt) {
    float speed = c.phenotype.speed;
    c.vel = {0.f, 0.f};

    auto foodGradientDir = [&](glm::vec2 pos) {
        float best = m_grid.sampleFoodNearby(pos, 2.f);
        glm::vec2 bestDir{0.f, 0.f};
        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            glm::vec2 check = pos + glm::vec2{(float)dx[d], (float)dy[d]};
            float food = m_grid.sampleFoodNearby(check, 2.f);
            if (food > best + 0.0001f) {
                best = food;
                bestDir = {(float)dx[d], (float)dy[d]};
            }
        }
        return bestDir;
    };

    switch (action) {
    case MotorAction::RUN:
        speed *= 2.f;
        c.biochem.spendATP(2.f, dt);
        [[fallthrough]];
    case MotorAction::APPROACH:
    case MotorAction::COME:
    {
        glm::vec2 bestDir = foodGradientDir(c.pos);
        if (glm::length(bestDir) > 0.f) {
            c.vel = bestDir * speed;
        } else {
            float angle = c.facingAngle;
            c.vel = {std::cos(angle) * speed * 0.5f, std::sin(angle) * speed * 0.5f};
            c.facingAngle += 0.3f;
        }
        break;
    }
    case MotorAction::EAT:
        if (handleEat(c) <= 0.01f) {
            glm::vec2 bestDir = foodGradientDir(c.pos);
            if (glm::length(bestDir) > 0.f) {
                c.vel = bestDir * speed * 0.75f;
            } else {
                float angle = c.facingAngle;
                c.vel = {std::cos(angle) * speed * 0.35f, std::sin(angle) * speed * 0.35f};
                c.facingAngle += 0.25f;
            }
        }
        break;
    case MotorAction::SPEAK:
        c.biochem.inject(Chem::COMFORT, 6.f * dt);
        c.biochem.inject(Chem::OXYTOCIN, 4.f * dt);
        c.biochem.inject(Chem::REWARD, 2.f * dt);
        break;
    case MotorAction::SLEEP:
        c.biochem.pool().consume(Chem::TIREDNESS, 20.f * dt);
        c.biochem.inject(Chem::ATP, 5.f * dt);
        break;
    case MotorAction::ATTACK:
        c.vel = {0.f, 0.f};
        break;
    default:
        break;
    }

    c.pos += c.vel * dt;

    const float margin = 0.5f;
    if (c.pos.x < margin) { c.pos.x = margin; c.vel.x = 0.f; }
    if (c.pos.y < margin) { c.pos.y = margin; c.vel.y = 0.f; }
    if (c.pos.x > WorldGrid::WIDTH - margin)  { c.pos.x = WorldGrid::WIDTH - margin; c.vel.x = 0.f; }
    if (c.pos.y > WorldGrid::HEIGHT - margin) { c.pos.y = WorldGrid::HEIGHT - margin; c.vel.y = 0.f; }
}

float SimSystem::handleEat(Creature& c) {
    int tx = m_grid.tileX(c.pos.x);
    int ty = m_grid.tileY(c.pos.y);
    float eaten = m_grid.consumeFood(tx, ty, 0.4f);
    if (eaten > 0.01f) {
        c.biochem.eatFood(eaten * 80.f, eaten * 15.f, eaten * 5.f);
        c.ateFoodThisTick = true;
        if (onEvent) onEvent(m_simTime, c.name, "ate food");
    }
    return eaten;
}

SocialMemory* SimSystem::findOrCreateSocialMemory(Creature& c, uint64_t otherId) {
    if (otherId == 0 || otherId == c.id) return nullptr;

    for (auto& memory : c.socialMemory) {
        if (memory.otherId == otherId)
            return &memory;
    }

    if ((int)c.socialMemory.size() < MAX_SOCIAL_MEMORY_ENTRIES) {
        c.socialMemory.push_back({otherId, 0.f, m_simTime});
        return &c.socialMemory.back();
    }

    size_t worstIdx = 0;
    for (size_t i = 1; i < c.socialMemory.size(); ++i) {
        const SocialMemory& candidate = c.socialMemory[i];
        const SocialMemory& incumbent = c.socialMemory[worstIdx];
        float candidateAbs = std::fabs(candidate.valence);
        float incumbentAbs = std::fabs(incumbent.valence);

        if (candidateAbs < incumbentAbs ||
            (candidateAbs == incumbentAbs && candidate.lastInteractionTime < incumbent.lastInteractionTime) ||
            (candidateAbs == incumbentAbs &&
             candidate.lastInteractionTime == incumbent.lastInteractionTime &&
             candidate.otherId > incumbent.otherId)) {
            worstIdx = i;
        }
    }

    c.socialMemory[worstIdx] = {otherId, 0.f, m_simTime};
    return &c.socialMemory[worstIdx];
}

const SocialMemory* SimSystem::findSocialMemory(const Creature& c, uint64_t otherId) const {
    for (const auto& memory : c.socialMemory) {
        if (memory.otherId == otherId)
            return &memory;
    }
    return nullptr;
}

void SimSystem::resolveSocialInteractions(float dt) {
    auto addBoundedValence = [&](Creature& from, uint64_t otherId, float delta) {
        if (SocialMemory* memory = findOrCreateSocialMemory(from, otherId)) {
            memory->valence = std::max(-1.f, std::min(1.f, memory->valence + delta));
            memory->lastInteractionTime = m_simTime;
        }
    };

    for (size_t i = 0; i < m_creatures.size(); ++i) {
        Creature& a = *m_creatures[i];
        if (!a.alive) continue;

        for (size_t j = i + 1; j < m_creatures.size(); ++j) {
            Creature& b = *m_creatures[j];
            if (!b.alive) continue;

            float dist = glm::length(a.pos - b.pos);
            if (dist > 2.5f) continue;

            bool sameSpecies = a.speciesId == b.speciesId;
            bool safePair = m_grid.zoneAt(a.pos) == 1 && m_grid.zoneAt(b.pos) == 1;

            if (sameSpecies && dist <= 2.25f) {
                float comfortGain = 12.f * dt * (0.5f + 0.5f * (a.phenotype.socialAffinity + b.phenotype.socialAffinity) * 0.5f);
                a.biochem.inject(Chem::COMFORT, comfortGain);
                b.biochem.inject(Chem::COMFORT, comfortGain);
                a.biochem.pool().consume(Chem::LONELINESS, 10.f * dt);
                b.biochem.pool().consume(Chem::LONELINESS, 10.f * dt);
                a.biochem.inject(Chem::OXYTOCIN, 5.f * dt);
                b.biochem.inject(Chem::OXYTOCIN, 5.f * dt);
                a.gotComfortThisTick = true;
                b.gotComfortThisTick = true;

                if (safePair && dist <= 1.5f) {
                    const SocialMemory* memoryA = findSocialMemory(a, b.id);
                    const SocialMemory* memoryB = findSocialMemory(b, a.id);
                    float lastA = memoryA ? memoryA->lastInteractionTime : -1000.f;
                    float lastB = memoryB ? memoryB->lastInteractionTime : -1000.f;
                    if (m_simTime - lastA >= 1.0f) addBoundedValence(a, b.id, 0.08f);
                    if (m_simTime - lastB >= 1.0f) addBoundedValence(b, a.id, 0.08f);
                } else {
                    const SocialMemory* memoryA = findSocialMemory(a, b.id);
                    const SocialMemory* memoryB = findSocialMemory(b, a.id);
                    float lastA = memoryA ? memoryA->lastInteractionTime : -1000.f;
                    float lastB = memoryB ? memoryB->lastInteractionTime : -1000.f;
                    if (m_simTime - lastA >= 2.0f) addBoundedValence(a, b.id, 0.02f);
                    if (m_simTime - lastB >= 2.0f) addBoundedValence(b, a.id, 0.02f);
                }
            }

            if (dist <= 1.35f) {
                if (a.lastAction == MotorAction::ATTACK) {
                    b.biochem.inject(Chem::PAIN, 24.f);
                    b.biochem.inject(Chem::FEAR, 18.f);
                    a.biochem.inject(Chem::REWARD, 6.f);
                    b.tookDamageThisTick = true;
                    addBoundedValence(b, a.id, -0.30f);
                    if (onEvent) onEvent(m_simTime, a.name, std::string("attacked ") + b.name);
                }
                if (b.lastAction == MotorAction::ATTACK) {
                    a.biochem.inject(Chem::PAIN, 24.f);
                    a.biochem.inject(Chem::FEAR, 18.f);
                    b.biochem.inject(Chem::REWARD, 6.f);
                    a.tookDamageThisTick = true;
                    addBoundedValence(a, b.id, -0.30f);
                    if (onEvent) onEvent(m_simTime, b.name, std::string("attacked ") + a.name);
                }
            }
        }
    }
}

void SimSystem::resolveReproduction() {
    std::vector<BirthRequest> births;
    std::vector<bool> reserved(m_creatures.size(), false);

    for (size_t i = 0; i < m_creatures.size(); ++i) {
        Creature& parentA = *m_creatures[i];
        if (!parentA.alive || reserved[i] || !isReadyToReproduce(parentA))
            continue;

        size_t bestIndex = m_creatures.size();
        float bestScore = -1.f;
        uint64_t bestId = 0;

        for (size_t j = 0; j < m_creatures.size(); ++j) {
            if (i == j || reserved[j]) continue;
            Creature& candidate = *m_creatures[j];
            float score = reproductionScore(parentA, candidate);
            if (score < 0.f) continue;

            if (score > bestScore + 0.0001f ||
                (std::fabs(score - bestScore) <= 0.0001f && candidate.id < bestId)) {
                bestScore = score;
                bestIndex = j;
                bestId = candidate.id;
            }
        }

        if (bestIndex == m_creatures.size())
            continue;

        Creature& parentB = *m_creatures[bestIndex];
        reserved[i] = true;
        reserved[bestIndex] = true;

        glm::vec2 midpoint = (parentA.pos + parentB.pos) * 0.5f;
        midpoint.x += (randomUnit() - 0.5f) * 1.5f;
        midpoint.y += (randomUnit() - 0.5f) * 1.5f;
        midpoint.x = std::max(0.5f, std::min((float)WorldGrid::WIDTH - 0.5f, midpoint.x));
        midpoint.y = std::max(0.5f, std::min((float)WorldGrid::HEIGHT - 0.5f, midpoint.y));

        births.push_back({parentA.id, parentB.id, midpoint, buildChildGenome(parentA, parentB)});

        float atpCostA = 26.f + parentA.phenotype.size * 2.0f;
        float atpCostB = 26.f + parentB.phenotype.size * 2.0f;
        parentA.biochem.pool().consume(Chem::ATP, atpCostA);
        parentB.biochem.pool().consume(Chem::ATP, atpCostB);
        parentA.biochem.inject(Chem::COMFORT, 10.f);
        parentB.biochem.inject(Chem::COMFORT, 10.f);
        parentA.biochem.pool().consume(Chem::LONELINESS, 16.f);
        parentB.biochem.pool().consume(Chem::LONELINESS, 16.f);
        parentA.reproductionCooldown = std::max(18.f, parentA.phenotype.reproductionRate * 0.25f);
        parentB.reproductionCooldown = std::max(18.f, parentB.phenotype.reproductionRate * 0.25f);
    }

    for (const BirthRequest& birth : births) {
        uint64_t childId = spawnCreature(birth.pos, birth.genome, birth.parentAId);
        if (Creature* child = findCreature(childId)) {
            child->reproductionCooldown = std::max(25.f, child->phenotype.reproductionRate * 0.5f);
            child->biochem.pool().set(Chem::ATP, 70.f);
            child->biochem.pool().set(Chem::COMFORT, 120.f);
            if (onEvent) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "born from %llu + %llu (%zu mutations)",
                    (unsigned long long)birth.parentAId,
                    (unsigned long long)birth.parentBId,
                    birth.genome.mutationLog.size());
                onEvent(m_simTime, child->name, buf);
            }
        }
    }
}

void SimSystem::updateOutcomeLearning(Creature& c) {
    float atpGain = c.biochem.pool().get(Chem::ATP) - c.tickStartATP;
    float comfortGain = c.biochem.pool().get(Chem::COMFORT) - c.tickStartComfort;
    float painGain = c.biochem.pool().get(Chem::PAIN) - c.tickStartPain;
    float fearGain = c.biochem.pool().get(Chem::FEAR) - c.tickStartFear;

    bool positiveOutcome =
        c.ateFoodThisTick || atpGain >= 8.f || comfortGain >= 12.f || c.gotComfortThisTick;
    bool negativeOutcome =
        c.tookDamageThisTick || painGain >= 8.f || fearGain >= 10.f ||
        c.biochem.pool().get(Chem::PUNISH) >= 5.f;

    if (!positiveOutcome && !negativeOutcome)
        return;

    for (const RecentSignalObservation& obs : c.recentSignalObservations) {
        if (m_simTime - obs.simTime > 1.5f) continue;

        SignalAssociation& assoc = c.signalAssociations[signalTypeIndex(obs.type)];
        if (positiveOutcome) {
            assoc.positive = std::min(1.5f, assoc.positive + 0.18f * obs.strength);
        }
        if (negativeOutcome) {
            assoc.negative = std::min(1.5f, assoc.negative + 0.22f * obs.strength);
        }

        if (obs.emitterId != 0 && obs.emitterId != c.id) {
            if (positiveOutcome && obs.type == SignalType::COMFORT) {
                if (SocialMemory* memory = findOrCreateSocialMemory(c, obs.emitterId)) {
                    memory->valence = std::min(1.f, memory->valence + 0.12f);
                    memory->lastInteractionTime = m_simTime;
                }
            }
            if (negativeOutcome && obs.type == SignalType::FEAR) {
                if (SocialMemory* memory = findOrCreateSocialMemory(c, obs.emitterId)) {
                    memory->valence = std::max(-1.f, memory->valence - 0.10f);
                    memory->lastInteractionTime = m_simTime;
                }
            }
        }
    }
}

void SimSystem::decaySocialState(Creature& c, float dt) {
    for (auto& memory : c.socialMemory) {
        if (memory.valence > 0.f) {
            memory.valence = std::max(0.f, memory.valence - 0.015f * dt);
        } else if (memory.valence < 0.f) {
            memory.valence = std::min(0.f, memory.valence + 0.015f * dt);
        }
    }

    c.recentSignalObservations.erase(
        std::remove_if(
            c.recentSignalObservations.begin(),
            c.recentSignalObservations.end(),
            [&](const RecentSignalObservation& obs) {
                return m_simTime - obs.simTime > 1.5f;
            }),
        c.recentSignalObservations.end());
}

void SimSystem::refreshRenderSignals(const std::vector<SignalPulse>& tickSignals, float dt) {
    for (SignalPulse& pulse : m_activeSignals)
        pulse.ttl -= dt;

    m_activeSignals.erase(
        std::remove_if(
            m_activeSignals.begin(),
            m_activeSignals.end(),
            [](const SignalPulse& pulse) { return pulse.ttl <= 0.f || pulse.intensity <= 0.f; }),
        m_activeSignals.end());

    for (const SignalPulse& pulse : tickSignals) {
        auto it = std::find_if(
            m_activeSignals.begin(),
            m_activeSignals.end(),
            [&](const SignalPulse& existing) {
                return existing.emitterId == pulse.emitterId &&
                       existing.type == pulse.type &&
                       existing.playerAuthored == pulse.playerAuthored;
            });

        if (it != m_activeSignals.end()) {
            *it = pulse;
        } else {
            m_activeSignals.push_back(pulse);
        }
    }
}

void SimSystem::cullDead() {
    m_creatures.erase(
        std::remove_if(
            m_creatures.begin(),
            m_creatures.end(),
            [this](const std::unique_ptr<Creature>& c) {
                if (!c->alive) {
                    m_species.onDeath(c->speciesId);
                    if (onEvent) onEvent(m_simTime, c->name, "died");
                    return true;
                }
                return false;
            }),
        m_creatures.end());
}

void SimSystem::killCreature(uint64_t id) {
    if (auto* c = findCreature(id)) {
        c->alive = false;
        m_species.onDeath(c->speciesId);
        if (onEvent) onEvent(m_simTime, c->name, "killed by player");
    }
}

void SimSystem::feedCreature(uint64_t id, float amount) {
    if (auto* c = findCreature(id)) {
        c->biochem.eatFood(amount, amount * 0.2f, amount * 0.1f);
        if (onEvent) onEvent(m_simTime, c->name, "fed by player");
    }
}

Creature* SimSystem::findCreature(uint64_t id) {
    for (auto& c : m_creatures) {
        if (c->id == id) return c.get();
    }
    return nullptr;
}

void SimSystem::setScenarioMode(ScenarioMode s) {
    m_scenario = s;
    switch (s) {
    case ScenarioMode::PARADISE:
        m_grid.addFood(50, 50, 45, 0.9f);
        break;
    case ScenarioMode::PANDEMIC:
        for (int i = 0; i < 5; ++i)
            m_grid.addToxin(20 * i, 50, 6, 0.8f);
        break;
    default:
        break;
    }
}
