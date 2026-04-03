#include "sim/Species.hpp"
#include <cmath>
#include <cstdio>

float SpeciesTracker::genomeDistance(const Genome& a, const Genome& b) const {
    float sumSq = 0.f;
    for (int i = 0; i < Genome::GENE_COUNT; ++i) {
        float d = a.genes[i] - b.genes[i];
        sumSq += d * d;
    }
    return std::sqrt(sumSq / Genome::GENE_COUNT);  // normalised RMS distance
}

uint64_t SpeciesTracker::classify(const Genome& g, uint64_t creatureId) {
    // Find the closest existing species archetype
    float bestDist = 1e9f;
    size_t bestIdx = 0;
    for (size_t i = 0; i < m_archetypes.size(); ++i) {
        float d = genomeDistance(g, m_archetypes[i]);
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    if (bestDist <= SPECIATION_THRESHOLD && !m_archetypes.empty()) {
        // Belongs to existing species
        Species& sp = m_species[bestIdx];
        sp.livingCount++;
        // Update running averages
        int n = (int)sp.livingCount;
        sp.avgMetabolism = sp.avgMetabolism + (g.genes[Genome::GENE_METABOLISM] - sp.avgMetabolism) / n;
        sp.avgAggression = sp.avgAggression + (g.genes[Genome::GENE_AGGRESSION] - sp.avgAggression) / n;
        sp.avgSpeed      = sp.avgSpeed      + (g.genes[Genome::GENE_SPEED]      - sp.avgSpeed)      / n;
        return sp.id;
    }

    // New species
    char name[32];
    std::snprintf(name, sizeof(name), "Species-%03llu", (unsigned long long)m_nextId);
    Species sp;
    sp.id           = m_nextId++;
    sp.name         = name;
    sp.ancestorId   = creatureId;
    sp.livingCount  = 1;
    sp.avgMetabolism = g.genes[Genome::GENE_METABOLISM];
    sp.avgAggression = g.genes[Genome::GENE_AGGRESSION];
    sp.avgSpeed      = g.genes[Genome::GENE_SPEED];
    m_species.push_back(sp);
    m_archetypes.push_back(g);
    return sp.id;
}

void SpeciesTracker::onDeath(uint64_t speciesId) {
    for (auto& sp : m_species) {
        if (sp.id == speciesId && sp.livingCount > 0) {
            sp.livingCount--;
            return;
        }
    }
}
