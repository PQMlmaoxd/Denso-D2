#include "denso_d2/decision/baselines.hpp"

#include <cmath>
#include <stdexcept>

#include "denso_d2/decision/catalog.hpp"

namespace denso_d2::decision {
namespace {

// A simulation outcome must be finite to enter comparison: non-finite KPIs
// would make policy results order-dependent (completeness of the preorder
// breaks), violating determinism. Fail clearly instead.
void require_finite(const SimulationResult& outcome) {
    const auto& k = outcome.kpis;
    if (!std::isfinite(k.throughput) || !std::isfinite(k.lead_time) ||
        !std::isfinite(k.lateness) || !std::isfinite(k.wip) ||
        (k.cost.has_value() && !std::isfinite(*k.cost))) {
        throw std::invalid_argument(
            "simulation outcome contains a non-finite KPI value");
    }
}

// Feasibility of an EvaluatedCandidate per the selection rule.
bool is_feasible(const EvaluatedCandidate& candidate) {
    return candidate.feasibility.status == FeasibilityStatus::Feasible;
}

}  // namespace

EvaluatedCandidate evaluate_no_action(
    const FactoryState& state,
    const ScenarioId& scenario,
    Seed seed,
    const ConstraintRegistry& registry,
    const SimulationEvaluator& evaluator) {
    EvaluatedCandidate candidate;
    candidate.action = ActionCatalog::no_action();
    candidate.feasibility =
        evaluate_feasibility(state, candidate.action, registry);
    // An evidenced status-quo violation makes a_0 infeasible; infeasible
    // candidates are never simulated (same rule as every other action).
    if (candidate.feasibility.status == FeasibilityStatus::Feasible) {
        candidate.simulated = true;
        candidate.outcome =
            evaluator.evaluate(state, candidate.action, scenario, seed);
        require_finite(candidate.outcome);
    }
    return candidate;
}

std::vector<Action> generate_rule_based_candidates(const FactoryState& state) {
    std::vector<Action> candidates;
    candidates.reserve(state.resources.size());
    for (const auto& [resource, info] : state.resources) {
        Action action;
        action.id = ActionId{"add-" + resource.value};
        action.payload = AddTemporaryCapacity{resource, 1};
        action.claim_status = ClaimStatus::Candidate;
        // The generated action is a synthetic rule proposal: it never
        // inherits the resource datum's provenance (a mentor-confirmed
        // count does not make the proposed change mentor-confirmed).
        action.provenance = Provenance{
            SourceType::Synthetic,
            "rule-generated candidate (Baseline 1); needs mentor validation",
            true};
        candidates.push_back(std::move(action));
    }
    return candidates;
}

bool SyntheticLexicographicPreference::at_least_as_preferred(
    const EvaluatedCandidate& a,
    const EvaluatedCandidate& b) const {
    const auto& ka = a.outcome.kpis;
    const auto& kb = b.outcome.kpis;

    if (ka.throughput != kb.throughput) {
        return ka.throughput > kb.throughput;
    }
    if (ka.lead_time != kb.lead_time) {
        return ka.lead_time < kb.lead_time;
    }
    // Compare costs only when both are known. Cost units are UNKNOWN
    // pre-tour, so no known-vs-unknown preference is fabricated; such
    // pairs tie on cost and fall through to the id tie-break.
    if (ka.cost.has_value() && kb.cost.has_value() &&
        *ka.cost != *kb.cost) {
        return *ka.cost < *kb.cost;
    }
    return true;
}

BaselineResult run_greedy_evaluation(
    const FactoryState& state,
    const std::vector<Action>& proposed_actions,
    const ScenarioId& scenario,
    Seed seed,
    const ConstraintRegistry& registry,
    const SimulationEvaluator& evaluator,
    const PreferencePolicy& policy) {
    // Evidence in FactoryState describes one action's simulation run and
    // must not leak across candidates' gate evaluations (see gate.hpp).
    if (!state.simulated_buffer_content.empty()) {
        throw std::invalid_argument(
            "run_greedy_evaluation: state carries simulation evidence; "
            "evaluate candidates individually with per-action states");
    }

    BaselineResult result;

    // Gate every proposed candidate; simulate only fully feasible ones.
    for (const Action& action : proposed_actions) {
        EvaluatedCandidate candidate;
        candidate.action = action;
        candidate.feasibility = evaluate_feasibility(state, action, registry);
        if (candidate.feasibility.status == FeasibilityStatus::Feasible) {
            candidate.simulated = true;
            candidate.outcome =
                evaluator.evaluate(state, action, scenario, seed);
            require_finite(candidate.outcome);
        }
        result.candidates.push_back(std::move(candidate));
    }

    // Baseline 0 reference outcome.
    EvaluatedCandidate no_action = evaluate_no_action(
        state, scenario, seed, registry, evaluator);

    // Selection among feasible candidates: linear max-scan; ties break
    // deterministically by action id (ascending).
    bool have_feasible = false;
    std::size_t best = 0;

    for (std::size_t i = 0; i < result.candidates.size(); ++i) {
        const auto& candidate = result.candidates[i];
        if (!is_feasible(candidate)) {
            continue;
        }
        if (!have_feasible) {
            have_feasible = true;
            best = i;
            continue;
        }
        const auto& current_best = result.candidates[best];
        const bool cand_ge_best =
            policy.at_least_as_preferred(candidate, current_best);
        const bool best_ge_cand =
            policy.at_least_as_preferred(current_best, candidate);
        if (cand_ge_best && !best_ge_cand) {
            best = i;  // strictly better
        } else if (cand_ge_best && best_ge_cand &&
                   candidate.action.id < current_best.action.id) {
            best = i;  // policy tie: deterministic id fallback
        }
    }

    // a_0 is always kept visible as the comparison reference. A candidate
    // is recommended only when strictly preferred over a_0; a tie keeps
    // the status quo (a zero-improvement change is not a recommendation).
    // When a_0 is infeasible, the best feasible candidate is selected
    // without comparison to a_0; when nothing is feasible, the result is
    // NoFeasibleOption with a_0 as the inspectable entry.
    const std::size_t a0_index = result.candidates.size();
    result.candidates.push_back(std::move(no_action));

    if (!have_feasible) {
        // No feasible candidate: the recommendation is the status quo when
        // a_0 itself is feasible; otherwise nothing is feasible at all and
        // the a_0 entry (with its violation) is the inspectable result.
        result.status = is_feasible(no_action)
                            ? SelectionStatus::NoAction
                            : SelectionStatus::NoFeasibleOption;
        result.selected = a0_index;
    } else if (is_feasible(no_action)) {
        const bool a0_ge_best = policy.at_least_as_preferred(
            result.candidates[a0_index], result.candidates[best]);
        const bool best_ge_a0 = policy.at_least_as_preferred(
            result.candidates[best], result.candidates[a0_index]);
        if (a0_ge_best && !best_ge_a0) {
            result.status = SelectionStatus::NoAction;
            result.selected = a0_index;
        } else if (a0_ge_best && best_ge_a0) {
            // Tie with a_0: status quo (no zero-improvement change).
            result.status = SelectionStatus::NoAction;
            result.selected = a0_index;
        } else {
            result.status = SelectionStatus::ActionSelected;
            result.selected = best;
        }
    } else {
        // a_0 infeasible (evidenced status-quo violation): recommend the
        // best feasible candidate; the a_0 entry exposes the violation.
        result.status = SelectionStatus::ActionSelected;
        result.selected = best;
    }
    return result;
}

}  // namespace denso_d2::decision
