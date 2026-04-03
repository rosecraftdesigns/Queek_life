#include "brain/Brain.hpp"
#include "biochem/ChemID.hpp"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cassert>

// =============================================================================
//  Static lobe size definition (required for ODR)
// =============================================================================
constexpr int Brain::LOBE_SIZES[Brain::NUM_LOBES];

// =============================================================================
//  Construction
// =============================================================================
Brain::Brain()
{
    // Compute lobe start offsets
    m_lobeStart[0] = 0;
    for (int i = 1; i < NUM_LOBES; ++i)
        m_lobeStart[i] = m_lobeStart[i-1] + LOBE_SIZES[i-1];

    assert(m_lobeStart[NUM_LOBES-1] + LOBE_SIZES[NUM_LOBES-1] == TOTAL_NEURONS);

    // Zero all neuron arrays
    m_output.fill(0.0f);
    m_state.fill(0.0f);
    m_restState.fill(0.0f);
    m_leakage.fill(0.05f);   // mild leakage by default

    // Allocate dendrite vectors
    m_dendrites.resize(TOTAL_NEURONS);
}

// =============================================================================
//  loadDefaultWiring
//
//  This is the "factory genome" — the starting wiring before evolution.
//  Philosophy:
//    • Decision lobe has DIRECT, strong connections from Drive + Percept.
//      This ensures survival behaviour without needing learned Concept paths.
//    • Concept lobe is sparsely wired with REINFORCE_POS/NEG dendrites.
//      These gain strength over the creature's lifetime and across generations.
//    • Attention lobe modulates Percept sensitivity.
//
//  Decision lobe layout (matches MotorAction enum):
//    0=QUIESCENT  1=PUSH   2=PULL     3=STOP    4=COME
//    5=RUN        6=EAT    7=ATTACK   8=SPEAK   9=SLEEP   10=APPROACH
// =============================================================================
void Brain::loadDefaultWiring()
{
    // Clear any existing wiring
    for (auto& v : m_dendrites) v.clear();

    // -----------------------------------------------------------------------
    // Decision lobe — survival-critical direct drives
    // -----------------------------------------------------------------------

    // QUIESCENT (0): baseline rest state — wins when no strong drive exists
    m_restState[globalIdx(LOBE_DECISION, 0)] = 0.35f;

    // RUN (5): fear + threat proximity → flee
    wireDecision(5, LOBE_DRIVE,   Drive::FEAR,           SVRule::ADD,          1.8f);
    wireDecision(5, LOBE_PERCEPT, Percept::THREAT_NEARBY, SVRule::ADD,          1.4f);
    wireDecision(5, LOBE_PERCEPT, Percept::SIGNAL_FEAR,   SVRule::ADD,          1.6f);
    wireDecision(5, LOBE_GENERAL, GeneralInput::SOCIAL_NEGATIVE, SVRule::ADD,   0.9f);
    wireDecision(5, LOBE_PERCEPT, Percept::SIGNAL_COMFORT, SVRule::SUB,         0.4f);
    wireDecision(5, LOBE_DRIVE,   Drive::PAIN,            SVRule::ADD,          1.0f);
    wireDecision(5, LOBE_DRIVE,   Drive::ADRENALINE,      SVRule::ADD,          0.8f);
    // Adrenaline amplifies run via threshold gate
    wireDecision(5, LOBE_DRIVE,   Drive::ADRENALINE,     SVRule::THRESHOLD_GT,  1.2f, 0.4f);

    // EAT (6): hunger + food nearby → eat
    wireDecision(6, LOBE_DRIVE,   Drive::HUNGER_CARB,    SVRule::ADD,           1.8f);
    wireDecision(6, LOBE_DRIVE,   Drive::HUNGER_PROTEIN, SVRule::ADD,           1.2f);
    wireDecision(6, LOBE_DRIVE,   Drive::HUNGER_FAT,     SVRule::ADD,           1.0f);
    wireDecision(6, LOBE_PERCEPT, Percept::FOOD_NEARBY,  SVRule::ADD,           1.5f);
    wireDecision(6, LOBE_PERCEPT, Percept::SIGNAL_FOOD,  SVRule::ADD,           1.1f);
    wireDecision(6, LOBE_PERCEPT, Percept::SIGNAL_HUNGER,SVRule::ADD,           0.5f);
    // Hunger must be elevated for food percept to maximally activate EAT
    wireDecision(6, LOBE_PERCEPT, Percept::FOOD_DISTANCE, SVRule::ADD,          0.8f);
    // Pain suppresses eating
    wireDecision(6, LOBE_DRIVE,   Drive::PAIN,           SVRule::SUB,           0.6f);

    // SLEEP (9): sleepiness + tiredness → sleep
    wireDecision(9, LOBE_DRIVE,   Drive::SLEEPINESS,     SVRule::ADD,           2.0f);
    wireDecision(9, LOBE_DRIVE,   Drive::TIREDNESS,      SVRule::ADD,           1.2f);
    // Safe zone boosts willingness to sleep
    wireDecision(9, LOBE_PERCEPT, Percept::IN_SAFE_ZONE, SVRule::ADD,           0.8f);
    // Fear suppresses sleep
    wireDecision(9, LOBE_DRIVE,   Drive::FEAR,           SVRule::SUB,           1.5f);
    wireDecision(9, LOBE_DRIVE,   Drive::PAIN,           SVRule::SUB,           0.8f);

    // APPROACH (10): boredom + curiosity → explore
    wireDecision(10, LOBE_DRIVE,   Drive::BOREDOM,       SVRule::ADD,           1.4f);
    wireDecision(10, LOBE_PERCEPT, Percept::OBJECT_NEARBY,SVRule::ADD,          1.0f);
    wireDecision(10, LOBE_PERCEPT, Percept::ALLY_NEARBY,  SVRule::ADD,          0.6f);
    wireDecision(10, LOBE_PERCEPT, Percept::SIGNAL_FOOD,  SVRule::ADD,          0.9f);
    wireDecision(10, LOBE_PERCEPT, Percept::SIGNAL_COMFORT, SVRule::ADD,        0.8f);
    wireDecision(10, LOBE_PERCEPT, Percept::SIGNAL_HUNGER,  SVRule::ADD,        0.5f);
    wireDecision(10, LOBE_GENERAL, GeneralInput::SOCIAL_POSITIVE, SVRule::ADD,  0.7f);
    wireDecision(10, LOBE_DRIVE,   Drive::LONELINESS,    SVRule::ADD,           1.0f);

    // ATTACK (7): anger + fear (fight mode) — requires both to co-activate
    wireDecision(7, LOBE_DRIVE,   Drive::ANGER,          SVRule::ADD,           1.5f);
    wireDecision(7, LOBE_DRIVE,   Drive::PAIN,           SVRule::THRESHOLD_GT,  1.0f, 0.6f);
    wireDecision(7, LOBE_PERCEPT, Percept::THREAT_NEARBY, SVRule::ADD,          0.8f);
    // Fear suppresses attack below a threshold — cowardly creatures run instead
    wireDecision(7, LOBE_DRIVE,   Drive::FEAR,           SVRule::THRESHOLD_LT,  0.8f, 0.5f);

    // SPEAK (8): loneliness + ally nearby → vocalise
    wireDecision(8, LOBE_DRIVE,   Drive::LONELINESS,    SVRule::ADD,            1.0f);
    wireDecision(8, LOBE_PERCEPT, Percept::ALLY_NEARBY,  SVRule::ADD,           0.8f);
    wireDecision(8, LOBE_PERCEPT, Percept::SIGNAL_COMFORT, SVRule::ADD,         0.9f);
    wireDecision(8, LOBE_GENERAL, GeneralInput::SOCIAL_POSITIVE, SVRule::ADD,   0.6f);
    wireDecision(8, LOBE_DRIVE,   Drive::SEX_DRIVE,     SVRule::ADD,            0.6f);

    // STOP (3): crowding + in safe zone → pause
    wireDecision(3, LOBE_DRIVE,   Drive::CROWDING,      SVRule::ADD,            1.0f);
    wireDecision(3, LOBE_PERCEPT, Percept::IN_SAFE_ZONE, SVRule::THRESHOLD_GT,  0.8f, 0.7f);

    // COME (4): called by player/trainer (general input slot 0 = trainer signal)
    wireDecision(4, LOBE_GENERAL, GeneralInput::TRAINER_SIGNAL, SVRule::ADD,    2.0f);

    // PULL (2): object nearby + curiosity
    wireDecision(2, LOBE_PERCEPT, Percept::OBJECT_NEARBY,SVRule::ADD,           0.8f);
    wireDecision(2, LOBE_DRIVE,   Drive::BOREDOM,        SVRule::ADD,           0.6f);

    // PUSH (1): object nearby + anger
    wireDecision(1, LOBE_PERCEPT, Percept::OBJECT_NEARBY, SVRule::ADD,          0.8f);
    wireDecision(1, LOBE_DRIVE,   Drive::ANGER,           SVRule::ADD,          0.6f);

    // -----------------------------------------------------------------------
    // Concept lobe — associative pathways with reinforcement learning
    //
    // The Concept lobe acts as an intermediate layer between input lobes and
    // the Decision lobe. Default wiring is sparse with REINFORCE_POS rules
    // so pathways strengthen through rewarded experience.
    //
    // We wire ~6 dendrites per Concept neuron, spread across input lobes.
    // The first 11 Concept neurons also feed forward to Decision neurons
    // (one-to-one), creating a "shortcut" path that learning can amplify.
    // -----------------------------------------------------------------------
    const int conceptSize = LOBE_SIZES[LOBE_CONCEPT];
    const int kSrcLobes[] = { LOBE_DRIVE, LOBE_PERCEPT, LOBE_NOUN,
                               LOBE_VERB,  LOBE_STIM_SRC };
    const int kSrcLobeSizes[] = { LOBE_SIZES[LOBE_DRIVE],
                                   LOBE_SIZES[LOBE_PERCEPT],
                                   LOBE_SIZES[LOBE_NOUN],
                                   LOBE_SIZES[LOBE_VERB],
                                   LOBE_SIZES[LOBE_STIM_SRC] };

    // Use a simple LCG for reproducible pseudo-random wiring
    uint32_t seed = 0xDEADBEEF;
    auto lcg = [&]() -> uint32_t {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };
    auto randFloat = [&](float lo, float hi) -> float {
        return lo + (hi - lo) * ((float)(lcg() & 0xFFFF) / 65535.0f);
    };
    auto randInt = [&](int n) -> int {
        return (int)(lcg() % (uint32_t)n);
    };

    for (int c = 0; c < conceptSize; ++c)
    {
        // Each Concept neuron gets 6 incoming dendrites from input lobes
        for (int d = 0; d < 6; ++d)
        {
            int srcLobeIdx = randInt(5);
            int srcLobe    = kSrcLobes[srcLobeIdx];
            int srcNeuron  = randInt(kSrcLobeSizes[srcLobeIdx]);
            float gain     = randFloat(0.1f, 0.6f);
            // Even-indexed dendrites are REINFORCE_POS (strengthened by reward)
            SVRule::Op op  = (d % 2 == 0) ? SVRule::REINFORCE_POS
                                           : SVRule::ADD;
            wireConcept(c, srcLobe, srcNeuron, op, gain);
        }
    }

    // First 11 Concept neurons forward to Decision (one-to-one concept→action paths)
    for (int i = 0; i < NUM_MOTOR_ACTIONS; ++i)
    {
        wireDecision(i, LOBE_CONCEPT, i, SVRule::REINFORCE_POS, 0.2f);
    }

    // -----------------------------------------------------------------------
    // Attention lobe — modulates Percept sensitivity
    // Attention neurons fire when specific drives are elevated,
    // then feed back to boost corresponding Percept neurons.
    // -----------------------------------------------------------------------
    // Attention[0]: food-seeking — hunger drives attention toward food percepts
    wireDecision(6, LOBE_ATTENTION, 0, SVRule::ADD, 0.5f);  // boosts EAT
    wireConcept(0, LOBE_DRIVE, Drive::HUNGER_CARB, SVRule::ADD, 0.8f);  // concept[0]~food

    // Attention lobe wiring (Drive → Attention)
    auto wireAttention = [&](int attNeuron, int srcLobe, int srcNeuron,
                              SVRule::Op op, float gain)
    {
        int gIdx = globalIdx(LOBE_ATTENTION, attNeuron);
        Dendrite d;
        d.srcLobe   = (uint8_t)srcLobe;
        d.srcNeuron = (uint16_t)srcNeuron;
        d.rule.op        = op;
        d.rule.baseGain  = gain;
        d.rule.learnedGain = gain;
        m_dendrites[gIdx].push_back(d);
    };

    wireAttention(0, LOBE_DRIVE, Drive::HUNGER_CARB,  SVRule::ADD, 1.5f); // food focus
    wireAttention(1, LOBE_DRIVE, Drive::FEAR,          SVRule::ADD, 1.5f); // threat focus
    wireAttention(2, LOBE_DRIVE, Drive::LONELINESS,    SVRule::ADD, 1.0f); // social focus
    wireAttention(3, LOBE_DRIVE, Drive::BOREDOM,       SVRule::ADD, 1.0f); // explore focus

    // Attention → Percept feedback (boosts percept sensitivity when attention fires)
    // Implemented by adding Attention dendrites to Percept neurons
    // (Percept neurons are normally set externally; this adds a small bias)
    auto wirePercept = [&](int perceptNeuron, int srcLobe, int srcNeuron,
                            SVRule::Op op, float gain)
    {
        int gIdx = globalIdx(LOBE_PERCEPT, perceptNeuron);
        Dendrite d;
        d.srcLobe    = (uint8_t)srcLobe;
        d.srcNeuron  = (uint16_t)srcNeuron;
        d.rule.op         = op;
        d.rule.baseGain   = gain;
        d.rule.learnedGain = gain;
        m_dendrites[gIdx].push_back(d);
    };

    wirePercept(Percept::FOOD_NEARBY,   LOBE_ATTENTION, 0, SVRule::ADD, 0.2f);
    wirePercept(Percept::THREAT_NEARBY, LOBE_ATTENTION, 1, SVRule::ADD, 0.2f);
    wirePercept(Percept::SIGNAL_FOOD,   LOBE_ATTENTION, 0, SVRule::ADD, 0.15f);
    wirePercept(Percept::SIGNAL_FEAR,   LOBE_ATTENTION, 1, SVRule::ADD, 0.15f);

    // -----------------------------------------------------------------------
    // Set leakage rates per lobe (controls how fast neurons return to rest)
    // -----------------------------------------------------------------------
    // Input lobes: no leakage (externally driven)
    for (int i = 0; i < LOBE_SIZES[LOBE_PERCEPT]; ++i)
        m_leakage[globalIdx(LOBE_PERCEPT, i)] = 0.0f;
    for (int i = 0; i < LOBE_SIZES[LOBE_DRIVE]; ++i)
        m_leakage[globalIdx(LOBE_DRIVE, i)] = 0.0f;

    // Concept lobe: moderate leakage (short-term memory effect)
    for (int i = 0; i < LOBE_SIZES[LOBE_CONCEPT]; ++i)
        m_leakage[globalIdx(LOBE_CONCEPT, i)] = 0.1f;

    // Decision lobe: fast leakage (decisions are fresh each tick)
    for (int i = 0; i < LOBE_SIZES[LOBE_DECISION]; ++i)
        m_leakage[globalIdx(LOBE_DECISION, i)] = 0.3f;
}

// =============================================================================
//  External input setters
// =============================================================================
void Brain::setDriveInputs(const ChemicalPool& pool)
{
    // Map biochem chemicals directly to Drive lobe neuron outputs.
    // Input lobes bypass the dendrite/SVRule system — they are set directly.
    auto setDrive = [&](int slot, uint8_t chemID) {
        int gIdx = globalIdx(LOBE_DRIVE, slot);
        m_output[gIdx] = pool.get(chemID) / 255.0f;
        m_state[gIdx]  = m_output[gIdx];
    };

    setDrive(Drive::HUNGER_CARB,    Chem::HUNGER_CARB);
    setDrive(Drive::HUNGER_PROTEIN, Chem::HUNGER_PROTEIN);
    setDrive(Drive::HUNGER_FAT,     Chem::HUNGER_FAT);
    setDrive(Drive::SLEEPINESS,     Chem::SLEEPINESS);
    setDrive(Drive::PAIN,           Chem::PAIN);
    setDrive(Drive::SEX_DRIVE,      Chem::SEX_DRIVE);
    setDrive(Drive::LONELINESS,     Chem::LONELINESS);
    setDrive(Drive::BOREDOM,        Chem::BOREDOM);
    setDrive(Drive::FEAR,           Chem::FEAR);
    setDrive(Drive::ANGER,          Chem::ANGER);
    setDrive(Drive::COMFORT,        Chem::COMFORT);
    setDrive(Drive::CROWDING,       Chem::CROWDING);
    setDrive(Drive::ADRENALINE,     Chem::ADRENALINE);
    setDrive(Drive::TIREDNESS,      Chem::TIREDNESS);
    setDrive(Drive::REWARD,         Chem::REWARD);
    setDrive(Drive::PUNISH,         Chem::PUNISH);
}

void Brain::setPerceptInput(int idx, float value)
{
    if (idx < 0 || idx >= LOBE_SIZES[LOBE_PERCEPT]) return;
    int gIdx = globalIdx(LOBE_PERCEPT, idx);
    m_output[gIdx] = clamp01(value);
    m_state[gIdx]  = m_output[gIdx];
}

void Brain::setNounInput(int idx, float value)
{
    if (idx < 0 || idx >= LOBE_SIZES[LOBE_NOUN]) return;
    int gIdx = globalIdx(LOBE_NOUN, idx);
    m_output[gIdx] = clamp01(value);
    m_state[gIdx]  = m_output[gIdx];
}

void Brain::setVerbInput(int idx, float value)
{
    if (idx < 0 || idx >= LOBE_SIZES[LOBE_VERB]) return;
    int gIdx = globalIdx(LOBE_VERB, idx);
    m_output[gIdx] = clamp01(value);
    m_state[gIdx]  = m_output[gIdx];
}

void Brain::setGeneralInput(int idx, float value)
{
    if (idx < 0 || idx >= LOBE_SIZES[LOBE_GENERAL]) return;
    int gIdx = globalIdx(LOBE_GENERAL, idx);
    m_output[gIdx] = clamp01(value);
    m_state[gIdx]  = m_output[gIdx];
}

void Brain::setStimSrcInput(int idx, float value)
{
    if (idx < 0 || idx >= LOBE_SIZES[LOBE_STIM_SRC]) return;
    int gIdx = globalIdx(LOBE_STIM_SRC, idx);
    m_output[gIdx] = clamp01(value);
    m_state[gIdx]  = m_output[gIdx];
}

// =============================================================================
//  tick
// =============================================================================
BrainDecision Brain::tick(float dt, const ChemicalPool& pool)
{
    // Reward/punish signals for SVRule learning
    float rewardSig = pool.get(Chem::REWARD) / 255.0f;
    float punishSig = pool.get(Chem::PUNISH) / 255.0f;
    bool  doLearn   = (rewardSig > 0.01f || punishSig > 0.01f);

    // Processing order: Attention → Concept → Decision
    // Input lobes (Percept, Drive, StimSrc, Verb, Noun, General) were already
    // set externally via setXxxInput() before this call.
    processLobe(LOBE_ATTENTION, rewardSig, punishSig, doLearn);
    processLobe(LOBE_CONCEPT,   rewardSig, punishSig, doLearn);
    processLobe(LOBE_DECISION,  rewardSig, punishSig, false);  // WTA, no learning here

    m_lastDecision = evaluateDecision(dt);
    return m_lastDecision;
}

// =============================================================================
//  processLobe
// =============================================================================
void Brain::processLobe(int lobeID, float rewardSig, float punishSig, bool learn)
{
    int start = m_lobeStart[lobeID];
    int count = LOBE_SIZES[lobeID];

    for (int local = 0; local < count; ++local)
    {
        int gIdx = start + local;

        // Apply leakage first (state decays toward restState)
        float leak = m_leakage[gIdx];
        float acc  = m_state[gIdx] * (1.0f - leak) + m_restState[gIdx] * leak;

        // Accumulate dendrite contributions
        for (auto& dend : m_dendrites[gIdx])
        {
            int srcGIdx = globalIdx(dend.srcLobe, dend.srcNeuron);
            float input = m_output[srcGIdx];

            float delta = dend.rule.execute(input, acc, rewardSig, punishSig);
            acc += delta;

            if (learn)
                dend.rule.updateLearning(input, rewardSig, punishSig);
        }

        // Clamp accumulator to prevent extreme values
        if (acc >  6.0f) acc =  6.0f;
        if (acc < -6.0f) acc = -6.0f;

        m_state[gIdx]  = acc;
        m_output[gIdx] = sigmoid(acc);
    }
}

// =============================================================================
//  evaluateDecision  — Winner-Takes-All on the Decision lobe
// =============================================================================
BrainDecision Brain::evaluateDecision(float dt) const
{
    int decStart = m_lobeStart[LOBE_DECISION];
    int decCount = LOBE_SIZES[LOBE_DECISION];

    // Gather all activations
    struct Entry { int idx; float activation; };
    std::array<Entry, NUM_MOTOR_ACTIONS> entries;
    for (int i = 0; i < decCount; ++i)
        entries[i] = { i, m_output[decStart + i] };

    // Sort descending by activation
    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b){ return a.activation > b.activation; });

    // Compute total neural activity for ATP cost
    float totalActivity = 0.0f;
    for (int i = 0; i < TOTAL_NEURONS; ++i)
        totalActivity += m_output[i];

    BrainDecision dec;
    dec.action     = static_cast<MotorAction>(entries[0].idx);
    dec.confidence = entries[0].activation;
    dec.atpCost    = totalActivity * ATP_COST_PER_ACTIVATION * dt;

    for (int k = 0; k < 3; ++k)
    {
        dec.top3[k].action     = static_cast<MotorAction>(entries[k].idx);
        dec.top3[k].activation = entries[k].activation;
    }

    dec.dominantDriveLabel = buildDriveLabel();

    // Build human-readable reason line
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "action=%s (%.2f) vs %s (%.2f) | %s",
        motorActionName(dec.top3[0].action), dec.top3[0].activation,
        motorActionName(dec.top3[1].action), dec.top3[1].activation,
        dec.dominantDriveLabel.c_str());
    dec.reasonLine = buf;

    return dec;
}

// =============================================================================
//  buildDriveLabel
// =============================================================================
std::string Brain::buildDriveLabel() const
{
    // Find the highest drive signal
    static const char* kDriveNames[] = {
        "hunger_carb","hunger_protein","hunger_fat","sleepiness",
        "pain","sex_drive","loneliness","boredom","fear","anger",
        "comfort","crowding","adrenaline","tiredness","reward","punish"
    };

    int   bestDrive = 0;
    float bestVal   = 0.0f;
    for (int i = 0; i < LOBE_SIZES[LOBE_DRIVE]; ++i)
    {
        float v = m_output[globalIdx(LOBE_DRIVE, i)];
        if (v > bestVal) { bestVal = v; bestDrive = i; }
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s=%.2f", kDriveNames[bestDrive], bestVal);
    return std::string(buf);
}

// =============================================================================
//  Queries
// =============================================================================
float Brain::getNeuronOutput(int lobeID, int localIdx) const
{
    return m_output[globalIdx(lobeID, localIdx)];
}

int Brain::globalIdx(int lobeID, int localIdx) const
{
    return m_lobeStart[lobeID] + localIdx;
}

float Brain::lobeNeuronOutput(int lobeID, int localIdx) const
{
    return m_output[globalIdx(lobeID, localIdx)];
}

// =============================================================================
//  Sigmoid activation function
//  Using a fast piecewise approximation that avoids std::exp overhead.
//  Returns values in (0, 1).
// =============================================================================
float Brain::sigmoid(float x)
{
    // Standard logistic: 1 / (1 + exp(-x))
    // Fast approximation for |x| <= 6 (clamped at entry)
    return 1.0f / (1.0f + std::exp(-x));
}

// =============================================================================
//  Wire helpers
// =============================================================================
void Brain::wireDecision(int decisionNeuron, int srcLobe, int srcNeuron,
                          SVRule::Op op, float gain, float threshold)
{
    int gIdx = globalIdx(LOBE_DECISION, decisionNeuron);
    Dendrite d;
    d.srcLobe          = (uint8_t)srcLobe;
    d.srcNeuron        = (uint16_t)srcNeuron;
    d.rule.op          = op;
    d.rule.baseGain    = gain;
    d.rule.learnedGain = gain;
    d.rule.threshold   = threshold;
    m_dendrites[gIdx].push_back(d);
}

void Brain::wireConcept(int conceptNeuron, int srcLobe, int srcNeuron,
                         SVRule::Op op, float gain, float threshold)
{
    int gIdx = globalIdx(LOBE_CONCEPT, conceptNeuron);
    Dendrite d;
    d.srcLobe          = (uint8_t)srcLobe;
    d.srcNeuron        = (uint16_t)srcNeuron;
    d.rule.op          = op;
    d.rule.baseGain    = gain;
    d.rule.learnedGain = gain;
    d.rule.threshold   = threshold;
    m_dendrites[gIdx].push_back(d);
}

// =============================================================================
//  Debug / Scientist Mode output
// =============================================================================
void Brain::dumpLobe(int lobeID, const char* label) const
{
    std::printf("--- Lobe[%d] %s ---\n", lobeID, label ? label : "");
    int start = m_lobeStart[lobeID];
    int count = LOBE_SIZES[lobeID];
    for (int i = 0; i < count; ++i)
    {
        float v = m_output[start + i];
        if (v > 0.01f)
        {
            int bars = (int)(v * 20.0f);
            std::printf("  [%3d] ", i);
            for (int b = 0; b < 20; ++b) std::putchar(b < bars ? '#' : '.');
            std::printf(" %.3f\n", v);
        }
    }
}

void Brain::dumpDecision() const
{
    std::printf("--- Decision Lobe (WTA) ---\n");
    int start = m_lobeStart[LOBE_DECISION];
    for (int i = 0; i < NUM_MOTOR_ACTIONS; ++i)
    {
        float v    = m_output[start + i];
        int   bars = (int)(v * 20.0f);
        bool  win  = (m_lastDecision.action == static_cast<MotorAction>(i));
        std::printf("  [%2d] %-10s ", i, motorActionName(static_cast<MotorAction>(i)));
        for (int b = 0; b < 20; ++b) std::putchar(b < bars ? '#' : '.');
        std::printf(" %.3f %s\n", v, win ? "<< WINNER" : "");
    }
    std::printf("  Reason: %s\n\n", m_lastDecision.reasonLine.c_str());
}
