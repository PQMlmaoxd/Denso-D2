#include <algorithm>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "denso_d2/decision/gate.hpp"

namespace denso_d2::decision {

namespace {

// Builds one evaluation carrying the constraint's severity, so that
// downstream aggregation can honor the hard/soft distinction (eq:verdict
// quantifies over the hard set only).
ConstraintEvaluation make_eval(
    const Constraint& constraint,
    ConstraintStatus status,
    ConstraintReason reason) {
    return ConstraintEvaluation{constraint.id, status, reason, constraint.severity};
}

// Whether a constraint is relevant to the action at all. Derived from
// payload targets and action family; never from hidden global rules.
bool applies_to_action(const Constraint& constraint, const Action& action) {
    const ConstraintPayload& payload = constraint.payload;

    // Cross-cutting constraint kinds: budget (C-BUS-001) and response
    // deadline (C-OPS-007) bind every change-carrying action regardless
    // of family or target.
    if (std::holds_alternative<BudgetBound>(payload) ||
        std::holds_alternative<ResponseDeadline>(payload)) {
        return true;
    }

    return std::visit(
        [&constraint, &payload](const auto& concrete) -> bool {
            using Family = std::decay_t<decltype(concrete)>;

            if constexpr (std::is_same_v<Family, NoAction>) {
                // a_0 takes the delta=0 branch of every change-dependent
                // constraint; applicability is the registry flag.
                return constraint.evaluate_for_no_action;
            } else if constexpr (std::is_same_v<Family, ReallocateResource>) {
                const auto* bound = std::get_if<ResourceCountBound>(&payload);
                return bound != nullptr && bound->resource == concrete.resource;
            } else if constexpr (std::is_same_v<Family, AddTemporaryCapacity>) {
                const auto* bound = std::get_if<ResourceCountBound>(&payload);
                return bound != nullptr && bound->resource == concrete.resource;
            } else if constexpr (std::is_same_v<Family, AdjustBuffer>) {
                const auto* bound = std::get_if<BufferCapacityBound>(&payload);
                return bound != nullptr && bound->buffer == concrete.buffer;
            } else if constexpr (std::is_same_v<Family, ChangeDispatchPriority>) {
                return std::holds_alternative<DispatchCatalogOnly>(payload);
            } else if constexpr (std::is_same_v<Family, ChangeRoute>) {
                return std::holds_alternative<RouteKnown>(payload);
            } else if constexpr (std::is_same_v<Family, ChangeReplenishmentInterval>) {
                const auto* bound = std::get_if<ReplenishmentIntervalBound>(&payload);
                return bound != nullptr && bound->material == concrete.material;
            }
        },
        action.payload);
}

// Evaluates a resource-count bound (C-PHY-002: 0 <= c_r + delta <= c_bar).
// For AddTemporaryCapacity the change is +additional_units; for
// ReallocateResource the total count is unchanged, so the bound reduces
// to the current count. For a_0 the bound reduces to the current count
// as well (delta=0): with the state known and already violating the
// bound, the violation is real and reported (eq:phistatus requires
// verified evaluability, which known count + known ceiling provides);
// with the ceiling unknown the status quo is not blocked (unknown data
// about changes never blocks the status quo).
ConstraintEvaluation evaluate_resource_count(
    const Constraint& constraint,
    const ResourceCountBound& bound,
    const FactoryState& state,
    const Action& action) {
    const auto resource_it = state.resources.find(bound.resource);
    if (resource_it == state.resources.end()) {
        if (is_no_action(action)) {
            // Current count unknown; a_0 changes nothing and cannot be
            // blocked by missing change evidence.
            return make_eval(
                constraint, ConstraintStatus::Satisfied,
                ConstraintReason::NoActionTakesDeltaZeroBranch);
        }
        // Current count unknown: cannot evaluate either branch.
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const FactoryState::ResourceInfo& info = resource_it->second;

    int proposed_count = info.current_count;
    if (const auto* add = std::get_if<AddTemporaryCapacity>(&action.payload)) {
        if (add->additional_units < 0) {
            // This family only adds units; a negative request is a
            // domain-invalid proposal, provable from the proposal alone.
            return make_eval(
                constraint, ConstraintStatus::Violated,
                ConstraintReason::ViolationProvableFromProposal);
        }
        proposed_count = info.current_count + add->additional_units;
    }

    if (proposed_count < 0) {
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    if (info.max_count.has_value() && proposed_count > *info.max_count) {
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    if (!info.max_count.has_value()) {
        if (is_no_action(action) || std::holds_alternative<ReallocateResource>(action.payload)) {
            // Pool ceiling unknown, but these actions do not raise the
            // count: the delta=0 branch cannot be blocked by missing
            // data about a change that does not happen.
            return make_eval(
                constraint, ConstraintStatus::Satisfied,
                ConstraintReason::NoActionTakesDeltaZeroBranch);
        }
        // Pool ceiling unknown: satisfaction cannot be verified (the
        // proposed count might exceed an unknown ceiling).
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    return make_eval(
        constraint, ConstraintStatus::Satisfied,
        ConstraintReason::VerifiedSatisfied);
}

// Evaluates a buffer-capacity bound (C-PHY-003, simulator-enforced).
// For a_0 the status quo does not change any buffer: without evidence it
// takes the delta=0 branch (satisfied by construction); with evidence a
// real state violation (overflow already present) is still reported.
// For change-carrying actions, violation is provable from the proposal
// alone when the new capacity would be negative; otherwise satisfaction
// requires simulation evidence of the resulting content. Applicability
// (applies_to_action) already restricts this evaluator to AdjustBuffer
// and NoAction, so no other action family reaches it.
ConstraintEvaluation evaluate_buffer_capacity(
    const Constraint& constraint,
    const BufferCapacityBound& bound,
    const FactoryState& state,
    const Action& action) {
    const auto capacity_it = state.buffer_capacities.find(bound.buffer);
    if (capacity_it == state.buffer_capacities.end()) {
        if (is_no_action(action)) {
            // Capacity unknown, but a_0 changes nothing: the delta=0
            // branch cannot be blocked by missing change evidence.
            return make_eval(
                constraint, ConstraintStatus::Satisfied,
                ConstraintReason::NoActionTakesDeltaZeroBranch);
        }
        // Capacity unknown: even the proposal cannot be checked against
        // a bound that does not exist.
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const int current_capacity = capacity_it->second;

    if (is_no_action(action)) {
        // a_0: only a real, evidenced state violation can disqualify
        // the status quo (LaTeX sec:generic: unknown data about changes
        // never blocks the status quo).
        const auto evidence_it = state.simulated_buffer_content.find(bound.buffer);
        if (evidence_it != state.simulated_buffer_content.end()) {
            if (evidence_it->second > current_capacity) {
                return make_eval(
                    constraint, ConstraintStatus::Violated,
                    ConstraintReason::SimulatorEvidenceSupplied);
            }
            return make_eval(
                constraint, ConstraintStatus::Satisfied,
                ConstraintReason::SimulatorEvidenceSupplied);
        }
        return make_eval(
            constraint, ConstraintStatus::Satisfied,
            ConstraintReason::NoActionTakesDeltaZeroBranch);
    }

    // The only change family reaching this evaluator is AdjustBuffer.
    const auto* adjust = std::get_if<AdjustBuffer>(&action.payload);
    if (adjust == nullptr) {
        // Defensive guard: applicability is established in a different
        // function; a null payload here means the applicability matrix
        // and this evaluator drifted apart (see review P2-12).
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const int proposed_capacity = current_capacity + adjust->delta_units;
    if (proposed_capacity < 0) {
        // Violation provable from the proposal alone (LaTeX static
        // pre-check paragraph).
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    // With simulation evidence: content after the change must fit.
    const auto evidence_it = state.simulated_buffer_content.find(bound.buffer);
    if (evidence_it != state.simulated_buffer_content.end()) {
        if (evidence_it->second > proposed_capacity) {
            return make_eval(
                constraint, ConstraintStatus::Violated,
                ConstraintReason::SimulatorEvidenceSupplied);
        }
        return make_eval(
            constraint, ConstraintStatus::Satisfied,
            ConstraintReason::SimulatorEvidenceSupplied);
    }
    // No simulation evidence: a simulator-enforced constraint cannot
    // be verified satisfied (only a violation or a held invariant
    // with evidence can).
    return make_eval(
        constraint, ConstraintStatus::Unknown,
        ConstraintReason::MissingRequiredEvidence);
}

// C-OPS-003 dispatch restrictions: the requested rule must be in the
// known allowed set. An empty map means the allowed set is unknown.
// An absent entry in a non-empty map is a documentation gap, not a
// confirmed prohibition: a route may be recorded false (known
// inadmissible) or simply not recorded yet (unknown) — only the
// recorded false verdict is a violation (eq:unknownrule).
ConstraintEvaluation evaluate_dispatch_catalog(
    const Constraint& constraint,
    const FactoryState& state,
    const Action& action) {
    const auto* change = std::get_if<ChangeDispatchPriority>(&action.payload);
    if (change == nullptr) {
        // Defensive guard: applicability is established in a different
        // function; a null payload here means the applicability matrix
        // and this evaluator drifted apart.
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    if (state.allowed_dispatch_rules.empty()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const auto rule_it = state.allowed_dispatch_rules.find(change->rule);
    if (rule_it == state.allowed_dispatch_rules.end()) {
        // Rule absent from a non-empty record: unknown, not forbidden.
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    if (!rule_it->second) {
        // Rule recorded as inadmissible: violation provable from the
        // proposal plus the record.
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    return make_eval(
        constraint, ConstraintStatus::Satisfied,
        ConstraintReason::VerifiedSatisfied);
}

// C-PHY-005 route connectivity: the route must be known-admissible.
// Same completeness contract as dispatch: absent from a non-empty map
// is unknown; recorded false is a violation.
ConstraintEvaluation evaluate_route_known(
    const Constraint& constraint,
    const RouteKnown& bound,
    const FactoryState& state) {
    if (state.known_routes.empty()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const auto route_it = state.known_routes.find(bound.route);
    if (route_it == state.known_routes.end()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    if (!route_it->second) {
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    return make_eval(
        constraint, ConstraintStatus::Satisfied,
        ConstraintReason::VerifiedSatisfied);
}

// C-OPS-004 replenishment bounds: interval must lie in [min, max].
ConstraintEvaluation evaluate_replenishment(
    const Constraint& constraint,
    const ReplenishmentIntervalBound& bound,
    const FactoryState& state,
    const Action& action) {
    const auto* change = std::get_if<ChangeReplenishmentInterval>(&action.payload);
    if (change == nullptr) {
        // Defensive guard: applicability matrix drift (see dispatch).
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const auto bounds_it = state.replenishment_bounds.find(bound.material);
    if (bounds_it == state.replenishment_bounds.end()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const FactoryState::IntervalBound& bounds = bounds_it->second;
    if (change->interval_min < bounds.min_min) {
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    if (bounds.max_min.has_value() && change->interval_min > *bounds.max_min) {
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    if (!bounds.max_min.has_value()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    return make_eval(
        constraint, ConstraintStatus::Satisfied,
        ConstraintReason::VerifiedSatisfied);
}

// C-OPS-007 response deadline: known lead time + known deadline are both
// required to verify; unknown lead time => Unknown.
ConstraintEvaluation evaluate_response_deadline(
    const Constraint& constraint,
    const FactoryState& state,
    const Action& action) {
    if (!state.response_deadline_min.has_value()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    const auto lead_it = state.action_lead_times_min.find(action.id);
    if (lead_it == state.action_lead_times_min.end()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::MissingRequiredEvidence);
    }
    if (lead_it->second > *state.response_deadline_min) {
        return make_eval(
            constraint, ConstraintStatus::Violated,
            ConstraintReason::ViolationProvableFromProposal);
    }
    return make_eval(
        constraint, ConstraintStatus::Satisfied,
        ConstraintReason::VerifiedSatisfied);
}

// C-BUS-001 budget: requires a known budget AND a known action cost.
// The current action model deliberately carries no cost (unit UNKNOWN
// pre-tour), so a budget constraint evaluates to Unknown for any
// change-carrying action; it never fabricates a zero cost (T9).
ConstraintEvaluation evaluate_budget(
    const Constraint& constraint,
    const FactoryState& state,
    const Action& action) {
    if (is_no_action(action)) {
        // a_0 spends nothing: the delta=0 branch is satisfied by
        // construction.
        return make_eval(
            constraint, ConstraintStatus::Satisfied,
            ConstraintReason::NoActionTakesDeltaZeroBranch);
    }
    if (!state.budget.has_value()) {
        return make_eval(
            constraint, ConstraintStatus::Unknown,
            ConstraintReason::PolicyRequiresUnknownValue);
    }
    // Budget known but action cost unknown (no cost field exists
    // pre-tour): cannot verify.
    return make_eval(
        constraint, ConstraintStatus::Unknown,
        ConstraintReason::MissingRequiredEvidence);
}

// Evaluates one constraint against (state, action). a_0 handling per
// constraint kind: STATE constraints (resource-count bound, buffer
// bound) route a_0 through the real evaluator, because a known
// violating state is a real violation even for the status quo
// (eq:phistatus); CHANGE constraints (dispatch catalog, route choice,
// replenishment interval, response deadline, budget) take the delta=0
// branch for a_0: they are properties of the change, and a change that
// does not happen cannot violate them.
ConstraintEvaluation evaluate_constraint(
    const Constraint& constraint,
    const FactoryState& state,
    const Action& action) {
    // a_0 short-circuit only when the constraint explicitly excludes
    // no-action evaluation: the constraint is not evaluated for a_0
    // (a change-dependent bound that the status quo cannot violate).
    if (is_no_action(action) && !constraint.evaluate_for_no_action) {
        return make_eval(
            constraint, ConstraintStatus::Satisfied,
            ConstraintReason::NotApplicableToActionFamily);
    }

    if (!applies_to_action(constraint, action) && !is_no_action(action)) {
        return make_eval(
            constraint, ConstraintStatus::Satisfied,
            ConstraintReason::NotApplicableToActionFamily);
    }

    return std::visit(
        [&constraint, &state, &action](const auto& payload_instance)
            -> ConstraintEvaluation {
            using Payload = std::decay_t<decltype(payload_instance)>;

            if constexpr (std::is_same_v<Payload, ResourceCountBound>) {
                // State constraint: a_0 is evaluated for real (a known
                // over-ceiling count is a violation even for a_0).
                return evaluate_resource_count(constraint, payload_instance, state, action);
            } else if constexpr (std::is_same_v<Payload, BufferCapacityBound>) {
                // State constraint with evidence semantics: a_0 is
                // evaluated inside (evidenced overflow reported, no
                // evidence -> delta=0 satisfied).
                return evaluate_buffer_capacity(constraint, payload_instance, state, action);
            } else if constexpr (std::is_same_v<Payload, DispatchCatalogOnly>) {
                if (is_no_action(action)) {
                    return make_eval(
                        constraint, ConstraintStatus::Satisfied,
                        ConstraintReason::NoActionTakesDeltaZeroBranch);
                }
                return evaluate_dispatch_catalog(constraint, state, action);
            } else if constexpr (std::is_same_v<Payload, RouteKnown>) {
                if (is_no_action(action)) {
                    return make_eval(
                        constraint, ConstraintStatus::Satisfied,
                        ConstraintReason::NoActionTakesDeltaZeroBranch);
                }
                return evaluate_route_known(constraint, payload_instance, state);
            } else if constexpr (std::is_same_v<Payload, ReplenishmentIntervalBound>) {
                if (is_no_action(action)) {
                    return make_eval(
                        constraint, ConstraintStatus::Satisfied,
                        ConstraintReason::NoActionTakesDeltaZeroBranch);
                }
                return evaluate_replenishment(constraint, payload_instance, state, action);
            } else if constexpr (std::is_same_v<Payload, BudgetBound>) {
                // a_0 handled inside evaluate_budget (spends nothing).
                return evaluate_budget(constraint, state, action);
            } else if constexpr (std::is_same_v<Payload, ResponseDeadline>) {
                if (is_no_action(action)) {
                    return make_eval(
                        constraint, ConstraintStatus::Satisfied,
                        ConstraintReason::NoActionTakesDeltaZeroBranch);
                }
                return evaluate_response_deadline(constraint, state, action);
            }
        },
        constraint.payload);
}

}  // namespace

FeasibilityResult evaluate_feasibility(
    const FactoryState& state,
    const Action& action,
    const ConstraintRegistry& registry) {
    FeasibilityResult result;
    result.evaluations.reserve(registry.constraints().size());

    bool any_hard_violated = false;
    bool any_hard_unknown = false;

    for (const Constraint& constraint : registry.constraints()) {
        ConstraintEvaluation evaluation = evaluate_constraint(constraint, state, action);
        if (evaluation.status == ConstraintStatus::Violated) {
            // Only a violated HARD constraint disqualifies the action
            // (eq:verdict: INFEASIBLE iff some hard constraint is
            // violated). Soft violations stay visible in the evaluation
            // list as flagged reasons but never change the verdict.
            if (constraint.severity == Severity::Soft) {
                evaluation.reason = ConstraintReason::SoftViolationFlagged;
            } else {
                any_hard_violated = true;
            }
        } else if (evaluation.status == ConstraintStatus::Unknown) {
            // eq:verdict quantifies over the hard set only: a soft
            // constraint with unverifiable data must not block the
            // action either; it stays visible as a flagged evaluation.
            if (constraint.severity == Severity::Hard) {
                any_hard_unknown = true;
            }
        }
        result.evaluations.push_back(evaluation);
    }

    // Aggregation per eq:verdict: violation dominates; then unknown.
    if (any_hard_violated) {
        result.status = FeasibilityStatus::Infeasible;
    } else if (any_hard_unknown) {
        result.status = FeasibilityStatus::Unknown;
    } else {
        result.status = FeasibilityStatus::Feasible;
    }
    return result;
}

std::vector<FeasibilityResult> evaluate_all(
    const FactoryState& state,
    const std::vector<Action>& actions,
    const ConstraintRegistry& registry) {
    std::vector<FeasibilityResult> results;
    results.reserve(actions.size());
    for (const Action& action : actions) {
        results.push_back(evaluate_feasibility(state, action, registry));
    }
    return results;
}

}  // namespace denso_d2::decision
