// =============================================================================
//  test_world.cpp  — Phase 2 world grid unit tests
// =============================================================================
#include "world/WorldGrid.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>

static int g_passed = 0, g_failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { std::printf("  [PASS] %s\n", name); ++g_passed; }
    else       { std::printf("  [FAIL] %s\n", name); ++g_failed; }
}

// ---- Tests ------------------------------------------------------------------

void test_seedDefault_nonZero() {
    WorldGrid g;
    g.seedDefault(42);
    float totalFood = 0.f;
    for (int ty = 0; ty < WorldGrid::HEIGHT; ++ty)
        for (int tx = 0; tx < WorldGrid::WIDTH; ++tx)
            totalFood += g.at(tx, ty).food;
    check(totalFood > 0.f, "seedDefault produces non-zero food");
}

void test_foodGrowsOnFertileTile() {
    WorldGrid g;
    g.reset();
    g.at(5, 5).fertility = 0.8f;
    g.at(5, 5).food      = 0.1f;
    float before = g.at(5, 5).food;
    for (int i = 0; i < 200; ++i) g.tick(0.05f);  // 10s
    float after = g.at(5, 5).food;
    check(after > before, "food grows on fertile tile");
}

void test_zeroFertilityNoGrowth() {
    WorldGrid g;
    g.reset();
    g.at(5, 5).fertility = 0.f;
    g.at(5, 5).food      = 0.f;
    for (int i = 0; i < 200; ++i) g.tick(0.05f);
    check(g.at(5, 5).food == 0.f, "zero-fertility tile stays at 0 food");
}

void test_sampleFoodNearby() {
    WorldGrid g;
    g.reset();
    check(g.sampleFoodNearby({50.f, 50.f}, 3.f) == 0.f,
          "sampleFoodNearby returns 0 on empty grid");

    g.at(50, 50).food = 0.8f;
    check(g.sampleFoodNearby({50.5f, 50.5f}, 2.f) > 0.f,
          "sampleFoodNearby returns >0 after setting food");
}

void test_zoneQuery() {
    WorldGrid g;
    g.reset();
    g.setZone(20, 20, 4, 1);  // safe zone
    g.setZone(80, 80, 4, 2);  // hazard zone
    check(g.zoneAt({20.5f, 20.5f}) == 1, "zone query: safe zone correct");
    check(g.zoneAt({80.5f, 80.5f}) == 2, "zone query: hazard zone correct");
    check(g.zoneAt({50.5f, 50.5f}) == 0, "zone query: neutral zone correct");
}

void test_consumeFood() {
    WorldGrid g;
    g.reset();
    g.at(10, 10).food = 0.7f;
    float consumed = g.consumeFood(10, 10, 0.3f);
    check(std::fabs(consumed - 0.3f) < 0.001f, "consumeFood returns correct amount");
    check(std::fabs(g.at(10, 10).food - 0.4f) < 0.001f, "consumeFood reduces tile food");
}

void test_toxinDecays() {
    WorldGrid g;
    g.reset();
    g.at(30, 30).toxin = 1.f;
    for (int i = 0; i < 4000; ++i) g.tick(0.05f);  // 200s
    check(g.at(30, 30).toxin < 0.5f, "toxin decays over time");
}

// ---- Main -------------------------------------------------------------------

int main() {
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════╗\n");
    std::printf("║  ALife Sim — Phase 2 World Tests         ║\n");
    std::printf("╚══════════════════════════════════════════╝\n\n");

    test_seedDefault_nonZero();
    test_foodGrowsOnFertileTile();
    test_zeroFertilityNoGrowth();
    test_sampleFoodNearby();
    test_zoneQuery();
    test_consumeFood();
    test_toxinDecays();

    std::printf("\n══════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", g_passed, g_failed);
    std::printf("══════════════════════════════════════════\n\n");
    return g_failed > 0 ? 1 : 0;
}
