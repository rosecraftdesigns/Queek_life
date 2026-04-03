#include "sim/Genome.hpp"
#include <cstdio>
#include <algorithm>
#include <cmath>

// Simple LCG helper
static float lcgFloat(uint32_t& rng) {
    rng = rng * 1664525u + 1013904223u;
    return (float)(rng >> 8) / (float)(1 << 24);  // [0, 1)
}

static float wrapDegrees(float degrees) {
    while (degrees < 0.f) degrees += 360.f;
    while (degrees >= 360.f) degrees -= 360.f;
    return degrees;
}

static float circularBlendDegrees(float a, float b) {
    float delta = b - a;
    while (delta > 180.f) delta -= 360.f;
    while (delta < -180.f) delta += 360.f;
    return wrapDegrees(a + delta * 0.5f);
}

Genome Genome::makeDefault() {
    Genome g;
    g.genes.fill(0.5f);
    g.generation = 0;
    g.lineageId  = 0;
    g.lineageHueDegrees = 0.f;
    return g;
}

Genome Genome::mutate(float mutationRate, uint32_t& rngSeed) const {
    Genome child = *this;
    child.generation = generation + 1;
    child.mutationLog.clear();

    for (int i = 0; i < GENE_COUNT; ++i) {
        if (lcgFloat(rngSeed) < mutationRate) {
            float oldVal = child.genes[i];
            // Gaussian-ish mutation: small delta biased toward existing value
            float delta = (lcgFloat(rngSeed) - 0.5f) * 0.4f;
            float newVal = oldVal + delta;
            if (newVal < 0.f) newVal = 0.f;
            if (newVal > 1.f) newVal = 1.f;
            child.genes[i] = newVal;
            child.mutationLog.push_back({(uint8_t)i, oldVal, newVal, child.generation});
        }
    }
    return child;
}

Genome Genome::crossover(const Genome& a, const Genome& b, uint32_t& rngSeed) {
    Genome child;
    child.generation = std::max(a.generation, b.generation) + 1;
    child.lineageId  = a.lineageId;  // inherit first parent's lineage
    child.lineageHueDegrees = circularBlendDegrees(a.lineageHueDegrees, b.lineageHueDegrees);
    for (int i = 0; i < GENE_COUNT; ++i) {
        // 50/50 per gene, plus blend
        float t = lcgFloat(rngSeed);
        child.genes[i] = t < 0.5f ? a.genes[i] : b.genes[i];
        // Clamp
        if (child.genes[i] < 0.f) child.genes[i] = 0.f;
        if (child.genes[i] > 1.f) child.genes[i] = 1.f;
    }
    return child;
}

std::string Genome::summary() const {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "Gen%u Lin%llu hue=%.1f spd=%.2f met=%.2f agg=%.2f per=%.2f siz=%.2f soc=%.2f",
        generation, (unsigned long long)lineageId,
        lineageHueDegrees,
        genes[GENE_SPEED], genes[GENE_METABOLISM],
        genes[GENE_AGGRESSION], genes[GENE_PERCEPTION],
        genes[GENE_SIZE], genes[GENE_SOCIAL]);
    return buf;
}
