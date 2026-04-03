#pragma once
// =============================================================================
//  MotorAction.hpp  —  The 11 discrete outputs of the Decision lobe
//
//  The Decision lobe uses Winner-Takes-All. The neuron with the highest
//  post-activation value maps directly to one of these actions.
//  Index order is fixed by the genome; mutation can rewire WHICH concept
//  neurons feed each slot but cannot change the action meanings.
// =============================================================================

#include <cstdint>

enum class MotorAction : uint8_t
{
    QUIESCENT  = 0,   // Do nothing; default resting state
    PUSH       = 1,   // Push the attended object away
    PULL       = 2,   // Pull the attended object closer
    STOP       = 3,   // Cease current locomotion
    COME       = 4,   // Move toward a called target
    RUN        = 5,   // Flee at maximum speed
    EAT        = 6,   // Consume the attended food object
    ATTACK     = 7,   // Strike the attended creature/object
    SPEAK      = 8,   // Emit a vocalisation
    SLEEP      = 9,   // Enter rest state; clears TIREDNESS
    APPROACH   = 10,  // Move toward the attended object (gentle)
};

static constexpr int NUM_MOTOR_ACTIONS = 11;

inline const char* motorActionName(MotorAction a)
{
    switch (a)
    {
        case MotorAction::QUIESCENT: return "quiescent";
        case MotorAction::PUSH:      return "push";
        case MotorAction::PULL:      return "pull";
        case MotorAction::STOP:      return "stop";
        case MotorAction::COME:      return "come";
        case MotorAction::RUN:       return "run";
        case MotorAction::EAT:       return "eat";
        case MotorAction::ATTACK:    return "attack";
        case MotorAction::SPEAK:     return "speak";
        case MotorAction::SLEEP:     return "sleep";
        case MotorAction::APPROACH:  return "approach";
        default:                     return "unknown";
    }
}
