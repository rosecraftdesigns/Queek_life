#include "sim/GenomePhenotype.hpp"
#include "biochem/BiochemEngine.hpp"

GenomePhenotype GenomePhenotype::fromGenome(const Genome& g) {
    GenomePhenotype p;
    p.speed            = g.genes[Genome::GENE_SPEED]      * 6.f + 1.f;   // [1,7]
    p.metabolism       = g.genes[Genome::GENE_METABOLISM] * 1.5f + 0.5f; // [0.5,2.0]
    p.aggression       = g.genes[Genome::GENE_AGGRESSION] * 3.f;         // [0,3]
    p.perceptionRadius = g.genes[Genome::GENE_PERCEPTION] * 8.f + 2.f;   // [2,10]
    p.size             = g.genes[Genome::GENE_SIZE]        * 6.f + 4.f;   // [4,10]
    p.reproductionRate = 300.f - g.genes[Genome::GENE_REPRO_RATE] * 240.f; // [60,300]s
    p.socialAffinity   = g.genes[Genome::GENE_SOCIAL];                    // [0,1]
    p.learningRate     = g.genes[Genome::GENE_WIRES]       * 1.5f + 0.5f; // [0.5,2.0]
    p.glowIntensity    = g.genes[Genome::GENE_WIRES];                     // [0,1]
    return p;
}

void GenomePhenotype::apply(BiochemEngine& biochem) const {
    // Scale aerobic respiration kcat by metabolism gene
    if (auto* rxn = biochem.findReaction("aerobic_respiration")) {
        rxn->setKcat(0.04f * metabolism);
    }

    // Scale anaerobic glycolysis
    if (auto* rxn = biochem.findReaction("anaerobic_glycolysis")) {
        rxn->setKcat(0.012f * metabolism);
    }

    // Scale lung O2 supply by speed (faster creature needs more O2)
    if (auto* emit = biochem.findEmitter("lung_o2_supply")) {
        emit->setGain(30.f * (0.5f + speed / 14.f));  // faster = more O2
    }

    // Scale adrenal emitter by aggression
    if (auto* emit = biochem.findEmitter("adrenal_fear_spike")) {
        emit->setGain(25.f * (aggression / 3.f + 0.1f));
    }
}
