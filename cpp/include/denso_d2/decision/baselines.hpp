#pragma once

#include <vector>

#include "denso_d2/decision/action.hpp"
#include "denso_d2/decision/evaluator.hpp"
#include "denso_d2/decision/factory_state.hpp"
#include "denso_d2/decision/feasibility.hpp"
#include "denso_d2/decision/gate.hpp"
#include "denso_d2/decision/simulation_result.hpp"

namespace denso_d2::decision {

// One evaluated candidate: the action, its feasibility verdict, and (when
// simulated) its KPI outcome. Only FEASIBLE candidates are ever simulated;
// the outcome field is meaningful only when `simulated` is true.
struct EvaluatedCandidate {
    Action action;
    FeasibilityResult feasibility;
    bool simulated = false;
    SimulationResult outcome{};
};

// Preference policy over evaluated candidates. Synthetic-only until the
// Factory Tour fixes the real KPI hierarchy; any concrete policy must be
// explicitly labeled as synthetic/test-oriented. The policy orders two
// simulated candidates; ties are broken deterministically by action id
// (ascending) so the same inputs always produce the same recommendation.
class PreferencePolicy {
  public:
    virtual ~PreferencePolicy() = default;

    // Returns true when candidate `a` is at least as preferred as `b`.
    // Implementations must be a total preorder over finite outcomes and
    // must not depend on anything outside the two candidates.
    // (run_greedy_evaluation rejects non-finite KPI outcomes at intake, so
    // policies may assume finite values.)
    [[nodiscard]] virtual bool at_least_as_preferred(
        const EvaluatedCandidate& a,
        const EvaluatedCandidate& b) const = 0;
};

// A synthetic lexicographic test policy (EXPLICITLY NOT a DENSO objective):
// maximize throughput, then minimize lead time, then compare known costs
// numerically. When either cost is unknown the pair ties on cost — no
// preference between known and unknown cost is fabricated (cost units are
// UNKNOWN pre-tour, so the tie falls through to the id tie-break).
// Its existence must not be read as a statement about DENSO priorities.
class SyntheticLexicographicPreference final : public PreferencePolicy {
  public:
    [[nodiscard]] bool at_least_as_preferred(
        const EvaluatedCandidate& a,
        const EvaluatedCandidate& b) const override;
};

// Baseline 0: evaluate the no-action candidate a_0 through the normal
// pipeline (feasibility gate + same simulation boundary). Produces the
// reference outcome every other candidate is compared against. a_0 is
// simulated only when the gate verdict is FEASIBLE (an evidenced
// status-quo violation makes a_0 infeasible, per the decision model
// report's evidence-aware a_0 rule).
[[nodiscard]] EvaluatedCandidate evaluate_no_action(
    const FactoryState& state,
    const ScenarioId& scenario,
    Seed seed,
    const ConstraintRegistry& registry,
    const SimulationEvaluator& evaluator);

// Baseline 1: generic rule-based candidate generation. Rules are synthetic
// and structural only: for each resource that exists in the state, propose
// one AddTemporaryCapacity action (+1 unit). Generated actions carry their
// own synthetic provenance (never the state datum's) and require mentor
// validation. No DENSO-specific triggers; richer rules arrive only with
// tour-confirmed action semantics.
[[nodiscard]] std::vector<Action> generate_rule_based_candidates(
    const FactoryState& state);

// What the Baseline 2 run selected.
enum class SelectionStatus {
    ActionSelected,   // a feasible candidate strictly beats a_0
    NoAction,         // no feasible candidate strictly beats a_0 (status quo)
    NoFeasibleOption,  // neither a_0 nor any candidate is FEASIBLE
    BaselineEvidenceUnknown,  // a_0 cannot support a valid comparison
};

// Baseline 2: greedy exhaustive evaluation over a candidate set.
// Pipeline: gate every candidate -> INFEASIBLE are excluded (never
// simulated) -> UNKNOWN are excluded from simulation and reported as
// unknown (never silently treated as eligible) -> FEASIBLE candidates are
// simulated under the shared scenario/seed -> a_0 is evaluated the same
// way and kept visible as the comparison reference.
//
// Selection rule (pre-tour default, pending tour confirmation): a
// candidate is recommended only when it is STRICTLY preferred over a_0
// under the policy — a zero-improvement change is not a recommendation,
// so ties with a_0 keep the status quo. When a_0 itself is not FEASIBLE
// (evidenced status-quo violation), feasible candidates compete among
// themselves and the best is selected. When nothing is FEASIBLE the
// result is NoFeasibleOption and `selected` points at the a_0 entry so
// the violation is inspectable.
//
// The state must not carry simulation evidence (simulated_buffer_content):
// evidence describes one action's simulation run and cannot be shared
// across candidates; evidence-bearing states are rejected.
struct BaselineResult {
    std::vector<EvaluatedCandidate> candidates;
    std::size_t selected = 0;  // index into candidates
    SelectionStatus status = SelectionStatus::NoAction;
};

[[nodiscard]] BaselineResult run_greedy_evaluation(
    const FactoryState& state,
    const std::vector<Action>& proposed_actions,
    const ScenarioId& scenario,
    Seed seed,
    const ConstraintRegistry& registry,
    const SimulationEvaluator& evaluator,
    const PreferencePolicy& policy);

}  // namespace denso_d2::decision
