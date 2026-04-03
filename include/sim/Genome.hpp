#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <string>

struct MutationLogEntry {
    uint8_t  geneIndex;
    float    oldValue;
    float    newValue;
    uint32_t generation;
};

struct Genome {
    static constexpr int GENE_COUNT = 32;

    // Named gene indices
    static constexpr int GENE_SPEED      = 0;
    static constexpr int GENE_METABOLISM = 1;
    static constexpr int GENE_AGGRESSION = 2;
    static constexpr int GENE_PERCEPTION = 3;
    static constexpr int GENE_SIZE       = 4;
    static constexpr int GENE_LIFESPAN   = 5;
    static constexpr int GENE_REPRO_RATE = 6;
    static constexpr int GENE_SOCIAL     = 7;
    static constexpr int GENE_WIRES      = 8;  // learning capacity / glow
    // 9–31: spare biochem modifier genes

    std::array<float, GENE_COUNT> genes;   // all in [0, 1]
    uint32_t generation = 0;
    uint64_t lineageId  = 0;
    float    lineageHueDegrees = 0.f;

    std::vector<MutationLogEntry> mutationLog;

    // Produce a child genome with point mutations applied.
    // mutationRate: fraction of genes to mutate each reproduction (e.g. 0.05)
    Genome mutate(float mutationRate, uint32_t& rngSeed) const;

    // Average of two genomes with LCG-seeded crossover
    static Genome crossover(const Genome& a, const Genome& b, uint32_t& rngSeed);

    // Default "wild-type" genome with mid-range genes
    static Genome makeDefault();

    // Formatted one-line summary
    std::string summary() const;
};
