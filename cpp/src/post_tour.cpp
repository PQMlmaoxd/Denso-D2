#include "denso_d2/decision/post_tour.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

#include "denso_d2/decision/catalog.hpp"

namespace denso_d2::decision {
namespace {

constexpr const char* kSyntheticNote =
    "post-tour synthetic candidate; operational validity requires confirmation against actual factory rules/data";

ConstraintEvaluation finding(
    std::string id,
    ConstraintStatus status,
    ConstraintReason reason) {
    return {ConstraintId{std::move(id)}, status, reason, Severity::Hard};
}

FeasibilityResult aggregate(std::vector<ConstraintEvaluation> evaluations) {
    bool violated = false;
    bool unknown = false;
    for (const auto& evaluation : evaluations) {
        violated = violated || evaluation.status == ConstraintStatus::Violated;
        unknown = unknown || evaluation.status == ConstraintStatus::Unknown;
    }
    return {violated ? FeasibilityStatus::Infeasible
                     : unknown ? FeasibilityStatus::Unknown
                               : FeasibilityStatus::Feasible,
            std::move(evaluations)};
}

bool valid_priority(TaskPriority priority) {
    return priority == TaskPriority::Normal || priority == TaskPriority::Urgent;
}

bool valid_amr_status(AmrStatus status) {
    return status == AmrStatus::Available || status == AmrStatus::Busy ||
           status == AmrStatus::Charging || status == AmrStatus::Unavailable;
}

void validate_snapshot(const LogisticsSnapshot& snapshot) {
    if (snapshot.snapshot_id.empty()) {
        throw std::invalid_argument("post-tour snapshot_id must not be empty");
    }
    std::unordered_set<TransportTaskId> task_ids;
    for (const auto& task : snapshot.transport_tasks) {
        if (task.id.value.empty()) {
            throw std::invalid_argument("transport task id must not be empty");
        }
        if (task.pickup.value.empty() || task.destination.value.empty()) {
            throw std::invalid_argument("transport task endpoints must not be empty");
        }
        if (!valid_priority(task.priority)) {
            throw std::invalid_argument("transport task priority is not a valid enum value");
        }
        if (task.assigned_amr.has_value() && task.assigned_amr->value.empty()) {
            throw std::invalid_argument("task assigned_amr must not be empty");
        }
        if (task.current_route.has_value() && task.current_route->value.empty()) {
            throw std::invalid_argument("task current_route must not be empty");
        }
        if (task.required_resource_type.has_value() && task.required_resource_type->empty()) {
            throw std::invalid_argument("task required_resource_type must not be empty");
        }
        if (!task_ids.insert(task.id).second) {
            throw std::invalid_argument("duplicate TransportTaskId: " + task.id.value);
        }
        if (task.quantity <= 0) {
            throw std::invalid_argument("transport task quantity must be positive");
        }
        if (task.release_minute.has_value() && task.due_minute.has_value() &&
            *task.due_minute < *task.release_minute) {
            throw std::invalid_argument("transport task due time precedes release time");
        }
    }
    std::unordered_set<AmrId> amr_ids;
    for (const auto& amr : snapshot.amrs) {
        if (amr.id.value.empty()) {
            throw std::invalid_argument("AMR id must not be empty");
        }
        if (amr.location.value.empty()) {
            throw std::invalid_argument("AMR location must not be empty");
        }
        if (!valid_amr_status(amr.status)) {
            throw std::invalid_argument("AMR status is not a valid enum value");
        }
        if (amr.resource_type.has_value() && amr.resource_type->empty()) {
            throw std::invalid_argument("AMR resource_type must not be empty");
        }
        if (!amr_ids.insert(amr.id).second) {
            throw std::invalid_argument("duplicate AmrId: " + amr.id.value);
        }
        if (amr.battery_percent.has_value() &&
            (*amr.battery_percent < 0 || *amr.battery_percent > 100)) {
            throw std::invalid_argument("AMR battery percent must be in [0, 100]");
        }
        if (amr.payload_capacity.has_value() && *amr.payload_capacity < 0) {
            throw std::invalid_argument("AMR payload capacity must not be negative");
        }
    }
    std::unordered_set<RouteId> route_ids;
    for (const auto& route : snapshot.routes) {
        if (route.id.value.empty()) {
            throw std::invalid_argument("route id must not be empty");
        }
        if (route.origin.value.empty() || route.destination.value.empty()) {
            throw std::invalid_argument("route endpoints must not be empty");
        }
        if (!route_ids.insert(route.id).second) {
            throw std::invalid_argument("duplicate RouteId: " + route.id.value);
        }
        if (route.expected_travel_minutes.has_value() &&
            *route.expected_travel_minutes < 0) {
            throw std::invalid_argument("route travel time must not be negative");
        }
    }
    for (const auto& task : snapshot.transport_tasks) {
        if (task.assigned_amr.has_value() &&
            amr_ids.find(*task.assigned_amr) == amr_ids.end()) {
            throw std::invalid_argument("task assigned_amr is absent from snapshot");
        }
        if (task.current_route.has_value() &&
            route_ids.find(*task.current_route) == route_ids.end()) {
            throw std::invalid_argument("task current_route is absent from snapshot");
        }
        if (task.current_route.has_value()) {
            const auto route = std::find_if(
                snapshot.routes.begin(), snapshot.routes.end(),
                [&task](const auto& candidate) {
                    return candidate.id == *task.current_route;
                });
            if (route->origin != task.pickup || route->destination != task.destination) {
                throw std::invalid_argument("task current_route endpoints do not match task");
            }
        }
    }
}

void validate_rules(const PostTourRules& rules) {
    if (rules.model_version.empty()) {
        throw std::invalid_argument("post-tour model_version must not be empty");
    }
    if (rules.minimum_battery_percent.has_value() &&
        (*rules.minimum_battery_percent < 0 || *rules.minimum_battery_percent > 100)) {
        throw std::invalid_argument("minimum battery percent must be in [0, 100]");
    }
    for (const auto& [id, lead] : rules.action_lead_times_min) {
        (void)id;
        if (lead < 0) {
            throw std::invalid_argument("action lead time must not be negative");
        }
    }
    const auto require_nonnegative = [](const std::optional<int>& value, const char* name) {
        if (value.has_value() && *value < 0) {
            throw std::invalid_argument(std::string{name} + " must not be negative");
        }
    };
    require_nonnegative(rules.maximum_projected_shortage_minutes, "shortage limit");
    require_nonnegative(rules.maximum_projected_task_lateness_minutes, "lateness limit");
    require_nonnegative(rules.maximum_projected_travel_minutes, "travel limit");
}

const TransportTaskSnapshot* find_task(
    const LogisticsSnapshot& snapshot,
    const TransportTaskId& id) {
    const auto it = std::find_if(
        snapshot.transport_tasks.begin(), snapshot.transport_tasks.end(),
        [&id](const auto& task) { return task.id == id; });
    return it == snapshot.transport_tasks.end() ? nullptr : &*it;
}

const AmrSnapshot* find_amr(const LogisticsSnapshot& snapshot, const AmrId& id) {
    const auto it = std::find_if(
        snapshot.amrs.begin(), snapshot.amrs.end(),
        [&id](const auto& amr) { return amr.id == id; });
    return it == snapshot.amrs.end() ? nullptr : &*it;
}

const RouteSnapshot* find_route(
    const LogisticsSnapshot& snapshot,
    const RouteId& id) {
    const auto it = std::find_if(
        snapshot.routes.begin(), snapshot.routes.end(),
        [&id](const auto& route) { return route.id == id; });
    return it == snapshot.routes.end() ? nullptr : &*it;
}

std::string component(const std::string& value) {
    return std::to_string(value.size()) + ":" + value;
}

Action candidate(ActionId id, ActionPayload payload) {
    return {std::move(id), std::move(payload), ClaimStatus::Candidate,
            {SourceType::Synthetic, kSyntheticNote, true}};
}

void validate_action(const Action& action) {
    if (action.id.value.empty()) {
        throw std::invalid_argument("post-tour action id must not be empty");
    }
    if (is_no_action(action)) {
        return;
    }
    if (!is_post_tour_action(action)) {
        throw std::invalid_argument("action is not a post-tour logistics action");
    }
    std::visit(
        [](const auto& payload) {
            using Family = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Family, PrioritizeTransportTask>) {
                if (payload.task.value.empty()) {
                    throw std::invalid_argument("post-tour action task id must not be empty");
                }
            } else if constexpr (std::is_same_v<Family, AssignTransportTask>) {
                if (payload.task.value.empty() || payload.amr.value.empty()) {
                    throw std::invalid_argument("post-tour assignment ids must not be empty");
                }
            } else if constexpr (std::is_same_v<Family, ReassignTransportTask>) {
                if (payload.task.value.empty() || payload.from_amr.value.empty() ||
                    payload.to_amr.value.empty()) {
                    throw std::invalid_argument("post-tour reassignment ids must not be empty");
                }
            } else if constexpr (std::is_same_v<Family, SelectAlternateRoute>) {
                if (payload.task.value.empty() || payload.route.value.empty()) {
                    throw std::invalid_argument("post-tour route action ids must not be empty");
                }
            }
        },
        action.payload);
}

void add_amr_checks(
    std::vector<ConstraintEvaluation>& checks,
    const TransportTaskSnapshot* task,
    const AmrSnapshot* amr,
    const PostTourRules& rules) {
    if (amr == nullptr) {
        checks.push_back(finding("PT-AMR-EXISTS", ConstraintStatus::Unknown,
                                 ConstraintReason::MissingRequiredEvidence));
        return;
    }
    checks.push_back(finding(
        "PT-AMR-AVAILABLE",
        amr->status == AmrStatus::Available ? ConstraintStatus::Satisfied
                                            : ConstraintStatus::Violated,
        amr->status == AmrStatus::Available
            ? ConstraintReason::VerifiedSatisfied
            : ConstraintReason::ViolationProvableFromProposal));

    ConstraintStatus battery = ConstraintStatus::Unknown;
    if (rules.minimum_battery_percent.has_value() && amr->battery_percent.has_value()) {
        battery = *amr->battery_percent >= *rules.minimum_battery_percent
                      ? ConstraintStatus::Satisfied
                      : ConstraintStatus::Violated;
    }
    checks.push_back(finding(
        "PT-AMR-BATTERY", battery,
        battery == ConstraintStatus::Satisfied
            ? ConstraintReason::VerifiedSatisfied
            : battery == ConstraintStatus::Violated
                  ? ConstraintReason::ViolationProvableFromProposal
                  : ConstraintReason::MissingRequiredEvidence));

    const ConstraintStatus payload =
        task != nullptr && amr->payload_capacity.has_value()
            ? (task->quantity <= *amr->payload_capacity ? ConstraintStatus::Satisfied
                                                         : ConstraintStatus::Violated)
            : ConstraintStatus::Unknown;
    checks.push_back(finding(
        "PT-AMR-PAYLOAD", payload,
        payload == ConstraintStatus::Satisfied
            ? ConstraintReason::VerifiedSatisfied
            : payload == ConstraintStatus::Violated
                  ? ConstraintReason::ViolationProvableFromProposal
                  : ConstraintReason::MissingRequiredEvidence));

    ConstraintStatus compatible = ConstraintStatus::Unknown;
    if (task != nullptr && task->required_resource_type.has_value() &&
        amr->resource_type.has_value()) {
        compatible = *task->required_resource_type == *amr->resource_type
                         ? ConstraintStatus::Satisfied
                         : ConstraintStatus::Violated;
    }
    checks.push_back(finding(
        "PT-AMR-COMPATIBILITY", compatible,
        compatible == ConstraintStatus::Satisfied
            ? ConstraintReason::VerifiedSatisfied
            : compatible == ConstraintStatus::Violated
                  ? ConstraintReason::ViolationProvableFromProposal
                  : ConstraintReason::MissingRequiredEvidence));
}

void add_deadline_check(
    std::vector<ConstraintEvaluation>& checks,
    const TransportTaskSnapshot& task,
    const Action& action,
    const PostTourRules& rules) {
    if (!task.due_minute.has_value()) {
        checks.push_back(finding("PT-TASK-DEADLINE", ConstraintStatus::Unknown,
                                 ConstraintReason::MissingRequiredEvidence));
        return;
    }
    // Nonnegative lead times cannot recover a deadline already missed.
    if (rules.decision_minute.has_value() && *rules.decision_minute > *task.due_minute) {
        checks.push_back(finding("PT-TASK-DEADLINE", ConstraintStatus::Violated,
                                 ConstraintReason::ViolationProvableFromProposal));
        return;
    }
    const auto lead = rules.action_lead_times_min.find(action.id);
    if (!rules.decision_minute.has_value() || lead == rules.action_lead_times_min.end()) {
        checks.push_back(finding("PT-TASK-DEADLINE", ConstraintStatus::Unknown,
                                 ConstraintReason::MissingRequiredEvidence));
        return;
    }
    const auto completion_minute =
        static_cast<std::int64_t>(*rules.decision_minute) + lead->second;
    const bool met = completion_minute <= static_cast<std::int64_t>(*task.due_minute);
    checks.push_back(finding(
        "PT-TASK-DEADLINE",
        met ? ConstraintStatus::Satisfied : ConstraintStatus::Violated,
        met ? ConstraintReason::VerifiedSatisfied
            : ConstraintReason::ViolationProvableFromProposal));
}

void add_task_route_check(
    std::vector<ConstraintEvaluation>& checks,
    const LogisticsSnapshot& snapshot,
    const TransportTaskSnapshot& task) {
    bool matching_route = false;
    bool available_route = false;
    for (const auto& route : snapshot.routes) {
        if (route.origin == task.pickup && route.destination == task.destination) {
            matching_route = true;
            if (!task.current_route.has_value() || route.id == *task.current_route) {
                available_route = available_route || route.available;
            }
        }
    }
    const auto status = !matching_route ? ConstraintStatus::Unknown
                                       : available_route ? ConstraintStatus::Satisfied
                                                         : ConstraintStatus::Violated;
    checks.push_back(finding(
        "PT-TASK-ROUTE", status,
        status == ConstraintStatus::Satisfied
            ? ConstraintReason::VerifiedSatisfied
            : status == ConstraintStatus::Violated
                  ? ConstraintReason::ViolationProvableFromProposal
                  : ConstraintReason::MissingRequiredEvidence));
}

void require_finite(const SimulationResult& outcome) {
    const auto& k = outcome.kpis;
    if (!std::isfinite(k.throughput) || !std::isfinite(k.lead_time) ||
        !std::isfinite(k.lateness) || !std::isfinite(k.wip) ||
        (k.cost.has_value() && !std::isfinite(*k.cost))) {
        throw std::invalid_argument("post-tour simulation produced a non-finite KPI");
    }
}

bool finally_feasible(const PostTourEvaluatedCandidate& candidate);

EvaluatedCandidate policy_view(const PostTourEvaluatedCandidate& candidate) {
    EvaluatedCandidate result;
    result.action = candidate.action;
    result.feasibility = *candidate.final_feasibility;
    result.simulated = candidate.simulated;
    result.outcome = candidate.outcome;
    return result;
}

void validate_policy_relation(
    const std::vector<PostTourEvaluatedCandidate>& candidates,
    const PreferencePolicy& policy) {
    std::vector<EvaluatedCandidate> views;
    for (const auto& candidate : candidates) {
        if (finally_feasible(candidate)) {
            views.push_back(policy_view(candidate));
        }
    }
    const std::size_t n = views.size();
    // Evaluate the policy once per ordered pair. The relation is then a
    // plain boolean matrix, so the total-preorder checks below cost no
    // further virtual calls and stay O(n^2) in policy invocations.
    std::vector<std::vector<bool>> at_least(n, std::vector<bool>(n, false));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            at_least[i][j] = policy.at_least_as_preferred(views[i], views[j]);
        }
        if (!at_least[i][i]) {
            throw std::invalid_argument("preference policy is not reflexive");
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (!at_least[i][j] && !at_least[j][i]) {
                throw std::invalid_argument("preference policy is not complete");
            }
            for (std::size_t k = 0; k < n; ++k) {
                if (at_least[i][j] && at_least[j][k] && !at_least[i][k]) {
                    throw std::invalid_argument("preference policy is not transitive");
                }
            }
        }
    }
}

bool finally_feasible(const PostTourEvaluatedCandidate& candidate) {
    return candidate.final_feasibility.has_value() &&
           candidate.final_feasibility->status == FeasibilityStatus::Feasible;
}

bool strictly_preferred(
    const PostTourEvaluatedCandidate& a,
    const PostTourEvaluatedCandidate& b,
    const PreferencePolicy& policy) {
    const auto av = policy_view(a);
    const auto bv = policy_view(b);
    return policy.at_least_as_preferred(av, bv) &&
           !policy.at_least_as_preferred(bv, av);
}

bool tied(
    const PostTourEvaluatedCandidate& a,
    const PostTourEvaluatedCandidate& b,
    const PreferencePolicy& policy) {
    const auto av = policy_view(a);
    const auto bv = policy_view(b);
    return policy.at_least_as_preferred(av, bv) &&
           policy.at_least_as_preferred(bv, av);
}

using PayloadKey = std::tuple<int, std::string, std::string, std::string>;

PayloadKey payload_key(const ActionPayload& payload) {
    return std::visit(
        [](const auto& value) -> PayloadKey {
            using Family = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Family, PrioritizeTransportTask>) {
                return {1, value.task.value, "", ""};
            } else if constexpr (std::is_same_v<Family, AssignTransportTask>) {
                return {2, value.task.value, value.amr.value, ""};
            } else if constexpr (std::is_same_v<Family, ReassignTransportTask>) {
                return {3, value.task.value, value.from_amr.value, value.to_amr.value};
            } else if constexpr (std::is_same_v<Family, SelectAlternateRoute>) {
                return {4, value.task.value, value.route.value, ""};
            } else {
                return {9, "", "", ""};
            }
        },
        payload);
}

std::vector<Action> normalize_actions(const std::vector<Action>& proposed) {
    std::vector<Action> actions{ActionCatalog::no_action()};
    std::unordered_set<ActionId> ids{ActionCatalog::no_action().id};
    std::set<PayloadKey> payloads;
    for (const auto& action : proposed) {
        validate_action(action);
        if (is_no_action(action)) {
            throw std::invalid_argument("post-tour candidate set contains a second NoAction");
        }
        if (!ids.insert(action.id).second) {
            throw std::invalid_argument("duplicate post-tour ActionId: " + action.id.value);
        }
        if (!payloads.insert(payload_key(action.payload)).second) {
            throw std::invalid_argument(
                "duplicate post-tour action payload: the same operation appears twice");
        }
        actions.push_back(action);
    }
    std::sort(actions.begin() + 1, actions.end(), [](const Action& a, const Action& b) {
        const auto ka = payload_key(a.payload);
        const auto kb = payload_key(b.payload);
        return ka != kb ? ka < kb : a.id < b.id;
    });
    return actions;
}

PostTourEvaluatedCandidate evaluate_one(
    const LogisticsSnapshot& snapshot,
    const Action& action,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const PostTourSimulationEvaluator& evaluator) {
    PostTourEvaluatedCandidate candidate_result;
    candidate_result.action = action;
    candidate_result.pre_feasibility =
        evaluate_post_tour_pre_feasibility(snapshot, action, rules);
    if (candidate_result.pre_feasibility.status != FeasibilityStatus::Feasible) {
        return candidate_result;
    }
    PostTourSimulationRun run =
        evaluator.evaluate(snapshot, action, scenario, seed, rules.model_version);
    require_finite(run.outcome);
    if (run.outcome.scenario != scenario || run.outcome.seed != seed) {
        throw std::invalid_argument("post-tour simulation result identity mismatch");
    }
    candidate_result.simulated = true;
    candidate_result.outcome = std::move(run.outcome);
    candidate_result.post_feasibility = evaluate_post_tour_post_feasibility(
        snapshot, action, scenario, seed, rules, run.evidence);
    std::vector<ConstraintEvaluation> combined = candidate_result.pre_feasibility.evaluations;
    combined.insert(combined.end(),
                    candidate_result.post_feasibility->evaluations.begin(),
                    candidate_result.post_feasibility->evaluations.end());
    candidate_result.final_feasibility = aggregate(std::move(combined));
    return candidate_result;
}

PostTourBaselineResult evaluate_all_candidates(
    const LogisticsSnapshot& snapshot,
    const std::vector<Action>& proposed,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const PostTourSimulationEvaluator& evaluator) {
    validate_rules(rules);
    if (scenario.value.empty()) {
        throw std::invalid_argument("post-tour scenario id must not be empty");
    }
    PostTourBaselineResult result;
    for (const auto& action : normalize_actions(proposed)) {
        result.candidates.push_back(
            evaluate_one(snapshot, action, scenario, seed, rules, evaluator));
    }
    return result;
}

void finalize_selection(PostTourBaselineResult& result, const PreferencePolicy& policy) {
    validate_policy_relation(result.candidates, policy);
    const FeasibilityStatus a0_status = result.candidates.front().final_feasibility->status;
    std::optional<std::size_t> best;
    for (std::size_t i = 1; i < result.candidates.size(); ++i) {
        if (!finally_feasible(result.candidates[i])) {
            continue;
        }
        if (!best.has_value() ||
            strictly_preferred(result.candidates[i], result.candidates[*best], policy) ||
            (tied(result.candidates[i], result.candidates[*best], policy) &&
             result.candidates[i].action.id < result.candidates[*best].action.id)) {
            best = i;
        }
    }
    if (!best.has_value()) {
        result.selected = 0;
        result.status = a0_status == FeasibilityStatus::Feasible
                            ? SelectionStatus::NoAction
                            : a0_status == FeasibilityStatus::Unknown
                                  ? SelectionStatus::BaselineEvidenceUnknown
                                  : SelectionStatus::NoFeasibleOption;
    } else if (a0_status == FeasibilityStatus::Unknown) {
        result.selected = 0;
        result.status = SelectionStatus::BaselineEvidenceUnknown;
    } else if (a0_status == FeasibilityStatus::Feasible &&
               !strictly_preferred(result.candidates[*best], result.candidates[0], policy)) {
        result.selected = 0;
        result.status = SelectionStatus::NoAction;
    } else {
        result.selected = *best;
        result.status = SelectionStatus::ActionSelected;
    }
}

}  // namespace

std::vector<Action> generate_post_tour_candidates(const LogisticsSnapshot& snapshot) {
    validate_snapshot(snapshot);
    std::vector<TransportTaskSnapshot> tasks = snapshot.transport_tasks;
    std::vector<AmrSnapshot> amrs = snapshot.amrs;
    std::vector<RouteSnapshot> routes = snapshot.routes;
    std::sort(tasks.begin(), tasks.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(amrs.begin(), amrs.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(routes.begin(), routes.end(), [](const auto& a, const auto& b) { return a.id < b.id; });

    std::vector<Action> actions;
    for (const auto& task : tasks) {
        if (task.priority == TaskPriority::Normal) {
            actions.push_back(candidate(
                {"prioritize|" + component(task.id.value)},
                PrioritizeTransportTask{task.id}));
        }
    }
    for (const auto& task : tasks) {
        if (task.assigned_amr.has_value()) {
            continue;
        }
        for (const auto& amr : amrs) {
            actions.push_back(candidate(
                {"assign|" + component(task.id.value) + "|" + component(amr.id.value)},
                AssignTransportTask{task.id, amr.id}));
        }
    }
    for (const auto& task : tasks) {
        if (!task.assigned_amr.has_value()) {
            continue;
        }
        for (const auto& amr : amrs) {
            if (*task.assigned_amr != amr.id) {
                actions.push_back(candidate(
                    {"reassign|" + component(task.id.value) + "|" +
                     component(task.assigned_amr->value) + "|" + component(amr.id.value)},
                    ReassignTransportTask{task.id, *task.assigned_amr, amr.id}));
            }
        }
    }
    for (const auto& task : tasks) {
        for (const auto& route : routes) {
            if (route.origin == task.pickup && route.destination == task.destination &&
                (!task.current_route.has_value() || *task.current_route != route.id)) {
                actions.push_back(candidate(
                    {"route|" + component(task.id.value) + "|" + component(route.id.value)},
                    SelectAlternateRoute{task.id, route.id}));
            }
        }
    }
    return actions;
}

bool SyntheticPostTourPreference::at_least_as_preferred(
    const EvaluatedCandidate& a,
    const EvaluatedCandidate& b) const {
    const auto& ka = a.outcome.kpis;
    const auto& kb = b.outcome.kpis;
    if (ka.throughput != kb.throughput) return ka.throughput > kb.throughput;
    if (ka.lead_time != kb.lead_time) return ka.lead_time < kb.lead_time;
    if (ka.lateness != kb.lateness) return ka.lateness < kb.lateness;
    if (ka.wip != kb.wip) return ka.wip < kb.wip;
    return true;
}

std::optional<Action> generate_post_tour_priority_rule_candidate(
    const LogisticsSnapshot& snapshot,
    const TransportTaskId& risk_task) {
    validate_snapshot(snapshot);
    if (risk_task.value.empty()) {
        throw std::invalid_argument("risk task id must not be empty");
    }
    const auto* task = find_task(snapshot, risk_task);
    if (task == nullptr || task->priority != TaskPriority::Normal) {
        return std::nullopt;
    }
    return candidate(
        {"prioritize|" + component(task->id.value)},
        PrioritizeTransportTask{task->id});
}

FeasibilityResult evaluate_post_tour_pre_feasibility(
    const LogisticsSnapshot& snapshot,
    const Action& action,
    const PostTourRules& rules) {
    validate_snapshot(snapshot);
    validate_rules(rules);
    validate_action(action);
    if (is_no_action(action)) {
        return aggregate({finding("PT-NO-ACTION", ConstraintStatus::Satisfied,
                                  ConstraintReason::NoActionTakesDeltaZeroBranch)});
    }

    std::vector<ConstraintEvaluation> checks;
    std::visit(
        [&](const auto& payload) {
            using Family = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Family, PrioritizeTransportTask>) {
                const auto* task = find_task(snapshot, payload.task);
                checks.push_back(finding(
                    "PT-TASK-EXISTS", task ? ConstraintStatus::Satisfied : ConstraintStatus::Unknown,
                    task ? ConstraintReason::VerifiedSatisfied
                         : ConstraintReason::MissingRequiredEvidence));
                if (task != nullptr) {
                    checks.push_back(finding(
                        "PT-TASK-PRIORITY-CHANGE",
                        task->priority == TaskPriority::Normal ? ConstraintStatus::Satisfied
                                                               : ConstraintStatus::Violated,
                        task->priority == TaskPriority::Normal
                            ? ConstraintReason::VerifiedSatisfied
                            : ConstraintReason::ViolationProvableFromProposal));
                    add_deadline_check(checks, *task, action, rules);
                }
            } else if constexpr (std::is_same_v<Family, AssignTransportTask>) {
                const auto* task = find_task(snapshot, payload.task);
                checks.push_back(finding(
                    "PT-TASK-EXISTS", task ? ConstraintStatus::Satisfied : ConstraintStatus::Unknown,
                    task ? ConstraintReason::VerifiedSatisfied
                         : ConstraintReason::MissingRequiredEvidence));
                add_amr_checks(checks, task, find_amr(snapshot, payload.amr), rules);
                if (task != nullptr) {
                    checks.push_back(finding(
                        "PT-TASK-UNASSIGNED",
                        !task->assigned_amr.has_value() ? ConstraintStatus::Satisfied
                                                       : ConstraintStatus::Violated,
                        !task->assigned_amr.has_value()
                            ? ConstraintReason::VerifiedSatisfied
                            : ConstraintReason::ViolationProvableFromProposal));
                    add_task_route_check(checks, snapshot, *task);
                    add_deadline_check(checks, *task, action, rules);
                }
            } else if constexpr (std::is_same_v<Family, ReassignTransportTask>) {
                const auto* task = find_task(snapshot, payload.task);
                checks.push_back(finding(
                    "PT-TASK-EXISTS", task ? ConstraintStatus::Satisfied : ConstraintStatus::Unknown,
                    task ? ConstraintReason::VerifiedSatisfied
                         : ConstraintReason::MissingRequiredEvidence));
                const bool distinct = payload.from_amr != payload.to_amr;
                const bool source_matches = task != nullptr && task->assigned_amr.has_value() &&
                                            *task->assigned_amr == payload.from_amr;
                const auto source_status = !distinct
                                               ? ConstraintStatus::Violated
                                               : task == nullptr
                                                     ? ConstraintStatus::Unknown
                                                     : source_matches
                                                           ? ConstraintStatus::Satisfied
                                                           : ConstraintStatus::Violated;
                checks.push_back(finding(
                    "PT-REASSIGN-SOURCE", source_status,
                    source_status == ConstraintStatus::Satisfied
                        ? ConstraintReason::VerifiedSatisfied
                        : source_status == ConstraintStatus::Violated
                              ? ConstraintReason::ViolationProvableFromProposal
                              : ConstraintReason::MissingRequiredEvidence));
                add_amr_checks(checks, task, find_amr(snapshot, payload.to_amr), rules);
                if (task != nullptr) {
                    add_task_route_check(checks, snapshot, *task);
                    add_deadline_check(checks, *task, action, rules);
                }
            } else if constexpr (std::is_same_v<Family, SelectAlternateRoute>) {
                const auto* task = find_task(snapshot, payload.task);
                const auto* route = find_route(snapshot, payload.route);
                checks.push_back(finding(
                    "PT-TASK-EXISTS", task ? ConstraintStatus::Satisfied : ConstraintStatus::Unknown,
                    task ? ConstraintReason::VerifiedSatisfied
                         : ConstraintReason::MissingRequiredEvidence));
                checks.push_back(finding(
                    "PT-ROUTE-EXISTS", route ? ConstraintStatus::Satisfied : ConstraintStatus::Unknown,
                    route ? ConstraintReason::VerifiedSatisfied
                          : ConstraintReason::MissingRequiredEvidence));
                if (route != nullptr) {
                    checks.push_back(finding(
                        "PT-ROUTE-AVAILABLE",
                        route->available ? ConstraintStatus::Satisfied : ConstraintStatus::Violated,
                        route->available ? ConstraintReason::VerifiedSatisfied
                                         : ConstraintReason::ViolationProvableFromProposal));
                }
                if (task != nullptr && route != nullptr) {
                    const bool endpoints = route->origin == task->pickup &&
                                           route->destination == task->destination;
                    checks.push_back(finding(
                        "PT-ROUTE-ENDPOINTS",
                        endpoints ? ConstraintStatus::Satisfied : ConstraintStatus::Violated,
                        endpoints ? ConstraintReason::VerifiedSatisfied
                                  : ConstraintReason::ViolationProvableFromProposal));
                    const auto alternate = !task->current_route.has_value()
                                               ? ConstraintStatus::Unknown
                                               : *task->current_route != route->id
                                                     ? ConstraintStatus::Satisfied
                                                     : ConstraintStatus::Violated;
                    checks.push_back(finding(
                        "PT-ROUTE-ALTERNATE", alternate,
                        alternate == ConstraintStatus::Satisfied
                            ? ConstraintReason::VerifiedSatisfied
                            : alternate == ConstraintStatus::Violated
                                  ? ConstraintReason::ViolationProvableFromProposal
                                  : ConstraintReason::MissingRequiredEvidence));
                }
                if (task != nullptr) {
                    add_deadline_check(checks, *task, action, rules);
                }
            } else if constexpr (!std::is_same_v<Family, NoAction>) {
                throw std::invalid_argument("action is not a post-tour logistics action");
            }
        },
        action.payload);
    return aggregate(std::move(checks));
}

FeasibilityResult evaluate_post_tour_post_feasibility(
    const LogisticsSnapshot& snapshot,
    const Action& action,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const ActionSimulationEvidence& evidence) {
    validate_snapshot(snapshot);
    validate_rules(rules);
    validate_action(action);
    if (scenario.value.empty()) {
        throw std::invalid_argument("post-tour scenario id must not be empty");
    }
    const ActionEvidenceIdentity expected{
        snapshot.snapshot_id, snapshot.version, action.id, action.payload,
        scenario, seed, rules.model_version};
    if (evidence.identity != expected) {
        throw std::invalid_argument("post-tour action evidence identity mismatch");
    }
    const auto reject_negative = [](const std::optional<int>& value) {
        if (value.has_value() && *value < 0) {
            throw std::invalid_argument(
                "post-tour projected duration must not be negative");
        }
    };
    reject_negative(evidence.projected_shortage_minutes);
    reject_negative(evidence.projected_task_lateness_minutes);
    reject_negative(evidence.projected_travel_minutes);

    std::vector<ConstraintEvaluation> checks;
    const auto check_metric = [&](const char* id, const std::optional<int>& value,
                                  const std::optional<int>& maximum) {
        if (!maximum.has_value()) {
            return;
        }
        if (!value.has_value()) {
            checks.push_back(finding(id, ConstraintStatus::Unknown,
                                     ConstraintReason::MissingRequiredEvidence));
        } else if (*value > *maximum) {
            checks.push_back(finding(id, ConstraintStatus::Violated,
                                     ConstraintReason::SimulatorEvidenceSupplied));
        } else {
            checks.push_back(finding(id, ConstraintStatus::Satisfied,
                                     ConstraintReason::SimulatorEvidenceSupplied));
        }
    };
    check_metric("PT-POST-SHORTAGE", evidence.projected_shortage_minutes,
                 rules.maximum_projected_shortage_minutes);
    check_metric("PT-POST-LATENESS", evidence.projected_task_lateness_minutes,
                 rules.maximum_projected_task_lateness_minutes);
    check_metric("PT-POST-TRAVEL", evidence.projected_travel_minutes,
                 rules.maximum_projected_travel_minutes);
    if (checks.empty()) {
        checks.push_back(finding("PT-POST-EVIDENCE", ConstraintStatus::Satisfied,
                                 ConstraintReason::SimulatorEvidenceSupplied));
    }
    return aggregate(std::move(checks));
}

PostTourBaselineResult run_post_tour_greedy_evaluation(
    const LogisticsSnapshot& snapshot,
    const std::vector<Action>& proposed_actions,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const PostTourSimulationEvaluator& evaluator,
    const PreferencePolicy& policy) {
    auto result = evaluate_all_candidates(
        snapshot, proposed_actions, scenario, seed, rules, evaluator);
    finalize_selection(result, policy);
    return result;
}

PostTourBaselineResult exhaustive_post_tour_oracle(
    const LogisticsSnapshot& snapshot,
    const std::vector<Action>& proposed_actions,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const PostTourSimulationEvaluator& evaluator,
    const PreferencePolicy& policy) {
    auto result = evaluate_all_candidates(
        snapshot, proposed_actions, scenario, seed, rules, evaluator);
    validate_policy_relation(result.candidates, policy);
    const FeasibilityStatus a0_status = result.candidates[0].final_feasibility->status;
    std::vector<std::size_t> maximal;
    for (std::size_t i = 1; i < result.candidates.size(); ++i) {
        if (!finally_feasible(result.candidates[i])) {
            continue;
        }
        bool beaten = false;
        for (std::size_t j = 1; j < result.candidates.size(); ++j) {
            if (i != j && finally_feasible(result.candidates[j]) &&
                strictly_preferred(result.candidates[j], result.candidates[i], policy)) {
                beaten = true;
                break;
            }
        }
        if (!beaten) {
            maximal.push_back(i);
        }
    }
    if (maximal.empty()) {
        result.selected = 0;
        result.status = a0_status == FeasibilityStatus::Feasible
                            ? SelectionStatus::NoAction
                            : a0_status == FeasibilityStatus::Unknown
                                  ? SelectionStatus::BaselineEvidenceUnknown
                                  : SelectionStatus::NoFeasibleOption;
        return result;
    }
    const auto best = *std::min_element(
        maximal.begin(), maximal.end(), [&](std::size_t a, std::size_t b) {
            return result.candidates[a].action.id < result.candidates[b].action.id;
        });
    if (a0_status == FeasibilityStatus::Unknown) {
        result.selected = 0;
        result.status = SelectionStatus::BaselineEvidenceUnknown;
    } else if (a0_status == FeasibilityStatus::Feasible &&
               !strictly_preferred(result.candidates[best], result.candidates[0], policy)) {
        result.selected = 0;
        result.status = SelectionStatus::NoAction;
    } else {
        result.selected = best;
        result.status = SelectionStatus::ActionSelected;
    }
    return result;
}

}  // namespace denso_d2::decision
