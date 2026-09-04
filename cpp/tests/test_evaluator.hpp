#include "denso_d2/decision/baselines.hpp"

#include <unordered_map>
#include <variant>
#include <vector>

#include "denso_d2/decision/ids.hpp"
#include "denso_d2/decision/simulation_result.hpp"

namespace denso_d2::decision {
namespace {

// Deterministic synthetic evaluator for tests. Produces outcomes that
// depend only on the action's effect kind, so expected winners are
// hand-computable. Records every call for verification. The mutable call
// log makes concurrent evaluation unsafe; acceptable in a test double.
// The default curve gives a_0 {100, 30} and +1-capacity candidates
// strictly-better outcomes; use `tie_with_no_action` to force a candidate
// to tie with a_0 (selection-rule tests), and `known_costs` to attach
// known costs (cost-semantics tests).
class RecordingTestEvaluator final : public SimulationEvaluator {
  public:
    struct Call {
        ActionId action;
        ScenarioId scenario;
        Seed seed;
    };

    // Map from action id to a forced tie with the a_0 outcome.
    std::vector<ActionId> tie_with_no_action;
    // Map from action id to a known cost (overrides the unknown default).
    std::unordered_map<ActionId, double> known_costs;

    [[nodiscard]] SimulationResult evaluate(
        const FactoryState& state,
        const Action& action,
        const ScenarioId& scenario,
        Seed seed) const override {
        calls_.push_back(Call{action.id, scenario, seed});
        SimulationResult result;
        result.scenario = scenario;
        result.seed = seed;
        bool tie = false;
        for (const auto& id : tie_with_no_action) {
            if (id == action.id) {
                tie = true;
            }
        }
        if (!tie && std::holds_alternative<AddTemporaryCapacity>(
                        action.payload)) {
            const auto& add =
                std::get<AddTemporaryCapacity>(action.payload);
            const auto it = state.resources.find(add.resource);
            if (it != state.resources.end()) {
                const int count =
                    it->second.current_count + add.additional_units;
                // Synthetic response curve: throughput grows with count,
                // lead time shrinks, cost is unknown (no unit pre-tour).
                result.kpis.throughput = 100.0 + 5.0 * count;
                result.kpis.lead_time = 30.0 - count;
            }
        } else {
            // NoAction, forced ties, and other change kinds: the a_0
            // reference outcome.
            result.kpis.throughput = 100.0;
            result.kpis.lead_time = 30.0;
        }
        result.kpis.cost = std::nullopt;
        const auto cost_it = known_costs.find(action.id);
        if (cost_it != known_costs.end()) {
            result.kpis.cost = cost_it->second;
        }
        return result;
    }

    [[nodiscard]] const std::vector<Call>& calls() const noexcept {
        return calls_;
    }

  private:
    mutable std::vector<Call> calls_;
};

}  // namespace
}  // namespace denso_d2::decision
