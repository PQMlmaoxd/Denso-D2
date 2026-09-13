#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "denso_d2/decision/post_tour.hpp"

namespace {

using namespace denso_d2::decision;

int checks_run = 0;
int checks_failed = 0;

void check(bool condition, const std::string& message) {
    ++checks_run;
    if (!condition) {
        ++checks_failed;
        std::cout << "FAIL: " << message << "\n";
    }
}

template <typename T, typename U>
void check_eq(const T& actual, const U& expected, const std::string& message) {
    check(actual == expected, message);
}

LogisticsSnapshot snapshot() {
    return {
        "synthetic-snapshot",
        7,
        {{TransportTaskId{"task-b"}, LocationId{"pick"}, LocationId{"drop"}, 4,
          0, 30, TaskPriority::Normal, std::nullopt, std::string{"tote"},
          RouteId{"route-b"}},
         {TransportTaskId{"task-a"}, LocationId{"pick"}, LocationId{"drop"}, 3,
          0, 30, TaskPriority::Urgent, AmrId{"amr-a"}, std::string{"tote"},
          RouteId{"route-b"}}},
        {{AmrId{"amr-b"}, LocationId{"pick"}, AmrStatus::Available, 80, 8,
          std::string{"tote"}},
         {AmrId{"amr-a"}, LocationId{"pick"}, AmrStatus::Available, 70, 8,
          std::string{"tote"}}},
        {{RouteId{"route-b"}, LocationId{"pick"}, LocationId{"drop"}, 8, true},
         {RouteId{"route-a"}, LocationId{"pick"}, LocationId{"drop"}, 6, true}}};
}

PostTourRules rules() {
    PostTourRules value;
    value.minimum_battery_percent = 20;
    value.decision_minute = 0;
    value.maximum_projected_shortage_minutes = 0;
    value.maximum_projected_task_lateness_minutes = 0;
    value.model_version = "synthetic-model-v1";
    return value;
}

Action priority_action() {
    return {ActionId{"priority"}, PrioritizeTransportTask{TransportTaskId{"task-b"}},
            ClaimStatus::Candidate,
            {SourceType::Synthetic, "synthetic test action", true}};
}

Action route_action() {
    return {ActionId{"route"},
            SelectAlternateRoute{TransportTaskId{"task-b"}, RouteId{"route-a"}},
            ClaimStatus::Candidate,
            {SourceType::Synthetic, "synthetic test action", true}};
}

class SyntheticEvaluator final : public PostTourSimulationEvaluator {
  public:
    mutable std::vector<ActionId> calls;
    std::unordered_map<ActionId, double> throughputs;
    std::unordered_map<ActionId, int> shortages;
    std::unordered_map<ActionId, double> costs;
    std::unordered_set<ActionId> missing_shortages;

    PostTourSimulationRun evaluate(
        const LogisticsSnapshot& state,
        const Action& action,
        const ScenarioId& scenario,
        Seed seed,
        const std::string& model_version) const override {
        calls.push_back(action.id);
        const auto throughput = throughputs.find(action.id);
        const auto shortage = shortages.find(action.id);
        const auto cost = costs.find(action.id);
        return {
            {{throughput == throughputs.end() ? 1.0 : throughput->second,
              10.0, 0.0, 1.0,
              cost == costs.end() ? std::nullopt : std::optional<double>{cost->second}},
             scenario, seed},
            {{state.snapshot_id, state.version, action.id, action.payload,
              scenario, seed, model_version},
             missing_shortages.contains(action.id)
                 ? std::nullopt
                 : shortage == shortages.end() ? std::optional<int>{0}
                                               : std::optional<int>{shortage->second},
             0,
             5}};
    }
};

void add_lead(PostTourRules& value, const Action& action) {
    value.action_lead_times_min[action.id] = 5;
}

bool has_finding(
    const FeasibilityResult& result,
    const std::string& id,
    ConstraintStatus status) {
    return std::any_of(
        result.evaluations.begin(), result.evaluations.end(),
        [&](const auto& finding) {
            return finding.constraint_id.value == id && finding.status == status;
        });
}

void test_generation_is_canonical_and_permutation_invariant() {
    const auto first = generate_post_tour_candidates(snapshot());
    auto permuted = snapshot();
    std::reverse(permuted.transport_tasks.begin(), permuted.transport_tasks.end());
    std::reverse(permuted.amrs.begin(), permuted.amrs.end());
    std::reverse(permuted.routes.begin(), permuted.routes.end());
    const auto second = generate_post_tour_candidates(permuted);

    check_eq(first.size(), std::size_t{6}, "generator emits complete single-action catalog");
    check_eq(first.size(), second.size(), "permutation preserves catalog size");
    for (std::size_t i = 0; i < first.size(); ++i) {
        check_eq(first[i].id, second[i].id, "permutation preserves candidate order and ids");
        check(first[i].provenance.needs_mentor_validation,
              "generated action remains an unconfirmed proposal");
    }
    check(std::holds_alternative<PrioritizeTransportTask>(first[0].payload),
          "family order starts with priority");
    check(std::holds_alternative<AssignTransportTask>(first[1].payload),
          "assignment follows priority");
    check(std::holds_alternative<ReassignTransportTask>(first[3].payload),
          "reassignment follows assignment");
    check(std::holds_alternative<SelectAlternateRoute>(first[4].payload),
          "route selection is last family");

    const auto rule = generate_post_tour_priority_rule_candidate(
        snapshot(), TransportTaskId{"task-b"});
    check(rule.has_value() && std::holds_alternative<PrioritizeTransportTask>(rule->payload),
          "Baseline 1 maps an upstream risk task to one priority action");
    check(!generate_post_tour_priority_rule_candidate(
               snapshot(), TransportTaskId{"missing"}).has_value(),
          "Baseline 1 does not invent a missing task");

    auto duplicate = snapshot();
    duplicate.amrs.push_back(duplicate.amrs.front());
    bool threw = false;
    try {
        (void)generate_post_tour_candidates(duplicate);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "duplicate snapshot entity ids are configuration errors");

    auto dangling = snapshot();
    dangling.transport_tasks[1].assigned_amr = AmrId{"missing-amr"};
    threw = false;
    try {
        (void)generate_post_tour_candidates(dangling);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "dangling task assignment is rejected at the snapshot boundary");
}

void test_pre_gate_preserves_unknown_and_violation_priority() {
    auto state = snapshot();
    state.amrs[0].status = AmrStatus::Unavailable;
    state.amrs[0].battery_percent = std::nullopt;
    Action assign{ActionId{"assign-b"},
                  AssignTransportTask{TransportTaskId{"task-b"}, AmrId{"amr-b"}},
                  ClaimStatus::Candidate,
                  {SourceType::Synthetic, "synthetic", true}};
    auto config = rules();
    add_lead(config, assign);
    const auto result = evaluate_post_tour_pre_feasibility(state, assign, config);
    check_eq(result.status, FeasibilityStatus::Infeasible,
             "verified hard violation dominates missing battery evidence");
    check(result.has_unknown(), "unknown finding remains inspectable");

    state.amrs[0].status = AmrStatus::Available;
    const auto unknown = evaluate_post_tour_pre_feasibility(state, assign, config);
    check_eq(unknown.status, FeasibilityStatus::Unknown,
             "missing required battery evidence is not treated as zero or feasible");

    auto unavailable_route = snapshot();
    unavailable_route.routes[0].available = false;
    const auto blocked_route = evaluate_post_tour_pre_feasibility(
        unavailable_route, assign, config);
    check_eq(blocked_route.status, FeasibilityStatus::Infeasible,
             "assignment cannot silently replace an unavailable current route");

    auto overflow_state = snapshot();
    overflow_state.transport_tasks[0].due_minute = std::numeric_limits<int>::max();
    auto overflow_rules = rules();
    overflow_rules.decision_minute = std::numeric_limits<int>::max();
    overflow_rules.action_lead_times_min[assign.id] = 1;
    const auto overflow_safe = evaluate_post_tour_pre_feasibility(
        overflow_state, assign, overflow_rules);
    check_eq(overflow_safe.status, FeasibilityStatus::Infeasible,
             "deadline arithmetic does not overflow at integer limits");
}

void test_missing_task_preserves_independent_hard_violations() {
    auto state = snapshot();
    state.amrs[0].status = AmrStatus::Unavailable;
    Action assign{ActionId{"assign-missing"},
                  AssignTransportTask{TransportTaskId{"missing"}, AmrId{"amr-b"}},
                  ClaimStatus::Candidate,
                  {SourceType::Synthetic, "synthetic", true}};
    auto config = rules();
    add_lead(config, assign);
    const auto unavailable = evaluate_post_tour_pre_feasibility(state, assign, config);
    check_eq(unavailable.status, FeasibilityStatus::Infeasible,
             "known unavailable AMR dominates missing task evidence");
    check(has_finding(unavailable, "PT-TASK-EXISTS", ConstraintStatus::Unknown),
          "missing task remains explicit beside the AMR violation");
    check(has_finding(unavailable, "PT-AMR-AVAILABLE", ConstraintStatus::Violated),
          "AMR availability is evaluated independently of task evidence");

    state.amrs[0].status = AmrStatus::Available;
    Action self_reassign{
        ActionId{"self-reassign"},
        ReassignTransportTask{TransportTaskId{"missing"}, AmrId{"amr-b"}, AmrId{"amr-b"}},
        ClaimStatus::Candidate,
        {SourceType::Synthetic, "synthetic", true}};
    add_lead(config, self_reassign);
    const auto self = evaluate_post_tour_pre_feasibility(state, self_reassign, config);
    check_eq(self.status, FeasibilityStatus::Infeasible,
             "self-reassignment is infeasible even when task evidence is missing");
    check(has_finding(self, "PT-REASSIGN-SOURCE", ConstraintStatus::Violated),
          "proposal-provable reassignment violation remains inspectable");
    check(has_finding(self, "PT-TASK-EXISTS", ConstraintStatus::Unknown),
          "self-reassignment preserves missing task evidence");
}

void test_deadline_violations_survive_missing_unrelated_evidence() {
    const auto state = snapshot();
    auto config = rules();
    auto route = route_action();
    route.payload = SelectAlternateRoute{TransportTaskId{"task-b"}, RouteId{"missing"}};
    config.action_lead_times_min[route.id] = 31;
    const auto missing_route = evaluate_post_tour_pre_feasibility(state, route, config);
    check_eq(missing_route.status, FeasibilityStatus::Infeasible,
             "missing route cannot mask a known deadline violation");
    check(has_finding(missing_route, "PT-ROUTE-EXISTS", ConstraintStatus::Unknown) &&
              has_finding(missing_route, "PT-TASK-DEADLINE", ConstraintStatus::Violated),
          "route unknown and deadline violation remain inspectable together");

    const auto priority = priority_action();
    config.decision_minute = 31;
    const auto expired = evaluate_post_tour_pre_feasibility(state, priority, config);
    check_eq(expired.status, FeasibilityStatus::Infeasible,
             "expired deadline is violated without exact lead time");
    config.decision_minute = 30;
    const auto uncertain = evaluate_post_tour_pre_feasibility(state, priority, config);
    check_eq(uncertain.status, FeasibilityStatus::Unknown,
             "deadline equality with missing lead time remains unknown");
    config.action_lead_times_min[priority.id] = 0;
    check_eq(evaluate_post_tour_pre_feasibility(state, priority, config).status,
             FeasibilityStatus::Feasible, "zero lead time satisfies exact deadline");

    bool threw = false;
    try {
        (void)generate_post_tour_priority_rule_candidate(state, TransportTaskId{""});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "priority rule rejects malformed risk task identity");
}

void test_selectors_agree_with_competition_ties_and_infeasible_baseline() {
    const auto state = snapshot();
    const auto priority = priority_action();
    const auto route = route_action();
    auto config = rules();
    add_lead(config, priority);
    add_lead(config, route);
    SyntheticPostTourPreference policy;
    for (int scenario = 0; scenario < 4; ++scenario) {
        SyntheticEvaluator evaluator;
        evaluator.throughputs[priority.id] = 3.0;
        evaluator.throughputs[route.id] = scenario == 0 ? 4.0 : 3.0;
        if (scenario >= 2) {
            evaluator.throughputs[ActionId{"no-action"}] = 10.0;
            evaluator.shortages[ActionId{"no-action"}] = 1;
        }
        if (scenario == 3) {
            evaluator.shortages[priority.id] = 1;
            evaluator.shortages[route.id] = 1;
        }
        const auto production = run_post_tour_greedy_evaluation(
            state, {route, priority}, ScenarioId{"synthetic"}, 8, config, evaluator, policy);
        const auto oracle = exhaustive_post_tour_oracle(
            state, {priority, route}, ScenarioId{"synthetic"}, 8, config, evaluator, policy);
        const auto expected = scenario == 0 ? route.id
                              : scenario == 3 ? ActionId{"no-action"} : priority.id;
        check_eq(production.candidates[production.selected].action.id, expected,
                 "selector respects preference, id ties and baseline infeasibility");
        check_eq(production.status, scenario == 3 ? SelectionStatus::NoFeasibleOption
                                                 : SelectionStatus::ActionSelected,
                 "selection status distinguishes rescue from no feasible option");
        check_eq(oracle.status, production.status, "oracle agrees on edge-case status");
        check_eq(oracle.candidates[oracle.selected].action.id, expected,
                 "oracle agrees on edge-case winner");
    }
}

void test_alternate_route_requires_a_valid_current_route() {
    const auto state = snapshot();
    Action current{ActionId{"current-route"},
                   SelectAlternateRoute{TransportTaskId{"task-b"}, RouteId{"route-b"}},
                   ClaimStatus::Candidate,
                   {SourceType::Synthetic, "synthetic", true}};
    auto config = rules();
    add_lead(config, current);
    const auto no_op = evaluate_post_tour_pre_feasibility(state, current, config);
    check_eq(no_op.status, FeasibilityStatus::Infeasible,
             "selecting the current route is not an alternate-route action");

    auto missing = state;
    missing.transport_tasks[0].current_route = std::nullopt;
    const auto unknown = evaluate_post_tour_pre_feasibility(missing, current, config);
    check_eq(unknown.status, FeasibilityStatus::Unknown,
             "missing current-route evidence cannot prove an alternate route");

    auto invalid = state;
    invalid.routes[0].origin = LocationId{"other"};
    bool threw = false;
    try {
        (void)generate_post_tour_candidates(invalid);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "current-route endpoints must match the task endpoints");
}

void test_post_gate_rejects_cross_action_and_missing_evidence() {
    const auto state = snapshot();
    auto config = rules();
    const auto action = priority_action();
    add_lead(config, action);
    ActionSimulationEvidence evidence{
        {state.snapshot_id, state.version, ActionId{"other"}, action.payload,
         ScenarioId{"scenario"}, 4, config.model_version},
        0, 0, 5};
    bool threw = false;
    try {
        (void)evaluate_post_tour_post_feasibility(
            state, action, ScenarioId{"scenario"}, 4, config, evidence);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "evidence from another action is rejected");

    evidence.identity.action_id = action.id;
    evidence.identity.action_payload =
        PrioritizeTransportTask{TransportTaskId{"task-a"}};
    threw = false;
    try {
        (void)evaluate_post_tour_post_feasibility(
            state, action, ScenarioId{"scenario"}, 4, config, evidence);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "same-id evidence for a different payload is rejected");

    evidence.identity.action_payload = action.payload;
    evidence.projected_shortage_minutes = std::nullopt;
    const auto unknown = evaluate_post_tour_post_feasibility(
        state, action, ScenarioId{"scenario"}, 4, config, evidence);
    check_eq(unknown.status, FeasibilityStatus::Unknown,
             "missing required post-simulation evidence remains unknown");

    evidence.projected_shortage_minutes = -1;
    threw = false;
    try {
        (void)evaluate_post_tour_post_feasibility(
            state, action, ScenarioId{"scenario"}, 4, config, evidence);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "negative projected durations are rejected at the evidence boundary");
}

void test_post_tour_boundaries_reject_missing_and_unsupported_identity() {
    const auto state = snapshot();
    auto config = rules();
    SyntheticEvaluator evaluator;
    SyntheticPostTourPreference policy;

    auto empty_action = priority_action();
    empty_action.id = ActionId{""};
    bool threw = false;
    try {
        (void)run_post_tour_greedy_evaluation(
            state, {empty_action}, ScenarioId{"scenario"}, 4, config, evaluator, policy);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "empty action id is rejected before simulation");
    check(evaluator.calls.empty(), "invalid action identity does not reach evaluator");

    threw = false;
    try {
        (void)run_post_tour_greedy_evaluation(
            state, {}, ScenarioId{""}, 4, config, evaluator, policy);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "empty scenario id is rejected before simulation");
    check(evaluator.calls.empty(), "invalid scenario identity does not reach evaluator");

    Action generic{ActionId{"generic"}, AdjustBuffer{LocationId{"buffer"}, 1},
                   ClaimStatus::Candidate,
                   {SourceType::Synthetic, "synthetic", true}};
    ActionSimulationEvidence evidence{
        {state.snapshot_id, state.version, generic.id, generic.payload,
         ScenarioId{"scenario"}, 4, config.model_version},
        0, 0, 5};
    threw = false;
    try {
        (void)evaluate_post_tour_post_feasibility(
            state, generic, ScenarioId{"scenario"}, 4, config, evidence);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "public post-gate rejects generic action families");
}

void test_pipeline_and_oracle_agree() {
    const auto state = snapshot();
    const auto priority = priority_action();
    const auto route = route_action();
    auto config = rules();
    add_lead(config, priority);
    add_lead(config, route);
    SyntheticPostTourPreference policy;

    SyntheticEvaluator production_evaluator;
    production_evaluator.throughputs[ActionId{"no-action"}] = 1.0;
    production_evaluator.throughputs[priority.id] = 3.0;
    production_evaluator.throughputs[route.id] = 4.0;
    production_evaluator.shortages[route.id] = 2;
    const auto result = run_post_tour_greedy_evaluation(
        state, {route, priority}, ScenarioId{"scenario"}, 9, config,
        production_evaluator, policy);

    check_eq(result.candidates.size(), std::size_t{3}, "Baseline 0 remains visible");
    check(is_no_action(result.candidates[0].action), "Baseline 0 is first");
    check_eq(result.status, SelectionStatus::ActionSelected,
             "pipeline selects a strictly improving final-feasible action");
    check_eq(result.candidates[result.selected].action.id, priority.id,
             "post-infeasible higher-KPI action cannot be selected");
    check(result.candidates[result.selected].final_feasibility.has_value() &&
              result.candidates[result.selected].final_feasibility->evaluations.size() >
                  result.candidates[result.selected].post_feasibility->evaluations.size(),
          "final verdict preserves both static and dynamic findings");

    SyntheticEvaluator oracle_evaluator;
    oracle_evaluator.throughputs = production_evaluator.throughputs;
    oracle_evaluator.shortages = production_evaluator.shortages;
    const auto oracle = exhaustive_post_tour_oracle(
        state, {priority, route}, ScenarioId{"scenario"}, 9, config,
        oracle_evaluator, policy);
    check_eq(oracle.status, result.status, "exact oracle agrees on status");
    check_eq(oracle.candidates[oracle.selected].action.id,
             result.candidates[result.selected].action.id,
             "exact oracle agrees on winner");
}

void test_pre_unknown_is_never_simulated_and_tie_keeps_no_action() {
    const auto state = snapshot();
    const auto priority = priority_action();
    auto config = rules();
    SyntheticPostTourPreference policy;
    SyntheticEvaluator evaluator;
    const auto rejected = run_post_tour_greedy_evaluation(
        state, {priority}, ScenarioId{"scenario"}, 2, config, evaluator, policy);
    check_eq(evaluator.calls.size(), std::size_t{1},
             "only no-action is simulated when a change has static unknown evidence");
    check_eq(rejected.candidates[1].pre_feasibility.status, FeasibilityStatus::Unknown,
             "pre-gate unknown remains inspectable");

    add_lead(config, priority);
    SyntheticEvaluator tied_evaluator;
    const auto tied_result = run_post_tour_greedy_evaluation(
        state, {priority}, ScenarioId{"scenario"}, 2, config, tied_evaluator, policy);
    check_eq(tied_result.status, SelectionStatus::NoAction,
             "policy tie retains status quo");
    check_eq(tied_result.selected, std::size_t{0}, "tie selects canonical no-action");
}

void test_unknown_no_action_and_invalid_policy_cannot_recommend() {
    const auto state = snapshot();
    const auto priority = priority_action();
    const auto route = route_action();
    auto config = rules();
    add_lead(config, priority);
    add_lead(config, route);
    SyntheticPostTourPreference policy;

    SyntheticEvaluator unknown_a0;
    unknown_a0.missing_shortages.insert(ActionId{"no-action"});
    unknown_a0.throughputs[priority.id] = 3.0;
    const auto blocked = run_post_tour_greedy_evaluation(
        state, {priority}, ScenarioId{"scenario"}, 5, config, unknown_a0, policy);
    check_eq(blocked.status, SelectionStatus::BaselineEvidenceUnknown,
             "unknown no-action counterfactual blocks recommendation");
    check_eq(blocked.selected, std::size_t{0},
             "unknown no-action remains the inspectable selection");

    SyntheticEvaluator non_transitive;
    non_transitive.costs[priority.id] = 2.0;
    non_transitive.costs[route.id] = 1.0;
    bool threw = false;
    try {
        (void)run_post_tour_greedy_evaluation(
            state, {priority, route}, ScenarioId{"scenario"}, 5,
            config, non_transitive, SyntheticLexicographicPreference{});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "non-transitive policy relation is rejected before recommendation");

    threw = false;
    try {
        (void)exhaustive_post_tour_oracle(
            state, {priority, route}, ScenarioId{"scenario"}, 5,
            config, non_transitive, SyntheticLexicographicPreference{});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "oracle rejects the same invalid policy relation as production");
}

void test_generic_gate_rejects_post_tour_payload() {
    bool threw = false;
    try {
        (void)evaluate_feasibility(FactoryState{}, priority_action(), ConstraintRegistry{});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "generic gate cannot silently admit a post-tour logistics action");
}

void test_duplicate_payloads_and_invalid_snapshot_values_are_rejected() {
    auto config = rules();
    const auto priority = priority_action();
    add_lead(config, priority);
    auto duplicate_payload = priority_action();
    duplicate_payload.id = ActionId{"priority-copy"};

    SyntheticEvaluator evaluator;
    SyntheticPostTourPreference policy;
    bool threw = false;
    try {
        (void)run_post_tour_greedy_evaluation(
            snapshot(), {priority, duplicate_payload}, ScenarioId{"scenario"}, 3,
            config, evaluator, policy);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "the same operation under two ids is a duplicate candidate");

    auto empty_id = snapshot();
    empty_id.transport_tasks[0].id = TransportTaskId{""};
    threw = false;
    try {
        (void)generate_post_tour_candidates(empty_id);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "empty task identifier is rejected at the snapshot boundary");

    auto invalid_priority = snapshot();
    invalid_priority.transport_tasks[1].priority = static_cast<TaskPriority>(99);
    threw = false;
    try {
        (void)generate_post_tour_candidates(invalid_priority);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "out-of-range TaskPriority is rejected at the snapshot boundary");

    auto invalid_status = snapshot();
    invalid_status.amrs[0].status = static_cast<AmrStatus>(42);
    threw = false;
    try {
        (void)generate_post_tour_candidates(invalid_status);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "out-of-range AmrStatus is rejected at the snapshot boundary");
}

void test_oracle_reports_unknown_baseline_and_policy_ignores_cost() {
    const auto state = snapshot();
    const auto priority = priority_action();
    auto config = rules();
    add_lead(config, priority);
    SyntheticPostTourPreference policy;

    SyntheticEvaluator evaluator;
    evaluator.missing_shortages.insert(ActionId{"no-action"});
    evaluator.throughputs[priority.id] = 3.0;
    const auto oracle = exhaustive_post_tour_oracle(
        state, {priority}, ScenarioId{"scenario"}, 6, config, evaluator, policy);
    check_eq(oracle.status, SelectionStatus::BaselineEvidenceUnknown,
             "oracle mirrors unknown-baseline blocking");
    check_eq(oracle.selected, std::size_t{0},
             "oracle keeps unknown no-action as the inspectable selection");

    EvaluatedCandidate expensive;
    EvaluatedCandidate cheaper;
    expensive.outcome.kpis = {1.0, 2.0, 0.0, 0.0, 5.0};
    cheaper.outcome.kpis = {1.0, 2.0, 0.0, 0.0, 1.0};
    check(policy.at_least_as_preferred(expensive, cheaper) &&
              policy.at_least_as_preferred(cheaper, expensive),
          "synthetic post-tour policy does not fabricate a cost preference");
}

}  // namespace

int main() {
    test_generation_is_canonical_and_permutation_invariant();
    test_pre_gate_preserves_unknown_and_violation_priority();
    test_missing_task_preserves_independent_hard_violations();
    test_deadline_violations_survive_missing_unrelated_evidence();
    test_selectors_agree_with_competition_ties_and_infeasible_baseline();
    test_alternate_route_requires_a_valid_current_route();
    test_post_gate_rejects_cross_action_and_missing_evidence();
    test_post_tour_boundaries_reject_missing_and_unsupported_identity();
    test_pipeline_and_oracle_agree();
    test_pre_unknown_is_never_simulated_and_tie_keeps_no_action();
    test_unknown_no_action_and_invalid_policy_cannot_recommend();
    test_generic_gate_rejects_post_tour_payload();
    test_duplicate_payloads_and_invalid_snapshot_values_are_rejected();
    test_oracle_reports_unknown_baseline_and_policy_ignores_cost();

    if (checks_failed != 0) {
        std::cout << checks_failed << " of " << checks_run << " checks failed\n";
        return 1;
    }
    std::cout << checks_run << " checks passed\n";
    return 0;
}
