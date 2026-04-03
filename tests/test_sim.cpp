// =============================================================================
//  test_sim.cpp  - Sim integration tests
// =============================================================================
#include "sim/Creature.hpp"
#include "sim/SimSystem.hpp"
#include "brain/Brain.hpp"
#include "biochem/ChemID.hpp"
#define SDL_MAIN_HANDLED
#include "render/DebugOverlay.hpp"

#include <cmath>
#include <cstdio>

static int g_passed = 0;
static int g_failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { std::printf("  [PASS] %s\n", name); ++g_passed; }
    else      { std::printf("  [FAIL] %s\n", name); ++g_failed; }
}

static bool nearf(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

static float wrapDegrees(float degrees) {
    while (degrees < 0.f) degrees += 360.f;
    while (degrees >= 360.f) degrees -= 360.f;
    return degrees;
}

static float circularDistanceDegrees(float a, float b) {
    float delta = wrapDegrees(a) - wrapDegrees(b);
    while (delta > 180.f) delta -= 360.f;
    while (delta < -180.f) delta += 360.f;
    return std::fabs(delta);
}

static Creature* newestCreature(SimSystem& sim) {
    Creature* newest = nullptr;
    for (const auto& c : sim.creatures()) {
        if (!newest || c->id > newest->id) newest = c.get();
    }
    return newest;
}

static void primeForReproduction(Creature& creature, float ageSeconds = 120.f) {
    creature.age = ageSeconds;
    creature.reproductionCooldown = 0.f;
    creature.biochem.pool().set(Chem::ATP, 190.f);
    creature.biochem.pool().set(Chem::COMFORT, 175.f);
    creature.biochem.pool().set(Chem::FEAR, 10.f);
    creature.biochem.pool().set(Chem::PAIN, 0.f);
    creature.biochem.pool().set(Chem::LONELINESS, 40.f);
    creature.biochem.pool().set(Chem::HUNGER_CARB, 90.f);
}

static bool hasSignal(const SimSystem& sim, SignalType type, bool playerAuthored = false) {
    for (const SignalPulse& pulse : sim.activeSignals()) {
        if (pulse.type == type && pulse.playerAuthored == playerAuthored)
            return true;
    }
    return false;
}

static void test_percepts_land_in_expected_brain_slots() {
    SimSystem sim;
    sim.init(77, 1);

    sim.grid().reset();
    sim.grid().setZone(20, 20, 3, 1);
    sim.grid().addFood(20, 20, 2, 0.8f);
    sim.grid().addToxin(23, 20, 1, 1.0f);

    Creature* creature = sim.creatures().front().get();
    creature->pos = {20.5f, 20.5f};
    creature->vel = {0.f, 0.f};

    sim.tick(0.05f);

    const Brain& brain = creature->brain;
    check(brain.getNeuronOutput(Brain::LOBE_PERCEPT, Percept::FOOD_NEARBY) > 0.05f,
          "food percept maps into FOOD_NEARBY");
    check(brain.getNeuronOutput(Brain::LOBE_PERCEPT, Percept::THREAT_NEARBY) > 0.01f,
          "toxin percept maps into THREAT_NEARBY");
    check(brain.getNeuronOutput(Brain::LOBE_PERCEPT, Percept::IN_SAFE_ZONE) > 0.9f,
          "safe-zone percept maps into IN_SAFE_ZONE");
    check(brain.getNeuronOutput(Brain::LOBE_PERCEPT, Percept::IN_HAZARD_ZONE) < 0.1f,
          "hazard-zone percept stays clear when not in hazard");
}

static void test_signal_emission_thresholds() {
    SimSystem sim;
    sim.init(99, 1);
    sim.grid().reset();
    sim.grid().addFood(20, 20, 2, 0.9f);

    Creature* creature = sim.creatures().front().get();
    creature->pos = {20.5f, 20.5f};
    creature->biochem.pool().set(Chem::HUNGER_CARB, 180.f);
    creature->biochem.pool().set(Chem::FEAR, 160.f);
    creature->biochem.pool().set(Chem::COMFORT, 170.f);

    sim.tick(0.05f);

    check(hasSignal(sim, SignalType::HUNGER), "hunger threshold emits HUNGER signal");
    check(hasSignal(sim, SignalType::FEAR), "fear threshold emits FEAR signal");
    check(hasSignal(sim, SignalType::COMFORT), "comfort threshold emits COMFORT signal");
    check(hasSignal(sim, SignalType::FOOD), "food-rich tile emits FOOD signal");
}

static void test_player_signal_perception_uses_same_system() {
    SimSystem sim;
    sim.init(123, 1);

    Creature* creature = sim.creatures().front().get();
    creature->pos = {30.5f, 30.5f};

    sim.queuePlayerSignal(creature->pos, SignalType::COMFORT, 1.f);
    sim.tick(0.05f);

    check(hasSignal(sim, SignalType::COMFORT, true), "player-authored signal is present in active signals");
    check(creature->perceivedSignals[signalTypeIndex(SignalType::COMFORT)] > 0.9f,
          "player-authored signal is perceived through the same pipeline");
}

static void test_strongest_signal_wins_per_type() {
    SimSystem sim;
    sim.init(222, 1);

    Creature* creature = sim.creatures().front().get();
    creature->pos = {40.5f, 40.5f};

    sim.queuePlayerSignal(creature->pos, SignalType::FEAR, 0.4f);
    sim.queuePlayerSignal({44.5f, 40.5f}, SignalType::FEAR, 1.0f);
    sim.tick(0.05f);

    float fear = creature->perceivedSignals[signalTypeIndex(SignalType::FEAR)];
    check(fear > 0.35f && fear < 0.45f, "signal perception uses strongest-per-type aggregation");
}

static void test_food_signal_learning_becomes_positive() {
    SimSystem sim;
    sim.init(333, 1);
    sim.grid().reset();
    sim.grid().addFood(25, 25, 2, 0.9f);

    Creature* creature = sim.creatures().front().get();
    creature->pos = {25.5f, 25.5f};
    creature->biochem.pool().set(Chem::HUNGER_CARB, 180.f);

    for (int i = 0; i < 20; ++i) {
        sim.queuePlayerSignal(creature->pos, SignalType::FOOD, 1.f);
        sim.tick(0.05f);
    }

    const SignalAssociation& assoc = creature->signalAssociations[signalTypeIndex(SignalType::FOOD)];
    check(assoc.positive > 0.f, "food-followed-by-eating builds positive FOOD signal valence");
}

static void test_social_memory_updates_and_decays() {
    SimSystem sim;
    sim.init(444, 2);
    sim.grid().reset();
    sim.grid().setZone(20, 20, 4, 1);

    Creature* a = sim.creatures()[0].get();
    Creature* b = sim.creatures()[1].get();
    a->pos = {20.5f, 20.5f};
    b->pos = {21.0f, 20.5f};

    for (int i = 0; i < 30; ++i) sim.tick(0.05f);

    float before = 0.f;
    for (const SocialMemory& memory : a->socialMemory) {
        if (memory.otherId == b->id) before = memory.valence;
    }
    check(before > 0.f, "safe proximity builds positive social memory");

    b->pos = {80.f, 80.f};
    for (int i = 0; i < 200; ++i) sim.tick(0.05f);

    float after = 0.f;
    for (const SocialMemory& memory : a->socialMemory) {
        if (memory.otherId == b->id) after = memory.valence;
    }
    check(after < before, "social memory decays over time");
}

static void test_visual_traits_are_deterministic_and_mutation_visible() {
    Genome base = Genome::makeDefault();
    base.lineageId = 1;
    base.lineageHueDegrees = 120.f;
    base.genes[Genome::GENE_METABOLISM] = 0.8f;
    base.genes[Genome::GENE_PERCEPTION] = 0.7f;
    base.genes[Genome::GENE_WIRES] = 0.8f;

    Creature a;
    Creature b;
    a.init(base, 1);
    b.init(base, 2);

    check(nearf(a.visualTraits.hue, b.visualTraits.hue), "visual hue is deterministic for identical genomes");
    check(nearf(a.visualTraits.brightness, b.visualTraits.brightness), "visual brightness is deterministic for identical genomes");
    check(a.visualTraits.pattern == b.visualTraits.pattern, "visual pattern is deterministic for identical genomes");

    uint32_t rng = 77;
    Genome mutated = base.mutate(1.0f, rng);
    Creature child;
    child.init(mutated, 3, 1);

    bool visiblyDifferent =
        !nearf(a.visualTraits.hue, child.visualTraits.hue) ||
        !nearf(a.visualTraits.brightness, child.visualTraits.brightness) ||
        a.visualTraits.pattern != child.visualTraits.pattern ||
        a.visualTraits.appendage != child.visualTraits.appendage;
    check(visiblyDifferent, "mutated genome changes at least one visible trait");
}

static void test_same_seed_stays_deterministic() {
    SimSystem left;
    SimSystem right;
    left.init(555, 2);
    right.init(555, 2);

    for (int tick = 0; tick < 12; ++tick) {
        if (tick == 2 || tick == 7) {
            left.queuePlayerSignal(left.creatures()[0]->pos, SignalType::COMFORT, 1.f);
            right.queuePlayerSignal(right.creatures()[0]->pos, SignalType::COMFORT, 1.f);
        }
        left.tick(0.05f);
        right.tick(0.05f);
    }

    const Creature& a = *left.creatures()[0];
    const Creature& b = *right.creatures()[0];
    check(nearf(a.visualTraits.hue, b.visualTraits.hue), "same seed produces identical lineage hue");
    check(nearf(a.signalAssociations[signalTypeIndex(SignalType::COMFORT)].positive,
                b.signalAssociations[signalTypeIndex(SignalType::COMFORT)].positive),
          "same seed and player signals produce identical learned signal valence");
    check(left.activeSignals().size() == right.activeSignals().size(),
          "same seed produces identical active signal counts");
}

static void test_reproduction_requires_safe_context_and_resources() {
    {
        SimSystem sim;
        sim.init(777, 0);
        sim.grid().reset();
        sim.grid().setZone(20, 20, 4, 1);
        sim.grid().addFood(20, 20, 2, 0.9f);

        Genome a = Genome::makeDefault();
        Genome b = Genome::makeDefault();
        a.lineageId = 10; a.lineageHueDegrees = 30.f;
        b.lineageId = 20; b.lineageHueDegrees = 60.f;
        a.genes[Genome::GENE_REPRO_RATE] = 1.0f;
        b.genes[Genome::GENE_REPRO_RATE] = 1.0f;

        sim.spawnCreature({20.5f, 20.5f}, a);
        sim.spawnCreature({21.0f, 20.5f}, b);
        primeForReproduction(*sim.creatures()[0]);
        primeForReproduction(*sim.creatures()[1]);
        sim.creatures()[1]->biochem.pool().set(Chem::ATP, 80.f);

        sim.tick(0.05f);
        check(sim.creatures().size() == 2, "low ATP blocks reproduction");
    }

    {
        SimSystem sim;
        sim.init(778, 0);
        sim.grid().reset();
        sim.grid().setZone(20, 20, 4, 2);
        sim.grid().addFood(20, 20, 2, 0.9f);

        Genome a = Genome::makeDefault();
        Genome b = Genome::makeDefault();
        a.lineageId = 10; a.lineageHueDegrees = 30.f;
        b.lineageId = 20; b.lineageHueDegrees = 60.f;
        a.genes[Genome::GENE_REPRO_RATE] = 1.0f;
        b.genes[Genome::GENE_REPRO_RATE] = 1.0f;

        sim.spawnCreature({20.5f, 20.5f}, a);
        sim.spawnCreature({21.0f, 20.5f}, b);
        primeForReproduction(*sim.creatures()[0]);
        primeForReproduction(*sim.creatures()[1]);

        sim.tick(0.05f);
        check(sim.creatures().size() == 2, "hazard context blocks reproduction");
    }
}

static void test_reproduction_creates_child_with_inherited_lineage() {
    SimSystem sim;
    sim.init(888, 0);
    sim.grid().reset();
    sim.grid().setZone(20, 20, 4, 1);
    sim.grid().addFood(20, 20, 2, 0.9f);

    Genome a = Genome::makeDefault();
    Genome b = Genome::makeDefault();
    a.generation = 4;
    b.generation = 7;
    a.lineageId = 101;
    b.lineageId = 202;
    a.lineageHueDegrees = 30.f;
    b.lineageHueDegrees = 60.f;
    a.genes[Genome::GENE_REPRO_RATE] = 1.0f;
    b.genes[Genome::GENE_REPRO_RATE] = 1.0f;
    a.genes[Genome::GENE_AGGRESSION] = 0.20f;
    b.genes[Genome::GENE_AGGRESSION] = 0.25f;
    b.genes[Genome::GENE_METABOLISM] = 0.65f;

    sim.spawnCreature({20.5f, 20.5f}, a);
    sim.spawnCreature({21.0f, 20.5f}, b);
    primeForReproduction(*sim.creatures()[0], 160.f);
    primeForReproduction(*sim.creatures()[1], 160.f);

    sim.tick(0.05f);

    check(sim.creatures().size() == 3, "ready parents produce a child");
    Creature* child = newestCreature(sim);
    check(child != nullptr && child->parentId == sim.creatures()[0]->id,
          "child keeps initiating parent as parentId");
    check(child != nullptr && child->genome.lineageId == a.lineageId,
          "child inherits initiating parent lineage ID");
    check(child != nullptr && child->genome.generation == 8,
          "child generation increments from the older parent generation");
    check(child != nullptr && !child->genome.mutationLog.empty(),
          "child birth preserves mutation logging");

    float expectedHue = 45.f;
    check(child != nullptr &&
          circularDistanceDegrees(child->genome.lineageHueDegrees, expectedHue) <= 6.01f,
          "child lineage hue stays within bounded drift of blended parent hue");
}

static void test_reproduction_is_deterministic_with_same_seed() {
    auto setupParents = [](SimSystem& sim) {
        sim.init(999, 0);
        sim.grid().reset();
        sim.grid().setZone(20, 20, 4, 1);
        sim.grid().addFood(20, 20, 2, 0.9f);

        Genome a = Genome::makeDefault();
        Genome b = Genome::makeDefault();
        a.lineageId = 301;
        b.lineageId = 302;
        a.lineageHueDegrees = 120.f;
        b.lineageHueDegrees = 150.f;
        a.generation = 2;
        b.generation = 2;
        a.genes[Genome::GENE_REPRO_RATE] = 1.0f;
        b.genes[Genome::GENE_REPRO_RATE] = 1.0f;
        a.genes[Genome::GENE_SOCIAL] = 0.8f;
        b.genes[Genome::GENE_SOCIAL] = 0.75f;
        b.genes[Genome::GENE_WIRES] = 0.7f;

        sim.spawnCreature({20.5f, 20.5f}, a);
        sim.spawnCreature({21.0f, 20.5f}, b);
        primeForReproduction(*sim.creatures()[0], 140.f);
        primeForReproduction(*sim.creatures()[1], 140.f);
    };

    SimSystem left;
    SimSystem right;
    setupParents(left);
    setupParents(right);

    left.tick(0.05f);
    right.tick(0.05f);

    Creature* leftChild = newestCreature(left);
    Creature* rightChild = newestCreature(right);
    bool sameGenes = leftChild != nullptr && rightChild != nullptr;
    if (sameGenes) {
        for (int i = 0; i < Genome::GENE_COUNT; ++i) {
            if (!nearf(leftChild->genome.genes[i], rightChild->genome.genes[i])) {
                sameGenes = false;
                break;
            }
        }
    }

    check(left.creatures().size() == right.creatures().size(),
          "same seed creates the same number of births");
    check(sameGenes, "same seed creates identical child genes");
    check(leftChild != nullptr && rightChild != nullptr &&
          nearf(leftChild->genome.lineageHueDegrees, rightChild->genome.lineageHueDegrees),
          "same seed creates identical child lineage hue");
    check(leftChild != nullptr && rightChild != nullptr &&
          leftChild->genome.mutationLog.size() == rightChild->genome.mutationLog.size(),
          "same seed preserves deterministic mutation logging");
}

static void test_default_startup_produces_motion_without_input() {
    SimSystem sim;
    sim.init(12345, 6);

    std::vector<glm::vec2> startPositions;
    startPositions.reserve(sim.creatures().size());
    for (const auto& c : sim.creatures())
        startPositions.push_back(c->pos);

    for (int i = 0; i < 40; ++i)
        sim.tick(0.05f);

    bool anyMoved = false;
    for (size_t i = 0; i < sim.creatures().size() && i < startPositions.size(); ++i) {
        if (glm::length(sim.creatures()[i]->pos - startPositions[i]) > 0.40f) {
            anyMoved = true;
            break;
        }
    }

    check(anyMoved, "default startup produces visible motion without player input");
}

static void test_default_startup_emits_non_food_signals_early() {
    SimSystem sim;
    sim.init(12345, 6);

    bool sawInternalSignal = false;
    for (int i = 0; i < 12 && !sawInternalSignal; ++i) {
        sim.tick(0.05f);
        for (const SignalPulse& pulse : sim.activeSignals()) {
            if (pulse.type != SignalType::FOOD) {
                sawInternalSignal = true;
                break;
            }
        }
    }

    check(sawInternalSignal, "default startup emits visible non-food signals early");
}

static void test_default_startup_shows_signal_variety() {
    SimSystem sim;
    sim.init(12345, 6);

    bool sawTypes[SIGNAL_TYPE_COUNT] = {false, false, false, false};
    for (int i = 0; i < 20; ++i) {
        sim.tick(0.05f);
        for (const SignalPulse& pulse : sim.activeSignals()) {
            sawTypes[signalTypeIndex(pulse.type)] = true;
        }
    }

    int distinctTypes = 0;
    for (bool saw : sawTypes) if (saw) ++distinctTypes;
    check(distinctTypes >= 2, "default startup shows multiple signal types early");
}

static void test_debug_overlay_button_mapping() {
    DebugOverlay overlay;
    int screenW = 1280;
    int screenH = 720;

    check(overlay.buttonAt(18, screenH - 18, screenW, screenH) == 1,
          "bottom-left first HUD button maps to overlay 1");
    check(overlay.buttonAt(76, screenH - 18, screenW, screenH) == 2,
          "bottom-left second HUD button maps to overlay 2");
    check(overlay.buttonAt(134, screenH - 18, screenW, screenH) == 3,
          "bottom-left third HUD button maps to overlay 3");
    check(overlay.buttonAt(192, screenH - 18, screenW, screenH) == 4,
          "bottom-left fourth HUD button maps to overlay 4");
    check(overlay.buttonAt(260, screenH - 18, screenW, screenH) == 0,
          "clicks outside HUD buttons do not trigger overlay toggles");
}

static void test_sim_time_and_creature_action_state_advance() {
    SimSystem sim;
    sim.init(12345, 2);

    float before = sim.simTime();
    sim.tick(0.05f);

    check(nearf(sim.simTime(), before + 0.05f),
          "sim time advances by dt each tick");
    check(!sim.creatures().empty() &&
          sim.creatures().front()->lastAction <= MotorAction::APPROACH,
          "runtime creature action state remains available after ticking");
}

int main() {
    std::printf("\n");
    std::printf("==========================================\n");
    std::printf("  ALife Sim - Sim Integration Tests\n");
    std::printf("==========================================\n\n");

    test_percepts_land_in_expected_brain_slots();
    test_signal_emission_thresholds();
    test_player_signal_perception_uses_same_system();
    test_strongest_signal_wins_per_type();
    test_food_signal_learning_becomes_positive();
    test_social_memory_updates_and_decays();
    test_visual_traits_are_deterministic_and_mutation_visible();
    test_same_seed_stays_deterministic();
    test_reproduction_requires_safe_context_and_resources();
    test_reproduction_creates_child_with_inherited_lineage();
    test_reproduction_is_deterministic_with_same_seed();
    test_default_startup_produces_motion_without_input();
    test_default_startup_emits_non_food_signals_early();
    test_default_startup_shows_signal_variety();
    test_debug_overlay_button_mapping();
    test_sim_time_and_creature_action_state_advance();

    std::printf("\n==========================================\n");
    std::printf("  Results: %d passed, %d failed\n", g_passed, g_failed);
    std::printf("==========================================\n\n");
    return g_failed > 0 ? 1 : 0;
}
