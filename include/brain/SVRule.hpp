#pragma once
// =============================================================================
//  SVRule.hpp  —  State Value Rule (register machine op on a dendrite)
//
//  CyberLife Design Principle
//  --------------------------
//  Traditional neural nets use floating-point weights and backpropagation.
//  SVRules replace the weight with a tiny opcode + operand pair that defines
//  HOW an input modifies the dendrite's accumulator state.
//
//  Key properties:
//    • Every legal opcode produces a bounded float — no crashes on mutation
//    • Unknown opcodes silently become NOP (safe default)
//    • Learning (reinforcement) modifies `learnedGain` in-place, not the opcode
//    • Evolution mutates opcodes, `baseGain`, and `threshold` freely
//
//  Execution contract
//  ------------------
//  execute() returns a DELTA to be added to the neuron's pre-activation state.
//  All inputs and outputs are in the range [-3, +3] (pre-sigmoid).
//
//  Reinforcement learning
//  ----------------------
//  REINFORCE_POS / REINFORCE_NEG opcodes scale their contribution by the
//  current reward/punish signal from the biochemistry:
//      effective contribution = base * (1 + reward) * input
//  After each tick, updateLearning() adjusts `learnedGain` using Hebbian
//  gated learning so the pathway strengthens if rewarded repeatedly.
// =============================================================================

#include <cstdint>
#include <algorithm>

struct SVRule
{
    // -----------------------------------------------------------------------
    // Opcodes
    // -----------------------------------------------------------------------
    enum Op : uint8_t
    {
        NOP            = 0,  // no contribution
        ADD            = 1,  // delta = input * baseGain
        SUB            = 2,  // delta = -(input * baseGain)
        THRESHOLD_GT   = 3,  // delta = baseGain  if input > threshold, else 0
        THRESHOLD_LT   = 4,  // delta = baseGain  if input < threshold, else 0
        MULTIPLY_STATE = 5,  // delta = currentState * baseGain - currentState
                             //          (scales current accumulator)
        REINFORCE_POS  = 6,  // delta = input * learnedGain * (1 + rewardSig)
        REINFORCE_NEG  = 7,  // delta = -(input * learnedGain * (1 + punishSig))
        DECAY_STATE    = 8,  // delta = -(currentState * baseGain)
                             //          (leakage toward zero)
        ABS_INPUT      = 9,  // delta = |input| * baseGain   (magnitude-sensitive)
        OP_COUNT       = 10
    };

    Op    op           = ADD;
    float baseGain     = 0.5f;   // set by genome; not modified by learning
    float learnedGain  = 0.5f;   // modified by reinforcement learning
    float threshold    = 0.5f;   // used by THRESHOLD_GT / THRESHOLD_LT

    // -----------------------------------------------------------------------
    // execute
    //   input        : source neuron's output [0, 1]
    //   currentState : the neuron's pre-activation accumulator before this dendrite
    //   rewardSig    : REWARD chemical / 255   [0, 1]
    //   punishSig    : PUNISH chemical / 255   [0, 1]
    //   returns      : delta to add to accumulator
    // -----------------------------------------------------------------------
    float execute(float input,
                  float currentState,
                  float rewardSig,
                  float punishSig) const
    {
        switch (op)
        {
            case NOP:
                return 0.0f;

            case ADD:
                return input * baseGain;

            case SUB:
                return -(input * baseGain);

            case THRESHOLD_GT:
                return (input > threshold) ? baseGain : 0.0f;

            case THRESHOLD_LT:
                return (input < threshold) ? baseGain : 0.0f;

            case MULTIPLY_STATE:
                // Scale the current accumulator — returns the delta needed
                return currentState * (baseGain - 1.0f);

            case REINFORCE_POS:
                return input * learnedGain * (1.0f + rewardSig);

            case REINFORCE_NEG:
                return -(input * learnedGain * (1.0f + punishSig));

            case DECAY_STATE:
                return -(currentState * baseGain);

            case ABS_INPUT:
                return (input < 0.0f ? -input : input) * baseGain;

            default:
                return 0.0f;   // unknown opcode → safe NOP
        }
    }

    // -----------------------------------------------------------------------
    // updateLearning
    //   Called after each brain tick to adjust learnedGain.
    //   Only REINFORCE_POS / REINFORCE_NEG rules are modified.
    //
    //   Rule: if reward is high and this dendrite was active, increase gain.
    //         if punish is high, decrease gain.
    //         learnedGain decays slowly toward baseGain when no signal.
    // -----------------------------------------------------------------------
    void updateLearning(float input, float rewardSig, float punishSig,
                        float learningRate = 0.01f)
    {
        if (op != REINFORCE_POS && op != REINFORCE_NEG) return;

        float activity = input * learnedGain;

        if (op == REINFORCE_POS)
        {
            learnedGain += learningRate * rewardSig * activity;
            learnedGain -= learningRate * punishSig * activity;
        }
        else
        {
            learnedGain += learningRate * punishSig * activity;
            learnedGain -= learningRate * rewardSig * activity;
        }

        // Clamp and decay toward baseGain
        learnedGain = std::max(0.01f, std::min(4.0f, learnedGain));
        learnedGain += (baseGain - learnedGain) * 0.001f; // slow mean-reversion
    }
};
