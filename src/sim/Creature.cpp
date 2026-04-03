#include "sim/Creature.hpp"
#include <cstdio>
#include <algorithm>

static float wrapDegrees(float degrees) {
    while (degrees < 0.f) degrees += 360.f;
    while (degrees >= 360.f) degrees -= 360.f;
    return degrees;
}

static VisualTraits deriveVisualTraits(const Genome& genome,
                                       const GenomePhenotype& phenotype) {
    VisualTraits traits;
    traits.size = phenotype.size;
    traits.glowIntensity = phenotype.glowIntensity;

    float hue = wrapDegrees(
        genome.lineageHueDegrees +
        (genome.genes[Genome::GENE_AGGRESSION] - 0.5f) * 24.f);

    if (!genome.mutationLog.empty()) {
        const MutationLogEntry& marker = genome.mutationLog.front();
        hue = wrapDegrees(hue + ((marker.geneIndex % 2 == 0) ? 4.f : -4.f));
    }
    traits.hue = hue;
    traits.brightness = 0.45f + 0.35f * genome.genes[Genome::GENE_METABOLISM];

    float sensoryIndex =
        0.6f * genome.genes[Genome::GENE_PERCEPTION] +
        0.4f * genome.genes[Genome::GENE_WIRES];
    if (sensoryIndex < 0.25f) traits.pattern = 0;
    else if (sensoryIndex < 0.50f) traits.pattern = 1;
    else if (sensoryIndex < 0.75f) traits.pattern = 2;
    else traits.pattern = 3;

    traits.appendage = 0;
    if (genome.genes[Genome::GENE_AGGRESSION] >= 0.70f) {
        traits.appendage = 2;
    } else if (genome.genes[Genome::GENE_PERCEPTION] >= 0.70f &&
               genome.genes[Genome::GENE_AGGRESSION] < 0.60f) {
        traits.appendage = 1;
    } else if (genome.genes[Genome::GENE_SOCIAL] >= 0.70f &&
               genome.genes[Genome::GENE_WIRES] >= 0.55f) {
        traits.appendage = 3;
    }

    return traits;
}

void Creature::init(const Genome& g, uint64_t newId, uint64_t pId) {
    id       = newId;
    parentId = pId;
    genome   = g;
    phenotype = GenomePhenotype::fromGenome(g);

    biochem.loadDefaults();
    phenotype.apply(biochem);

    brain.loadDefaultWiring();

    age                  = 0.f;
    reproductionCooldown = 0.f;
    alive                = true;
    vel                  = {0.f, 0.f};
    recentSignalObservations.clear();
    socialMemory.clear();
    perceivedSignals.fill(0.f);
    dominantSignal = SignalType::HUNGER;
    dominantSignalIntensity = 0.f;
    lastAction = MotorAction::QUIESCENT;
    tickStartATP = 0.f;
    tickStartComfort = 0.f;
    tickStartPain = 0.f;
    tickStartFear = 0.f;
    ateFoodThisTick = false;
    gotComfortThisTick = false;
    tookDamageThisTick = false;
    visualTraits = deriveVisualTraits(genome, phenotype);

    // Auto-name: species prefix will be set by SimSystem after classification
    char buf[32];
    std::snprintf(buf, sizeof(buf), "C_%llu", (unsigned long long)id);
    name = buf;

    biochem.onDeath = [this](const std::string& cause) {
        alive = false;
        (void)cause;
    };
}
