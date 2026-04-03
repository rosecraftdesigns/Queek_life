#pragma once
#include "sim/Genome.hpp"

class BiochemEngine;

// Phenotypic traits derived deterministically from a Genome.
// Ranges are annotated inline.
struct GenomePhenotype {
    float speed;            // tiles/s  — gene * 6.0 + 1.0  → [1, 7]
    float metabolism;       // kcat multiplier — gene * 1.5 + 0.5  → [0.5, 2.0]
    float aggression;       // adrenal emitter gain scale — gene * 3.0  → [0, 3]
    float perceptionRadius; // tiles   — gene * 8.0 + 2.0  → [2, 10]
    float size;             // px radius at default zoom — gene * 6.0 + 4.0  → [4, 10]
    float reproductionRate; // min age to reproduce (s) — inverse: gene maps [300 → 60s]
    float socialAffinity;   // loneliness drive baseline modifier — [0, 1]
    float learningRate;     // learnedGain update speed multiplier — [0.5, 2.0]
    float glowIntensity;    // visual glow from GENE_WIRES — [0, 1]

    // Derive phenotype from genome
    static GenomePhenotype fromGenome(const Genome& g);

    // Apply genome-derived phenotype to a BiochemEngine (after loadDefaults())
    // Scales kcat and emitter gains by label lookup.
    void apply(BiochemEngine& biochem) const;
};
