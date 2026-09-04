#include "test_evaluator.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "denso_d2/decision/baselines.hpp"
#include "denso_d2/decision/catalog.hpp"
#include "denso_d2/decision/constraint.hpp"
#include "denso_d2/decision/factory_state.hpp"
#include "denso_d2/decision/feasibility.hpp"
#include "denso_d2/decision/gate.hpp"
#include "denso_d2/decision/ids.hpp"
#include "denso_d2/decision/provenance.hpp"
#include "denso_d2/decision/simulation_result.hpp"

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
    ++checks_run;
    if (!(actual == expected)) {
        ++checks_failed;
        std::cout << "FAIL: " << message << "\n";
    }
}

// --- Fixtures (SYNTHETIC values, matching decision_tests.cpp) ---

Provenance synthetic_provenance() {
    return Provenance{SourceType::Synthetic, "synthetic fixture", false};
}

FactoryState synthetic_state() {
    FactoryState state;
    state.resources[ResourceId{"transporter"}] = {2, 3, synthetic_provenance()};
    state.resources[ResourceId{"agv"}] = {1, 2, synthetic_provenance()};
    state.allowed_dispatch_rules["bottleneck-first"] = true;
    state.allowed_dispatch_rules["fifo"] = true;
    state.known_routes[RouteId{"route_main"}] = true;
    state.known_routes[RouteId{"route_alt"}] = true;
    state.replenishment_bounds[MaterialId{"part_x"}] = {5, 60};
    state.action_lead_times_min[ActionId{"add-transporter"}] = 10;
    return state;
}

Constraint make_resource_count_constraint(const ResourceId& resource) {
    return Constraint{ConstraintId{"C-PHY-002-" + resource.value},
                      ConstraintCategory::Physical,
                      ConstraintCheckLayer::Static,
                      Severity::Hard,
                      ResourceCountBound{resource},
                      synthetic_provenance(),
                      true};
}

Constraint make_dispatch_constraint() {
    return Constraint{ConstraintId{"C-OPS-003"},
                      ConstraintCategory::Operational,
                      ConstraintCheckLayer::Static,
                      Severity::Hard,
                      DispatchCatalogOnly{},
                      synthetic_provenance(),
                      true};
}

Action make_add_transporter(int units) {
    Action action;
    action.id = ActionId{"add-transporter"};
    action.payload = AddTemporaryCapacity{ResourceId{"transporter"}, units};
    action.claim_status = ClaimStatus::Candidate;
    action.provenance = synthetic_provenance();
    return action;
}

Action make_add_agv(int units) {
    Action action;
    action.id = ActionId{"add-agv"};
    action.payload = AddTemporaryCapacity{ResourceId{"agv"}, units};
    action.claim_status = ClaimStatus::Candidate;
    action.provenance = synthetic_provenance();
    return action;
}

ConstraintRegistry default_registry() {
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    registry.add(make_resource_count_constraint(ResourceId{"agv"}));
    registry.add(make_dispatch_constraint());
    return registry;
}

// --- Tests ---

void test_baseline0_no_action_evaluated_normally() {
    RecordingTestEvaluator evaluator;
    const FactoryState state = synthetic_state();
    const auto result = evaluate_no_action(
        state, ScenarioId{"synthetic"}, 42, default_registry(), evaluator);

    check_eq(result.action.id, ActionId{"no-action"}, "a_0 id");
    check_eq(result.feasibility.status, FeasibilityStatus::Feasible,
             "a_0 feasible (change-dependent constraints take delta=0)");
    check(result.simulated, "a_0 must be simulated through the boundary");
    check_eq(evaluator.calls().size(), std::size_t{1},
             "a_0 evaluated exactly once");
    check_eq(evaluator.calls()[0].action, ActionId{"no-action"},
             "a_0 call recorded");
    check_eq(evaluator.calls()[0].seed, Seed{42}, "seed passed through");
    check_eq(result.outcome.kpis.throughput, 100.0,
             "a_0 reference throughput (SYNTHETIC)");
}

void test_baseline0_infeasible_a0_not_simulated() {
    // Evidenced buffer overflow: the status quo itself violates a hard
    // constraint, so a_0 is INFEASIBLE and must not be simulated
    // (evidence-aware a_0 rule; same treatment as every other action).
    RecordingTestEvaluator evaluator;
    FactoryState state = synthetic_state();
    state.buffer_capacities[LocationId{"buffer_a"}] = 20;
    state.simulated_buffer_content[LocationId{"buffer_a"}] = 25;
    ConstraintRegistry registry;
    registry.add(Constraint{ConstraintId{"C-PHY-003-buffer_a"},
                            ConstraintCategory::Physical,
                            ConstraintCheckLayer::SimulatorEnforced,
                            Severity::Hard,
                            BufferCapacityBound{LocationId{"buffer_a"}},
                            synthetic_provenance(),
                            true});

    const auto a0 = evaluate_no_action(
        state, ScenarioId{"synthetic"}, 42, registry, evaluator);

    check_eq(a0.feasibility.status, FeasibilityStatus::Infeasible,
             "evidenced overflow makes a_0 infeasible");
    check(!a0.simulated, "infeasible a_0 never simulated");
    check_eq(evaluator.calls().size(), std::size_t{0},
             "no simulation call for infeasible a_0");
}

void test_baseline1_rule_generation() {
    const FactoryState state = synthetic_state();
    const auto candidates = generate_rule_based_candidates(state);

    check_eq(candidates.size(), std::size_t{2},
             "one candidate per known resource");
    bool has_transporter = false;
    bool has_agv = false;
    for (const auto& action : candidates) {
        const auto* add = std::get_if<AddTemporaryCapacity>(&action.payload);
        check(add != nullptr, "rule candidates add capacity");
        // Rule-generated actions carry their OWN synthetic provenance and
        // require mentor validation; the state datum's provenance is not
        // inherited (a confirmed count does not confirm the proposal).
        check_eq(action.provenance.source_type, SourceType::Synthetic,
                 "rule-generated provenance is synthetic");
        check(action.provenance.needs_mentor_validation,
              "rule-generated actions need mentor validation");
        if (add != nullptr) {
            check_eq(add->additional_units, 1, "+1 unit per rule candidate");
            if (add->resource == ResourceId{"transporter"}) has_transporter = true;
            if (add->resource == ResourceId{"agv"}) has_agv = true;
        }
    }
    check(has_transporter && has_agv, "both synthetic resources covered");
}

void test_baseline1_provenance_not_inherited() {
    // A mentor-confirmed resource datum must NOT produce a
    // mentor-confirmed action label.
    FactoryState state = synthetic_state();
    state.resources[ResourceId{"transporter"}].provenance = Provenance{
        SourceType::MentorConfirmed, "confirmed datum", false};

    const auto candidates = generate_rule_based_candidates(state);
    for (const auto& action : candidates) {
        check_eq(action.provenance.source_type, SourceType::Synthetic,
                 "generated action never inherits datum provenance");
    }
}

void test_baseline2_infeasible_never_simulated() {
    RecordingTestEvaluator evaluator;
    const FactoryState state = synthetic_state();
    // add 2 transporters exceeds ceiling 3 -> INFEASIBLE.
    const std::vector<Action> proposed{make_add_transporter(2)};

    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    check_eq(result.candidates.size(), std::size_t{2},
             "proposed + a_0 both visible");
    check_eq(result.candidates[0].feasibility.status,
             FeasibilityStatus::Infeasible, "over-ceiling candidate infeasible");
    check(!result.candidates[0].simulated, "INFEASIBLE never simulated");
    check_eq(result.selected, std::size_t{1},
             "a_0 selected when the only candidate is infeasible");
    check_eq(result.status, SelectionStatus::NoAction,
             "status: no action recommended");
    for (const auto& call : evaluator.calls()) {
        check_eq(call.action, ActionId{"no-action"},
                 "only a_0 reaches the simulator");
    }
}

void test_baseline2_unknown_never_silently_eligible() {
    RecordingTestEvaluator evaluator;
    FactoryState state = synthetic_state();
    // Unknown ceiling for agv -> add-agv is UNKNOWN, not eligible.
    state.resources[ResourceId{"agv"}].max_count = std::nullopt;

    const std::vector<Action> proposed{make_add_agv(1)};
    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    check_eq(result.candidates[0].feasibility.status,
             FeasibilityStatus::Unknown, "unknown ceiling yields UNKNOWN");
    check(!result.candidates[0].simulated, "UNKNOWN never simulated");
    check_eq(result.selected, std::size_t{1},
             "a_0 selected; UNKNOWN not silently eligible");
    check_eq(result.status, SelectionStatus::NoAction,
             "status: no action recommended");
}

void test_baseline2_selection_by_policy() {
    RecordingTestEvaluator evaluator;
    const FactoryState state = synthetic_state();
    const std::vector<Action> proposed{make_add_transporter(1),
                                       make_add_agv(1)};

    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    check_eq(result.candidates.size(), std::size_t{3},
             "two candidates + a_0");
    check_eq(result.candidates[0].simulated, true, "candidate 1 simulated");
    check_eq(result.candidates[1].simulated, true, "candidate 2 simulated");
    check_eq(result.candidates[2].action.id, ActionId{"no-action"},
             "a_0 appended as reference");
    check_eq(result.selected, std::size_t{0},
             "add-transporter (throughput 115, SYNTHETIC) beats add-agv (110)");
    check_eq(result.status, SelectionStatus::ActionSelected,
             "status: action selected");
    check_eq(evaluator.calls().size(), std::size_t{3},
             "two candidates + a_0 simulated, all under shared seed");
    check_eq(evaluator.calls()[0].seed, Seed{7}, "shared seed candidate 1");
    check_eq(evaluator.calls()[1].seed, Seed{7}, "shared seed candidate 2");
    check_eq(evaluator.calls()[2].seed, Seed{7}, "shared seed a_0");
}

void test_baseline2_all_candidates_infeasible_selects_no_action() {
    RecordingTestEvaluator evaluator;
    FactoryState state = synthetic_state();
    state.resources[ResourceId{"transporter"}].max_count = 2;
    state.resources[ResourceId{"agv"}].max_count = 1;

    const std::vector<Action> proposed{make_add_transporter(1),
                                       make_add_agv(1)};
    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    check_eq(result.candidates[0].feasibility.status,
             FeasibilityStatus::Infeasible, "candidate 1 infeasible");
    check_eq(result.candidates[1].feasibility.status,
             FeasibilityStatus::Infeasible, "candidate 2 infeasible");
    check_eq(result.selected, std::size_t{2},
             "a_0 selected when every candidate is infeasible");
    check_eq(result.status, SelectionStatus::NoAction,
             "status: status quo (a_0 itself is feasible here)");
}

void test_baseline2_no_feasible_option_when_a0_also_infeasible() {
    // A statically known status-quo violation (count 5 > ceiling 3, both
    // KNOWN) makes a_0 infeasible; the only candidate is also infeasible
    // -> NO FEASIBLE OPTION at all; selected points at the a_0 entry so
    // the status-quo violation is inspectable.
    RecordingTestEvaluator evaluator;
    FactoryState state = synthetic_state();
    state.resources[ResourceId{"transporter"}] = {5, 3, synthetic_provenance()};
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));

    const std::vector<Action> proposed{make_add_transporter(1)};
    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, registry, evaluator,
        SyntheticLexicographicPreference{});

    check_eq(result.candidates[0].feasibility.status,
             FeasibilityStatus::Infeasible,
             "candidate infeasible (5+1 > 3)");
    check_eq(result.candidates[1].feasibility.status,
             FeasibilityStatus::Infeasible,
             "a_0 infeasible (known count 5 > ceiling 3)");
    check(!result.candidates[1].simulated, "infeasible a_0 not simulated");
    check_eq(result.status, SelectionStatus::NoFeasibleOption,
             "status: no feasible option");
    check_eq(result.selected, std::size_t{1},
             "selected points at the a_0 entry (inspectable violation)");
    check_eq(evaluator.calls().size(), std::size_t{0},
             "nothing simulated");
}

void test_baseline2_infeasible_a0_feasible_fix_selected() {
    // Known status-quo violation on the transporter (count 5 > ceiling 3)
    // makes a_0 infeasible; a feasible fix on ANOTHER resource exists: the
    // fix is selected without comparison to the infeasible a_0, and the
    // infeasible a_0 is never simulated.
    RecordingTestEvaluator evaluator;
    FactoryState state = synthetic_state();
    state.resources[ResourceId{"transporter"}] = {5, 3, synthetic_provenance()};
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    registry.add(make_resource_count_constraint(ResourceId{"agv"}));

    const std::vector<Action> proposed{make_add_agv(1)};
    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, registry, evaluator,
        SyntheticLexicographicPreference{});

    check_eq(result.candidates[0].feasibility.status,
             FeasibilityStatus::Feasible, "fix candidate feasible (1+1<=2)");
    check_eq(result.candidates[1].feasibility.status,
             FeasibilityStatus::Infeasible, "a_0 infeasible (transporter)");
    check(!result.candidates[1].simulated, "infeasible a_0 not simulated");
    check_eq(result.status, SelectionStatus::ActionSelected,
             "feasible fix selected when a_0 violates the state");
    check_eq(result.selected, std::size_t{0},
             "the fix is the selected entry");
    check_eq(evaluator.calls().size(), std::size_t{1},
             "only the feasible candidate is simulated");
    check_eq(evaluator.calls()[0].action, ActionId{"add-agv"},
             "the simulated call is the fix, not a_0");
}

void test_baseline2_tie_with_a0_keeps_status_quo() {
    // A candidate whose outcome equals a_0's is NOT a recommendation: a
    // zero-improvement change with unknown cost must not displace the
    // status quo (minimum-meaningful-improvement discipline).
    RecordingTestEvaluator evaluator;
    evaluator.tie_with_no_action.push_back(ActionId{"add-transporter"});
    const FactoryState state = synthetic_state();
    const std::vector<Action> proposed{make_add_transporter(1)};

    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    check_eq(result.candidates[0].feasibility.status,
             FeasibilityStatus::Feasible, "candidate feasible");
    check_eq(result.status, SelectionStatus::NoAction,
             "tie with a_0 keeps the status quo");
    check_eq(result.selected, std::size_t{1},
             "a_0 is the selected entry");
}

void test_baseline2_known_cost_semantics() {
    // Known costs compare numerically (smaller wins); a known cost never
    // fabricates a preference over an unknown cost (units UNKNOWN pre-tour).
    RecordingTestEvaluator evaluator;
    evaluator.known_costs[ActionId{"add-transporter"}] = 10.0;
    evaluator.known_costs[ActionId{"add-agv"}] = 5.0;
    const FactoryState state = synthetic_state();
    const std::vector<Action> proposed{make_add_transporter(1),
                                       make_add_agv(1)};

    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});
    // add-transporter: throughput 115 (count 3); add-agv: throughput 110.
    // Throughput dominates cost in the lexicographic policy, so cost only
    // breaks a throughput+lead-time tie.
    check_eq(result.selected, std::size_t{0},
             "throughput dominates; known costs only break ties");
}

void test_baseline2_cost_breaks_ties_numerically() {
    // Two candidates with identical throughput/lead time: smaller known
    // cost wins; a known-vs-unknown pair ties (no fabricated preference).
    RecordingTestEvaluator evaluator;
    evaluator.known_costs[ActionId{"add-transporter"}] = 10.0;
    FactoryState state = synthetic_state();
    // Identical counts and ceilings -> identical throughput/lead time.
    state.resources[ResourceId{"agv"}] = {2, 3, synthetic_provenance()};
    const std::vector<Action> proposed{make_add_transporter(1),
                                       make_add_agv(1)};

    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    // add-transporter cost 10 (known) vs add-agv cost unknown -> tie on
    // cost -> id tie-break -> "add-agv" < "add-transporter".
    check_eq(result.selected, std::size_t{1},
             "known-vs-unknown cost ties; id tie-break applies");

    // Both-known case: cheaper wins regardless of id ordering.
    RecordingTestEvaluator evaluator2;
    evaluator2.known_costs[ActionId{"add-transporter"}] = 4.0;
    evaluator2.known_costs[ActionId{"add-agv"}] = 8.0;
    const auto result2 = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator2, SyntheticLexicographicPreference{});
    check_eq(result2.selected, std::size_t{0},
             "smaller known cost wins the tie");
}

void test_baseline2_nan_outcome_rejected() {
    // Non-finite KPIs would make policy comparison order-dependent; the
    // pipeline rejects them at intake.
    struct NaNEvaluator final : SimulationEvaluator {
        [[nodiscard]] SimulationResult evaluate(
            const FactoryState&,
            const Action&,
            const ScenarioId& scenario,
            Seed seed) const override {
            SimulationResult result;
            result.scenario = scenario;
            result.seed = seed;
            result.kpis.throughput = std::nan("");
            result.kpis.lead_time = 30.0;
            return result;
        }
    };
    NaNEvaluator evaluator;
    const FactoryState state = synthetic_state();
    const std::vector<Action> proposed{make_add_transporter(1)};
    bool threw = false;
    try {
        static_cast<void>(run_greedy_evaluation(
            state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
            evaluator, SyntheticLexicographicPreference{}));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "non-finite KPI outcome rejected (fail clearly)");
}

void test_baseline2_evidence_bearing_state_rejected() {
    // Evidence in FactoryState describes one action's run; sharing it
    // across all gates would leak verdicts between candidates. Rejected.
    RecordingTestEvaluator evaluator;
    FactoryState state = synthetic_state();
    state.buffer_capacities[LocationId{"buffer_a"}] = 20;
    state.simulated_buffer_content[LocationId{"buffer_a"}] = 5;
    const std::vector<Action> proposed{make_add_transporter(1)};
    bool threw = false;
    try {
        static_cast<void>(run_greedy_evaluation(
            state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
            evaluator, SyntheticLexicographicPreference{}));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "evidence-bearing state rejected in batch evaluation");
}

void test_baseline2_empty_proposed_selects_no_action() {
    RecordingTestEvaluator evaluator;
    const FactoryState state = synthetic_state();
    const std::vector<Action> proposed;

    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    check_eq(result.candidates.size(), std::size_t{1}, "only a_0 present");
    check_eq(result.selected, std::size_t{0}, "a_0 selected");
    check_eq(result.status, SelectionStatus::NoAction,
             "empty proposal -> status quo");
}

void test_unknown_cost_never_zero() {
    RecordingTestEvaluator evaluator;
    const FactoryState state = synthetic_state();
    const auto a0 = evaluate_no_action(
        state, ScenarioId{"synthetic"}, 1, default_registry(), evaluator);

    check(!a0.outcome.kpis.cost.has_value(),
          "unknown cost must stay nullopt, never 0.0");
}

void test_determinism_same_inputs_same_recommendation() {
    RecordingTestEvaluator evaluator_a;
    RecordingTestEvaluator evaluator_b;
    const FactoryState state = synthetic_state();
    const std::vector<Action> proposed{make_add_transporter(1),
                                       make_add_agv(1)};

    const auto run_a = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 9, default_registry(),
        evaluator_a, SyntheticLexicographicPreference{});
    const auto run_b = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 9, default_registry(),
        evaluator_b, SyntheticLexicographicPreference{});

    check_eq(run_a.selected, run_b.selected,
             "same inputs -> same selected index");
    check_eq(run_a.status, run_b.status, "same inputs -> same status");
    check_eq(run_a.candidates.size(), run_b.candidates.size(),
             "same candidate count");
    for (std::size_t i = 0; i < run_a.candidates.size(); ++i) {
        check_eq(run_a.candidates[i].action.id, run_b.candidates[i].action.id,
                 "same candidate order");
    }
}

void test_permutation_invariance() {
    // Winner must not depend on the order candidates are proposed.
    const FactoryState state = synthetic_state();
    const std::vector<Action> base{make_add_transporter(1),
                                   make_add_agv(1)};
    // agv raised to {2,3} -> identical outcomes for both candidates ->
    // ordering must not flip the id-tie-break winner.
    FactoryState tied_state = synthetic_state();
    tied_state.resources[ResourceId{"agv"}] = {2, 3, synthetic_provenance()};

    const std::vector<Action> direct{make_add_transporter(1),
                                      make_add_agv(1)};
    const std::vector<Action> reversed{make_add_agv(1),
                                       make_add_transporter(1)};

    RecordingTestEvaluator evaluator_direct;
    RecordingTestEvaluator evaluator_reversed;
    const auto run_direct = run_greedy_evaluation(
        tied_state, direct, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator_direct, SyntheticLexicographicPreference{});
    const auto run_reversed = run_greedy_evaluation(
        tied_state, reversed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator_reversed, SyntheticLexicographicPreference{});

    check_eq(run_direct.candidates[run_direct.selected].action.id,
             run_reversed.candidates[run_reversed.selected].action.id,
             "selection invariant under candidate permutation");
    check_eq(run_direct.status, run_reversed.status,
             "status invariant under permutation");
}

void test_tie_breaking_deterministic() {
    RecordingTestEvaluator evaluator;
    FactoryState state = synthetic_state();
    // Two resources with identical counts and ceilings -> identical synthetic
    // outcomes -> policy tie -> id ordering decides.
    state.resources[ResourceId{"agv"}] = {2, 3, synthetic_provenance()};

    const std::vector<Action> proposed{make_add_transporter(1),
                                       make_add_agv(1)};
    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 7, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    // Both produce throughput 115 (count 3): tie -> "add-agv" < "add-transporter".
    check_eq(result.selected, std::size_t{1},
             "tie broken by ascending action id (add-agv)");
}

// --- Oracle: exact best by brute force over the same evaluator ---

void test_oracle_matches_greedy_selection() {
    RecordingTestEvaluator evaluator;
    const FactoryState state = synthetic_state();
    std::vector<Action> proposed{make_add_transporter(0),
                                  make_add_transporter(1),
                                  make_add_agv(1)};

    const auto result = run_greedy_evaluation(
        state, proposed, ScenarioId{"synthetic"}, 11, default_registry(),
        evaluator, SyntheticLexicographicPreference{});

    // Oracle: evaluate every feasible candidate AND a_0 independently and
    // pick the exact best under the same synthetic policy (throughput max,
    // lead-time min, known-cost min, id-ascending tie), applying the same
    // selection rule (candidate wins only when strictly preferred over a_0).
    auto outcome_of = [&](const Action& action) {
        return evaluator.evaluate(
            state, action, ScenarioId{"synthetic"}, Seed{11});
    };
    auto strictly_better = [](const SimulationResult& a,
                              const SimulationResult& b) {
        if (a.kpis.throughput != b.kpis.throughput) {
            return a.kpis.throughput > b.kpis.throughput;
        }
        if (a.kpis.lead_time != b.kpis.lead_time) {
            return a.kpis.lead_time < b.kpis.lead_time;
        }
        return false;  // tie on the policy keys used by this fixture
    };

    const SimulationResult a0_outcome = outcome_of(
        ActionCatalog::no_action());
    std::string oracle_id = "no-action";
    SimulationResult oracle_outcome = a0_outcome;
    for (const auto& action : proposed) {
        if (evaluate_feasibility(state, action, default_registry()).status !=
            FeasibilityStatus::Feasible) {
            continue;
        }
        const auto outcome = outcome_of(action);
        if (strictly_better(outcome, oracle_outcome) ||
            (outcome.kpis.throughput == oracle_outcome.kpis.throughput &&
             outcome.kpis.lead_time == oracle_outcome.kpis.lead_time &&
             action.id.value < oracle_id)) {
            oracle_outcome = outcome;
            oracle_id = action.id.value;
        }
    }
    check_eq(result.candidates[result.selected].action.id.value, oracle_id,
             "greedy selection equals exhaustive-oracle optimum");
}

}  // namespace

int main() {
    test_baseline0_no_action_evaluated_normally();
    test_baseline0_infeasible_a0_not_simulated();
    test_baseline1_rule_generation();
    test_baseline1_provenance_not_inherited();
    test_baseline2_infeasible_never_simulated();
    test_baseline2_unknown_never_silently_eligible();
    test_baseline2_selection_by_policy();
    test_baseline2_all_candidates_infeasible_selects_no_action();
    test_baseline2_no_feasible_option_when_a0_also_infeasible();
    test_baseline2_infeasible_a0_feasible_fix_selected();
    test_baseline2_tie_with_a0_keeps_status_quo();
    test_baseline2_known_cost_semantics();
    test_baseline2_cost_breaks_ties_numerically();
    test_baseline2_nan_outcome_rejected();
    test_baseline2_evidence_bearing_state_rejected();
    test_baseline2_empty_proposed_selects_no_action();
    test_unknown_cost_never_zero();
    test_determinism_same_inputs_same_recommendation();
    test_permutation_invariance();
    test_tie_breaking_deterministic();
    test_oracle_matches_greedy_selection();

    std::cout << checks_run << " checks, " << checks_failed << " failed\n";
    return checks_failed == 0 ? 0 : 1;
}
