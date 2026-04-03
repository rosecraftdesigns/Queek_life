#pragma once
#include "biochem/BiochemEngine.hpp"
#include "biochem/ChemID.hpp"

// Computes REWARD/PUNISH chemical injections based on biochemical drive changes.
// Called by SimSystem after each biochem.tick() to close the reinforcement loop.
struct RewardSystem {
    static constexpr float REWARD_ON_HUNGER_DECREASE = 40.f;
    static constexpr float PUNISH_ON_HUNGER_INCREASE  = 20.f;
    static constexpr float REWARD_ON_PAIN_DECREASE    = 30.f;
    static constexpr float PUNISH_ON_PAIN_INCREASE    = 50.f;
    static constexpr float REWARD_ON_SLEEP_CYCLE      = 20.f;

    struct DriveSnapshot {
        float hungerCarb;
        float pain;
        float sleepiness;
        float atp;
    };

    DriveSnapshot captureSnapshot(const ChemicalPool& pool) const {
        return {
            pool.get(Chem::HUNGER_CARB),
            pool.get(Chem::PAIN),
            pool.get(Chem::SLEEPINESS),
            pool.get(Chem::ATP)
        };
    }

    void apply(BiochemEngine& biochem,
               const DriveSnapshot& before,
               const DriveSnapshot& after) const {
        float reward = 0.f, punish = 0.f;

        // Hunger decreased = reward (ate successfully)
        float dHunger = after.hungerCarb - before.hungerCarb;
        if (dHunger < -5.f) reward += REWARD_ON_HUNGER_DECREASE;
        if (dHunger >  5.f) punish += PUNISH_ON_HUNGER_INCREASE;

        // Pain decreased = reward, pain increased = punish
        float dPain = after.pain - before.pain;
        if (dPain < -5.f) reward += REWARD_ON_PAIN_DECREASE;
        if (dPain >  5.f) punish += PUNISH_ON_PAIN_INCREASE;

        // Sleepiness decreased (slept) = small reward
        float dSleep = after.sleepiness - before.sleepiness;
        if (dSleep < -10.f) reward += REWARD_ON_SLEEP_CYCLE;

        if (reward > 0.f) biochem.inject(Chem::REWARD,  reward);
        if (punish > 0.f) biochem.inject(Chem::PUNISH,  punish);
    }
};
