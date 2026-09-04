#pragma once

#include "denso_d2/decision/action.hpp"
#include "denso_d2/decision/factory_state.hpp"
#include "denso_d2/decision/simulation_result.hpp"

namespace denso_d2::decision {

// Boundary to the simulation layer. The decision layer never implements
// physical evolution; it only consumes results through this interface.
//
// Runtime polymorphism is chosen deliberately: a deterministic test double
// ships in tests/ and the simulation owner's Digital-Twin adapter will be
// the production implementation, implementations are selected once at
// assembly time (not in templates), and the virtual call cost is
// irrelevant next to simulation runtime. A template parameter would leak
// the evaluation type into every caller and buy nothing at current scale.
class SimulationEvaluator {
  public:
    virtual ~SimulationEvaluator() = default;

    // Evaluates one action under one scenario and seed. Implementations
    // must be deterministic for identical (state, action, scenario, seed).
    [[nodiscard]] virtual SimulationResult evaluate(
        const FactoryState& state,
        const Action& action,
        const ScenarioId& scenario,
        Seed seed) const = 0;
};

}  // namespace denso_d2::decision
