#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace denso_d2::decision {

// Scenario identity for one evaluation run. Synthetic until the Factory
// Tour provides real forecast trajectories; never a DENSO fact.
struct ScenarioId {
    std::string value;

    bool operator==(const ScenarioId&) const = default;
    bool operator!=(const ScenarioId& other) const { return !(*this == other); }
};

// Replication seed: makes stochastic evaluation reproducible. The value is
// opaque to the decision layer; the simulation layer owns stream semantics.
using Seed = std::uint64_t;

// KPI outcome of one simulation run. Units are attached by the simulation
// layer's contract and are UNKNOWN pre-tour; the decision layer compares
// only values produced under the same contract.
//
// cost: known-zero-cost (0.0) and unknown-cost (nullopt) are distinct states.
// Unknown cost must never be silently treated as zero.
struct KpiOutcome {
    double throughput = 0.0;
    double lead_time = 0.0;
    double lateness = 0.0;
    double wip = 0.0;
    std::optional<double> cost;
};

// Result of one simulation evaluation of one action under one scenario and
// seed. Minimal boundary: no Digital-Twin internal state crosses it.
struct SimulationResult {
    KpiOutcome kpis;
    ScenarioId scenario;
    Seed seed = 0;
};

}  // namespace denso_d2::decision
