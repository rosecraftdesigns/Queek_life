#pragma once
#include "sim/Genome.hpp"
#include <vector>
#include <string>
#include <cstdint>

struct Species {
    uint64_t    id;
    std::string name;         // "Species-001" auto-generated
    uint64_t    ancestorId;   // lineage root creature ID
    uint32_t    livingCount = 0;
    float       avgMetabolism   = 0.5f;
    float       avgAggression   = 0.5f;
    float       avgSpeed        = 0.5f;
};

// Classifies creatures into species based on genome Euclidean distance.
class SpeciesTracker {
public:
    static constexpr float SPECIATION_THRESHOLD = 0.25f;

    SpeciesTracker() = default;

    // Assign (or confirm) a species ID for a genome; creates new species if distant enough.
    uint64_t classify(const Genome& g, uint64_t creatureId = 0);

    // Called when a creature with the given speciesId dies.
    void onDeath(uint64_t speciesId);

    const std::vector<Species>& allSpecies() const { return m_species; }

private:
    float genomeDistance(const Genome& a, const Genome& b) const;

    std::vector<Species>  m_species;
    std::vector<Genome>   m_archetypes;   // one representative genome per species
    uint64_t              m_nextId = 1;
};
