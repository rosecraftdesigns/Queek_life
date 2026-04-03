// =============================================================================
//  test_genome.cpp  — Phase 3 genome/phenotype unit tests
// =============================================================================
#include "sim/Genome.hpp"
#include "sim/GenomePhenotype.hpp"
#include "sim/Species.hpp"
#include "biochem/BiochemEngine.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <algorithm>

static int g_passed = 0, g_failed = 0;
static void check(bool cond, const char* name) {
    if (cond) { std::printf("  [PASS] %s\n", name); ++g_passed; }
    else       { std::printf("  [FAIL] %s\n", name); ++g_failed; }
}

void test_makeDefault_inRange() {
    Genome g = Genome::makeDefault();
    bool ok = true;
    for (float v : g.genes) if (v < 0.f || v > 1.f) ok = false;
    check(ok, "makeDefault: all genes in [0,1]");
}

void test_mutate_rate0_identical() {
    Genome g = Genome::makeDefault();
    uint32_t seed = 42;
    Genome m = g.mutate(0.f, seed);
    bool same = true;
    for (int i = 0; i < Genome::GENE_COUNT; ++i)
        if (m.genes[i] != g.genes[i]) same = false;
    check(same, "mutate(0.0) produces identical genome");
    check(m.mutationLog.empty(), "mutate(0.0) has empty mutation log");
}

void test_mutate_rate1_changed() {
    Genome g = Genome::makeDefault();
    uint32_t seed = 99;
    Genome m = g.mutate(1.f, seed);
    bool anyChanged = false;
    for (int i = 0; i < Genome::GENE_COUNT; ++i)
        if (m.genes[i] != g.genes[i]) { anyChanged = true; break; }
    check(anyChanged, "mutate(1.0) changes at least one gene");
    check(!m.mutationLog.empty(), "mutate(1.0) records mutation log entries");
}

void test_mutate_stays_in_range() {
    uint32_t seed = 7777;
    Genome g = Genome::makeDefault();
    for (int i = 0; i < 50; ++i) g = g.mutate(0.5f, seed);
    bool ok = true;
    for (float v : g.genes) if (v < 0.f || v > 1.f) ok = false;
    check(ok, "repeated mutations keep genes in [0,1]");
}

void test_phenotype_in_range() {
    Genome g = Genome::makeDefault();
    GenomePhenotype p = GenomePhenotype::fromGenome(g);
    check(p.speed >= 1.f && p.speed <= 7.f, "phenotype speed in [1,7]");
    check(p.metabolism >= 0.5f && p.metabolism <= 2.f, "phenotype metabolism in [0.5,2]");
    check(p.perceptionRadius >= 2.f && p.perceptionRadius <= 10.f, "phenotype percRadius in [2,10]");
    check(p.size >= 4.f && p.size <= 10.f, "phenotype size in [4,10]");
}

void test_phenotype_apply_scales_kcat() {
    // High-metabolism creature should have higher aerobic_respiration kcat
    Genome g = Genome::makeDefault();
    g.genes[Genome::GENE_METABOLISM] = 1.0f;  // max metabolism
    GenomePhenotype pHigh = GenomePhenotype::fromGenome(g);

    g.genes[Genome::GENE_METABOLISM] = 0.0f;  // min metabolism
    GenomePhenotype pLow  = GenomePhenotype::fromGenome(g);

    BiochemEngine eHigh, eLow;
    eHigh.loadDefaults(); pHigh.apply(eHigh);
    eLow.loadDefaults();  pLow.apply(eLow);

    auto* rxnHigh = eHigh.findReaction("aerobic_respiration");
    auto* rxnLow  = eLow.findReaction("aerobic_respiration");
    check(rxnHigh && rxnLow, "aerobic_respiration reaction found");
    if (rxnHigh && rxnLow)
        check(rxnHigh->kcat() > rxnLow->kcat(),
              "high-metabolism creature has higher aerobic kcat");
}

void test_crossover_in_range() {
    uint32_t seed = 42;
    Genome a = Genome::makeDefault();
    Genome b = Genome::makeDefault();
    a.genes[0] = 0.f; b.genes[0] = 1.f;
    Genome c = Genome::crossover(a, b, seed);
    bool ok = true;
    for (float v : c.genes) if (v < 0.f || v > 1.f) ok = false;
    check(ok, "crossover genes stay in [0,1]");
}

void test_species_same_genome_same_id() {
    SpeciesTracker tracker;
    Genome g = Genome::makeDefault();
    uint64_t id1 = tracker.classify(g, 1);
    uint64_t id2 = tracker.classify(g, 2);
    check(id1 == id2, "identical genomes get same species ID");
}

void test_species_distant_genomes_different_id() {
    SpeciesTracker tracker;
    Genome a = Genome::makeDefault();
    Genome b = Genome::makeDefault();
    // Make maximally different
    for (int i = 0; i < Genome::GENE_COUNT; i += 2) a.genes[i] = 0.f;
    for (int i = 0; i < Genome::GENE_COUNT; i += 2) b.genes[i] = 1.f;
    for (int i = 1; i < Genome::GENE_COUNT; i += 2) a.genes[i] = 1.f;
    for (int i = 1; i < Genome::GENE_COUNT; i += 2) b.genes[i] = 0.f;
    uint64_t id1 = tracker.classify(a, 1);
    uint64_t id2 = tracker.classify(b, 2);
    check(id1 != id2, "maximally different genomes get different species IDs");
}

int main() {
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════╗\n");
    std::printf("║  ALife Sim — Phase 3 Genome Tests        ║\n");
    std::printf("╚══════════════════════════════════════════╝\n\n");

    test_makeDefault_inRange();
    test_mutate_rate0_identical();
    test_mutate_rate1_changed();
    test_mutate_stays_in_range();
    test_phenotype_in_range();
    test_phenotype_apply_scales_kcat();
    test_crossover_in_range();
    test_species_same_genome_same_id();
    test_species_distant_genomes_different_id();

    std::printf("\n══════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", g_passed, g_failed);
    std::printf("══════════════════════════════════════════\n\n");
    return g_failed > 0 ? 1 : 0;
}
