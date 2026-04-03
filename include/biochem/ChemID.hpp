#pragma once
// =============================================================================
//  ChemID.hpp  —  Named indices into the 256-slot chemical pool
//
//  Conventions
//  -----------
//  • IDs 0–63   : reserved, named by this header
//  • IDs 64–255 : free for genome-defined chemicals
//  • Concentration range: 0.0f – 255.0f  (float32 throughout)
//  • "Drive" chemicals: 0 = satisfied, 255 = critical need
// =============================================================================

#include <cstdint>

namespace Chem {

enum ID : uint8_t
{
    // -----------------------------------------------------------------------
    // 0: Null / placeholder
    // -----------------------------------------------------------------------
    NULL_CHEM       = 0,

    // -----------------------------------------------------------------------
    // 1–9: Metabolic substrates
    // -----------------------------------------------------------------------
    GLUCOSE         = 1,   // Primary carbohydrate fuel
    OXYGEN          = 2,   // Required for aerobic respiration
    AMINO_ACID      = 3,   // Protein catabolism product
    FATTY_ACID      = 4,   // Fat catabolism product
    WATER           = 5,

    // -----------------------------------------------------------------------
    // 10–14: Energy currency
    // -----------------------------------------------------------------------
    ATP             = 10,  // Adenosine triphosphate — universal energy token
    ADP             = 11,  // Spent ATP; recycled back to ATP via respiration

    // -----------------------------------------------------------------------
    // 15–19: Metabolic waste
    // -----------------------------------------------------------------------
    CO2             = 15,  // Produced by aerobic respiration
    LACTATE         = 16,  // Produced when O2 is insufficient (anaerobic)
    UREA            = 17,  // Protein catabolism waste

    // -----------------------------------------------------------------------
    // 20–35: Drive chemicals
    //   High value = creature is in need; brain decision lobe responds
    // -----------------------------------------------------------------------
    HUNGER_CARB     = 20,  // Need for carbohydrates / glucose
    HUNGER_PROTEIN  = 21,  // Need for amino acids
    HUNGER_FAT      = 22,  // Need for fatty acids
    SLEEPINESS      = 23,  // Need for sleep; cleared by sleeping
    PAIN            = 24,  // Injury / nociception signal
    SEX_DRIVE       = 25,  // Reproductive urge
    LONELINESS      = 26,  // Social deprivation
    CROWDING        = 27,  // Over-stimulation / claustrophobia
    BOREDOM         = 28,  // Insufficient novel stimulation
    ANGER           = 29,  // Frustration; spikes on blocked goals
    FEAR            = 30,  // Threat-level response
    COMFORT         = 31,  // Inverse-drive: high value = well-being

    // -----------------------------------------------------------------------
    // 36–45: Hormones
    // -----------------------------------------------------------------------
    ADRENALINE      = 36,  // Fight-or-flight; boosts speed, suppresses sleep
    OESTROGEN       = 37,  // Female reproductive cycle
    TESTOSTERONE    = 38,  // Male reproductive + baseline aggression
    CORTISOL        = 39,  // Chronic stress; suppresses immune system
    SEROTONIN       = 40,  // Mood stability; low = depression-like state
    DOPAMINE        = 41,  // Reward anticipation; reinforces behaviour
    MELATONIN       = 42,  // Circadian; rises in darkness, induces sleep
    OXYTOCIN        = 43,  // Social bonding; released on touch
    INSULIN         = 44,  // Drives glucose into storage; lowers blood glucose
    GLUCAGON        = 45,  // Mobilises stored glucose; raises blood glucose

    // -----------------------------------------------------------------------
    // 46–55: Toxins & antidotes
    // -----------------------------------------------------------------------
    CYANIDE         = 46,  // Halts ATP production (cytochrome-c oxidase block)
    ALCOHOL         = 47,  // Depresses neural firing rates
    HISTAMINE       = 48,  // Inflammation mediator; elevated by injuries
    ANTITOXIN       = 49,  // General antidote; reduces several toxins
    ARSENIC         = 50,  // Disrupts glycolysis; prevents glucose → ATP
    ANTIHISTAMINE   = 51,  // Cyanide/histamine antidote

    // -----------------------------------------------------------------------
    // 56–63: Neuro-modulator signals (brain ↔ biochem bridge)
    // -----------------------------------------------------------------------
    REWARD          = 56,  // Positive reinforcement pulse (lobe input)
    PUNISH          = 57,  // Negative reinforcement pulse (lobe input)
    TIREDNESS       = 58,  // Accumulates during waking; cleared by sleep
    WAKEFULNESS     = 59,  // Antagonises tiredness; elevated by light/activity
    ATTENTION       = 60,  // Modulates perceptual lobe sensitivity

    // Boundary — genome-defined chemicals start here
    NAMED_COUNT     = 64
};

// Convenience: total pool size (fixed, always 256)
static constexpr int POOL_SIZE = 256;

// Human-readable names for the first 64 slots (for debugging / Scientist Mode)
inline const char* name(uint8_t id)
{
    switch (static_cast<ID>(id))
    {
        case NULL_CHEM:     return "null";
        case GLUCOSE:       return "glucose";
        case OXYGEN:        return "oxygen";
        case AMINO_ACID:    return "amino_acid";
        case FATTY_ACID:    return "fatty_acid";
        case WATER:         return "water";
        case ATP:           return "ATP";
        case ADP:           return "ADP";
        case CO2:           return "CO2";
        case LACTATE:       return "lactate";
        case UREA:          return "urea";
        case HUNGER_CARB:   return "hunger_carb";
        case HUNGER_PROTEIN:return "hunger_protein";
        case HUNGER_FAT:    return "hunger_fat";
        case SLEEPINESS:    return "sleepiness";
        case PAIN:          return "pain";
        case SEX_DRIVE:     return "sex_drive";
        case LONELINESS:    return "loneliness";
        case CROWDING:      return "crowding";
        case BOREDOM:       return "boredom";
        case ANGER:         return "anger";
        case FEAR:          return "fear";
        case COMFORT:       return "comfort";
        case ADRENALINE:    return "adrenaline";
        case OESTROGEN:     return "oestrogen";
        case TESTOSTERONE:  return "testosterone";
        case CORTISOL:      return "cortisol";
        case SEROTONIN:     return "serotonin";
        case DOPAMINE:      return "dopamine";
        case MELATONIN:     return "melatonin";
        case OXYTOCIN:      return "oxytocin";
        case INSULIN:       return "insulin";
        case GLUCAGON:      return "glucagon";
        case CYANIDE:       return "cyanide";
        case ALCOHOL:       return "alcohol";
        case HISTAMINE:     return "histamine";
        case ANTITOXIN:     return "antitoxin";
        case ARSENIC:       return "arsenic";
        case ANTIHISTAMINE: return "antihistamine";
        case REWARD:        return "reward";
        case PUNISH:        return "punish";
        case TIREDNESS:     return "tiredness";
        case WAKEFULNESS:   return "wakefulness";
        case ATTENTION:     return "attention";
        default:            return "genome_chem";
    }
}

} // namespace Chem
