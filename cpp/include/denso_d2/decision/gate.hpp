#pragma once

#include <unordered_map>
#include <vector>

#include "denso_d2/decision/action.hpp"
#include "denso_d2/decision/constraint.hpp"
#include "denso_d2/decision/factory_state.hpp"
#include "denso_d2/decision/feasibility.hpp"
#include "denso_d2/decision/ids.hpp"

namespace denso_d2::decision {

// Immutable constraint registry. Registration order is preserved for
// evaluation order; duplicate ids throw std::invalid_argument
// (configuration error, not infeasibility).
class ConstraintRegistry {
  public:
    // Adds a constraint. Throws std::invalid_argument on duplicate id.
    void add(Constraint constraint);

    [[nodiscard]] const std::vector<Constraint>& constraints() const noexcept {
        return constraints_;
    }

    // Returns nullptr when the id is absent. The pointer stays valid
    // until the next add().
    [[nodiscard]] const Constraint* find(const ConstraintId& id) const;

  private:
    std::vector<Constraint> constraints_;
    std::unordered_map<ConstraintId, std::size_t> registry_index_;
};

// Evaluates one action against all registered constraints, LaTeX
// eq:feasibleset + eq:verdict. Pure: no I/O, no global state, no
// mutation of inputs. Deterministic: evaluation order follows registry
// insertion order and is part of the observable output.
//
// a_0 rule: state constraints (count bounds, buffer bounds) evaluate
// a_0 for real, so an evidenced violating state is reported even for
// the status quo; change-dependent constraints take the delta=0 branch,
// so unknown data about changes never blocks a_0.
[[nodiscard]] FeasibilityResult evaluate_feasibility(
    const FactoryState& state,
    const Action& action,
    const ConstraintRegistry& registry);

// Batch helper: evaluates a range of actions against the same immutable
// state and registry. Output order matches input order. Semantically
// identical to calling evaluate_feasibility per action.
//
// Evidence scoping: simulated_buffer_content in FactoryState is
// evidence produced for ONE action's simulation run. Do not share an
// evidence-bearing state across multiple actions in batch evaluation:
// evidence for action A would be consumed as evidence for action B.
// Batch callers evaluating evidence-dependent constraints must call
// evaluate_feasibility per action with a per-action state.
[[nodiscard]] std::vector<FeasibilityResult> evaluate_all(
    const FactoryState& state,
    const std::vector<Action>& actions,
    const ConstraintRegistry& registry);

}  // namespace denso_d2::decision
