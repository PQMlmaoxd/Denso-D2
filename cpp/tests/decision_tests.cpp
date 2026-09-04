// Executable specification tests for the D2 decision core.
//
// Canonical math source: docs/decision/modeling/d2_decision_model.tex
// (eq:phistatus, eq:verdict, eq:unknownrule, eq:monotone, sec:hardsoft
// a_0 rule). All fixture values are SYNTHETIC and labeled as such; none
// is a DENSO fact.
//
// Self-contained stdlib-only harness: deterministic, no external
// dependencies, exits non-zero on the first failing check with a
// readable message.

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "denso_d2/decision/action.hpp"
#include "denso_d2/decision/catalog.hpp"
#include "denso_d2/decision/gate.hpp"

namespace {

int checks_run = 0;
int checks_failed = 0;

// Registers one named check. The vector keeps registration order so the
// process is deterministic run-to-run.
struct TestCase {
    std::string name;
    std::function<void()> body;
};
std::vector<TestCase>& test_cases() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> body) {
        test_cases().push_back({std::move(name), std::move(body)});
    }
};

#define TEST(name)                                       \
    static void test_##name();                           \
    static Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

void check(bool condition, const std::string& message) {
    ++checks_run;
    if (!condition) {
        ++checks_failed;
        std::cerr << "FAIL: " << message << "\n";
    }
}

template <typename T>
void check_eq(const T& actual, const T& expected, const std::string& message) {
    ++checks_run;
    if (!(actual == expected)) {
        ++checks_failed;
        std::cerr << "FAIL: " << message << "\n";
    }
}

// ---------- synthetic fixture builders ----------
// SYNTHETIC: 2 transporters, pool ceiling 3, buffer capacity 20.
// These are the canonical development values, never DENSO facts.

using denso_d2::decision::Action;
using denso_d2::decision::ActionCatalog;
using denso_d2::decision::ActionId;
using denso_d2::decision::AddTemporaryCapacity;
using denso_d2::decision::AdjustBuffer;
using denso_d2::decision::BudgetBound;
using denso_d2::decision::BufferCapacityBound;
using denso_d2::decision::ChangeDispatchPriority;
using denso_d2::decision::ChangeReplenishmentInterval;
using denso_d2::decision::ChangeRoute;
using denso_d2::decision::ClaimStatus;
using denso_d2::decision::Constraint;
using denso_d2::decision::ConstraintCategory;
using denso_d2::decision::ConstraintId;
using denso_d2::decision::ConstraintRegistry;
using denso_d2::decision::ConstraintStatus;
using denso_d2::decision::DispatchCatalogOnly;
using denso_d2::decision::FactoryState;
using denso_d2::decision::FeasibilityResult;
using denso_d2::decision::FeasibilityStatus;
using denso_d2::decision::LocationId;
using denso_d2::decision::MaterialId;
using denso_d2::decision::NoAction;
using denso_d2::decision::Provenance;
using denso_d2::decision::ReallocateResource;
using denso_d2::decision::ReplenishmentIntervalBound;
using denso_d2::decision::ResourceCountBound;
using denso_d2::decision::ResourceId;
using denso_d2::decision::ResponseDeadline;
using denso_d2::decision::RouteId;
using denso_d2::decision::RouteKnown;
using denso_d2::decision::SourceType;

Provenance synthetic_provenance() {
    return Provenance{SourceType::Synthetic, "synthetic fixture", false};
}

FactoryState synthetic_state() {
    FactoryState state;
    state.resources[ResourceId{"transporter"}] =
        FactoryState::ResourceInfo{2, 3, synthetic_provenance()};
    state.resources[ResourceId{"agv"}] =
        FactoryState::ResourceInfo{1, 2, synthetic_provenance()};
    state.buffer_capacities[LocationId{"buffer_a"}] = 20;
    state.allowed_dispatch_rules["bottleneck-first"] = true;
    state.allowed_dispatch_rules["fifo"] = true;
    state.known_routes[RouteId{"route_main"}] = true;
    state.known_routes[RouteId{"route_alt"}] = true;
    state.replenishment_bounds[MaterialId{"part_x"}] =
        FactoryState::IntervalBound{5, 60};
    state.action_lead_times_min[ActionId{"add-transporter"}] = 10;
    return state;
}

Constraint make_resource_count_constraint(const ResourceId& resource) {
    return Constraint{
        ConstraintId{"C-PHY-002-" + resource.value},
        ConstraintCategory::Physical,
        denso_d2::decision::ConstraintCheckLayer::Static,
        denso_d2::decision::Severity::Hard,
        ResourceCountBound{resource},
        synthetic_provenance(),
        true,
    };
}

Constraint make_buffer_constraint(const LocationId& buffer) {
    return Constraint{
        ConstraintId{"C-PHY-003-" + buffer.value},
        ConstraintCategory::Physical,
        denso_d2::decision::ConstraintCheckLayer::SimulatorEnforced,
        denso_d2::decision::Severity::Hard,
        BufferCapacityBound{buffer},
        synthetic_provenance(),
        true,
    };
}

Constraint make_dispatch_constraint() {
    return Constraint{
        ConstraintId{"C-OPS-003"},
        ConstraintCategory::Operational,
        denso_d2::decision::ConstraintCheckLayer::Static,
        denso_d2::decision::Severity::Hard,
        DispatchCatalogOnly{},
        synthetic_provenance(),
        true,
    };
}

Constraint make_route_constraint(const RouteId& route) {
    return Constraint{
        ConstraintId{"C-PHY-005-" + route.value},
        ConstraintCategory::Physical,
        denso_d2::decision::ConstraintCheckLayer::Static,
        denso_d2::decision::Severity::Hard,
        RouteKnown{route},
        synthetic_provenance(),
        true,
    };
}

Constraint make_replenishment_constraint(const MaterialId& material) {
    return Constraint{
        ConstraintId{"C-OPS-004-" + material.value},
        ConstraintCategory::Operational,
        denso_d2::decision::ConstraintCheckLayer::Static,
        denso_d2::decision::Severity::Hard,
        ReplenishmentIntervalBound{material},
        synthetic_provenance(),
        true,
    };
}

Constraint make_deadline_constraint() {
    return Constraint{
        ConstraintId{"C-OPS-007"},
        ConstraintCategory::Operational,
        denso_d2::decision::ConstraintCheckLayer::Static,
        denso_d2::decision::Severity::Hard,
        ResponseDeadline{},
        synthetic_provenance(),
        true,
    };
}

Constraint make_budget_constraint() {
    return Constraint{
        ConstraintId{"C-BUS-001"},
        ConstraintCategory::Business,
        denso_d2::decision::ConstraintCheckLayer::Static,
        denso_d2::decision::Severity::Hard,
        BudgetBound{},
        synthetic_provenance(),
        true,
    };
}

Action make_add_transporter(int units) {
    return Action{
        ActionId{"add-transporter"},
        AddTemporaryCapacity{ResourceId{"transporter"}, units},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
}

Action make_adjust_buffer(const LocationId& buffer, int delta) {
    return Action{
        ActionId{"adjust-buffer"},
        AdjustBuffer{buffer, delta},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
}

Action make_change_priority(const std::string& rule) {
    return Action{
        ActionId{"change-priority"},
        ChangeDispatchPriority{rule},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
}

Action make_no_action_action() {
    return Action{
        ActionId{"no-action"},
        NoAction{},
        ClaimStatus::Generic,
        synthetic_provenance(),
    };
}

ConstraintRegistry default_registry() {
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    registry.add(make_resource_count_constraint(ResourceId{"agv"}));
    registry.add(make_buffer_constraint(LocationId{"buffer_a"}));
    registry.add(make_dispatch_constraint());
    return registry;
}

// ---------- T1: NO_ACTION is an explicit domain action ----------
TEST(no_action_is_explicit_domain_action) {
    const Action no_action = make_no_action_action();
    check(denso_d2::decision::is_no_action(no_action),
          "NoAction payload must be recognized");
    check_eq(no_action.claim_status, ClaimStatus::Generic,
             "no-action claim status must be Generic");

    // a_0 rule: feasible by construction under the default registry even
    // though the budget is unknown in the state.
    FactoryState state = synthetic_state();
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, no_action, default_registry());
    check_eq(result.status, FeasibilityStatus::Feasible,
             "a_0 must be Feasible by construction (delta=0 branch)");

    // Even with a budget constraint registered (unknown budget value).
    ConstraintRegistry with_budget = default_registry();
    with_budget.add(make_budget_constraint());
    const FeasibilityResult result_with_budget =
        denso_d2::decision::evaluate_feasibility(state, no_action, with_budget);
    check_eq(result_with_budget.status, FeasibilityStatus::Feasible,
             "a_0 must stay Feasible when the budget is unknown");
}

// ---------- T2: hard violation => INFEASIBLE with constraint id ----------
TEST(hard_violation_is_infeasible_with_constraint_id) {
    FactoryState state = synthetic_state();
    // Pool ceiling 3, current 2: adding 2 transporters violates it.
    const Action action = make_add_transporter(2);
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, default_registry());
    check_eq(result.status, FeasibilityStatus::Infeasible,
             "count > ceiling must be INFEASIBLE");
    const auto blocking = denso_d2::decision::blocking_constraints(result);
    check(!blocking.empty(), "blocking constraint id must be reported");
    check_eq(blocking[0].value, std::string("C-PHY-002-transporter"),
             "violated constraint id must be the resource-count bound");
}

// ---------- T3: missing evidence, no violation => UNKNOWN ----------
TEST(missing_required_evidence_is_unknown) {
    FactoryState state = synthetic_state();
    // Replenishment bounds exist for part_x but not for part_y.
    const Action action{
        ActionId{"change-replenishment"},
        ChangeReplenishmentInterval{MaterialId{"part_y"}, 30},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    ConstraintRegistry registry;
    registry.add(make_replenishment_constraint(MaterialId{"part_y"}));
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check_eq(result.status, FeasibilityStatus::Unknown,
             "unknown replenishment bounds must be UNKNOWN, never FEASIBLE");
    check(result.has_unknown(), "evaluation vector must record the unknown");
}

// ---------- T4: all required hard satisfied => FEASIBLE ----------
TEST(all_required_hard_satisfied_is_feasible) {
    FactoryState state = synthetic_state();
    // Adding 1 transporter: 2 + 1 = 3 <= ceiling 3.
    const Action action = make_add_transporter(1);
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, default_registry());
    check_eq(result.status, FeasibilityStatus::Feasible,
             "count within ceiling with full evidence must be FEASIBLE");
}

// ---------- T5: tightening monotonicity ----------
TEST(tightening_monotonicity) {
    // Tighten by adding a constraint that APPLIES to the tested action, so
    // the Feasible -> restricted direction is exercised, not just the
    // trivial Infeasible-stays-Infeasible case.
    FactoryState state = synthetic_state();
    const Action action = make_change_priority("fifo");

    ConstraintRegistry base;
    base.add(make_resource_count_constraint(ResourceId{"transporter"}));

    ConstraintRegistry tightened = base;
    tightened.add(make_dispatch_constraint());

    // Base: the dispatch rule is accepted => Feasible under both.
    const FeasibilityResult r_base_ok =
        denso_d2::decision::evaluate_feasibility(state, action, base);
    const FeasibilityResult r_tight_ok =
        denso_d2::decision::evaluate_feasibility(state, action, tightened);
    check_eq(r_base_ok.status, FeasibilityStatus::Feasible,
             "fifo rule must be feasible with only the count bound");
    check_eq(r_tight_ok.status, FeasibilityStatus::Feasible,
             "accepted rule stays feasible after tightening");

    // Tightened state: fifo explicitly recorded as inadmissible => the
    // tightened registry must worsen the verdict from Feasible to Infeasible.
    state.allowed_dispatch_rules["fifo"] = false;
    const FeasibilityResult r_base_bad =
        denso_d2::decision::evaluate_feasibility(state, action, base);
    const FeasibilityResult r_tight_bad =
        denso_d2::decision::evaluate_feasibility(state, action, tightened);
    check_eq(r_base_bad.status, FeasibilityStatus::Feasible,
             "count bound alone does not restrict the dispatch change");
    check_eq(r_tight_bad.status, FeasibilityStatus::Infeasible,
             "adding an applicable hard constraint must not preserve Feasible here");
}

// ---------- T6: provenance survives catalog -> evaluation -> result ----------
TEST(provenance_survives_pipeline) {
    ActionCatalog catalog;
    Action action = make_add_transporter(1);
    action.provenance.source_note = "synthetic fixture note";
    catalog.add(action);

    const Action* found = catalog.find(ActionId{"add-transporter"});
    check(found != nullptr, "catalog must contain the added action");
    check_eq(found->provenance.source_note, std::string("synthetic fixture note"),
             "provenance note must survive catalog round-trip");
    check_eq(found->provenance.source_type, SourceType::Synthetic,
             "provenance source type must survive");
    check_eq(found->claim_status, ClaimStatus::Synthetic,
             "claim status must survive");

    // Catalog-level NO_ACTION entry exists exactly once with Generic status.
    const Action* no_action = catalog.find(ActionId{"no-action"});
    check(no_action != nullptr && denso_d2::decision::is_no_action(*no_action),
          "catalog must contain the explicit no-action entry");
}

// ---------- T7: simulator-enforced constraint without evidence ----------
TEST(simulator_enforced_without_evidence_is_unknown) {
    FactoryState state = synthetic_state();
    // Buffer capacity known (20) but no simulation evidence supplied:
    // a simulator-enforced constraint cannot be verified satisfied.
    const Action action = make_adjust_buffer(LocationId{"buffer_a"}, 5);
    ConstraintRegistry registry;
    registry.add(make_buffer_constraint(LocationId{"buffer_a"}));
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check_eq(result.status, FeasibilityStatus::Unknown,
             "simulator-enforced constraint without evidence must be UNKNOWN");

    // With evidence that fits: content 10 <= proposed capacity 25.
    state.simulated_buffer_content[LocationId{"buffer_a"}] = 10;
    const FeasibilityResult satisfied =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check_eq(satisfied.status, FeasibilityStatus::Feasible,
             "evidence within capacity must be FEASIBLE");

    // With evidence that overflows: content 30 > proposed capacity 25.
    state.simulated_buffer_content[LocationId{"buffer_a"}] = 30;
    const FeasibilityResult overflowed =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check_eq(overflowed.status, FeasibilityStatus::Infeasible,
             "evidence beyond capacity must be INFEASIBLE");
}

// ---------- T8: determinism ----------
TEST(determinism_same_inputs_same_outputs) {
    const FactoryState state = synthetic_state();
    const Action action = make_add_transporter(1);
    const ConstraintRegistry registry = default_registry();

    const FeasibilityResult first =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    const FeasibilityResult second =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check(first == second, "identical inputs must produce identical results");
    check(first.evaluations.size() == second.evaluations.size(),
          "evaluation counts must match");
    for (std::size_t i = 0; i < first.evaluations.size(); ++i) {
        check_eq(first.evaluations[i].constraint_id.value,
                 second.evaluations[i].constraint_id.value,
                 "evaluation order must be stable (registry order)");
    }
}

// ---------- T9: no hidden defaults ----------
TEST(no_hidden_defaults_unknown_capacity_cost_never_feasible) {
    // Ceiling unknown: adding any capacity cannot be verified.
    FactoryState state;
    state.resources[ResourceId{"transporter"}] =
        FactoryState::ResourceInfo{2, std::nullopt, synthetic_provenance()};
    const Action action = make_add_transporter(1);
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check_eq(result.status, FeasibilityStatus::Unknown,
             "unknown ceiling must yield UNKNOWN, never a silent default");

    // Unknown budget with a budget constraint: no fabricated zero cost.
    FactoryState state_with_budget_unknown = synthetic_state();
    const Action costly = make_add_transporter(1);
    ConstraintRegistry budget_registry;
    budget_registry.add(make_budget_constraint());
    const FeasibilityResult budget_result = denso_d2::decision::evaluate_feasibility(
        state_with_budget_unknown, costly, budget_registry);
    check_eq(budget_result.status, FeasibilityStatus::Unknown,
             "unknown budget/cost must yield UNKNOWN, never value_or(0)");
}

// ---------- T10: soft violation does not auto-invalidate ----------
TEST(soft_violation_does_not_auto_invalidate) {
    FactoryState state = synthetic_state();
    // Route explicitly recorded as inadmissible => a hard route constraint
    // would be violated; register the same bound as SOFT and confirm the
    // verdict is not forced to Infeasible by the soft constraint alone.
    state.known_routes[RouteId{"route_unknown"}] = false;
    Constraint soft_route = make_route_constraint(RouteId{"route_unknown"});
    soft_route.severity = denso_d2::decision::Severity::Soft;

    const Action action{
        ActionId{"change-route"},
        ChangeRoute{ResourceId{"agv"}, RouteId{"route_unknown"}},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    ConstraintRegistry registry;
    registry.add(soft_route);
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    // With only a soft constraint violated, the hard-constraint verdict
    // is Feasible; the soft violation must be visible as an evaluation.
    check_eq(result.status, FeasibilityStatus::Feasible,
             "soft violation alone must not produce INFEASIBLE");
    bool soft_violation_visible = false;
    for (const auto& evaluation : result.evaluations) {
        if (evaluation.constraint_id.value == "C-PHY-005-route_unknown"
            && evaluation.status == ConstraintStatus::Violated) {
            soft_violation_visible = true;
        }
    }
    check(soft_violation_visible,
          "the soft violation must remain visible in evaluations, no hidden penalty objective");
    check(denso_d2::decision::blocking_constraints(result).empty(),
          "a soft violation must never be reported as a hard blocking constraint");
    check(!result.has_violated(),
          "has_violated() must count hard violations only");
}

// ---------- T11: duplicate action ids rejected ----------
TEST(duplicate_action_ids_rejected) {
    ActionCatalog catalog;
    catalog.add(make_add_transporter(1));
    bool threw = false;
    try {
        catalog.add(make_add_transporter(2));  // same id, different payload
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "duplicate ActionId must throw std::invalid_argument");
}

// ---------- T12: duplicate constraint ids rejected ----------
TEST(duplicate_constraint_ids_rejected) {
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    bool threw = false;
    try {
        registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "duplicate ConstraintId must throw std::invalid_argument");
}

// ---------- T13: structured result consistency ----------
TEST(structured_result_consistency_feasible_never_with_hard_violation) {
    // FeasibilityResult constructed via the gate cannot be
    // Feasible + violated-present, by construction of
    // evaluate_feasibility. Assert the invariant on outputs of several
    // representative evaluations.
    const FactoryState state = synthetic_state();
    const ConstraintRegistry registry = default_registry();
    std::vector<Action> actions;
    actions.push_back(make_no_action_action());
    actions.push_back(make_add_transporter(1));
    actions.push_back(make_add_transporter(2));
    actions.push_back(make_change_priority("fifo"));
    actions.push_back(make_change_priority("unknown-rule"));

    for (const FeasibilityResult& result :
         denso_d2::decision::evaluate_all(state, actions, registry)) {
        const bool consistent =
            !(result.status == FeasibilityStatus::Feasible && result.has_violated());
        check(consistent,
              "Feasible result must never contain a violated hard constraint");
    }
}

// ---------- T14: batch consistency ----------
TEST(batch_consistency_individual_equals_batch) {
    const FactoryState state = synthetic_state();
    const ConstraintRegistry registry = default_registry();
    std::vector<Action> actions;
    actions.push_back(make_no_action_action());
    actions.push_back(make_add_transporter(1));
    actions.push_back(make_add_transporter(2));
    actions.push_back(make_adjust_buffer(LocationId{"buffer_a"}, -5));
    actions.push_back(make_change_priority("bottleneck-first"));

    const auto batch = denso_d2::decision::evaluate_all(state, actions, registry);
    check_eq(batch.size(), actions.size(), "batch result count must match input");
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const FeasibilityResult individual =
            denso_d2::decision::evaluate_feasibility(state, actions[i], registry);
        check(batch[i] == individual,
              "batch result must equal individual evaluation for action index "
                  + std::to_string(i));
    }
}

// ---------- golden fixture ----------
// SYNTHETIC golden case, human-verifiable by inspection:
//   2 transporters, ceiling 3; buffer capacity 20, no evidence.
//   Actions: no-action; add 1 transporter (within ceiling);
//   add 2 transporters (beyond ceiling).
//   Constraints: resource-count bound (static, hard) + buffer bound
//   (simulator-enforced, hard). Expected: Feasible / Feasible /
//   Infeasible. The buffer bound applies only to AdjustBuffer and the
//   no-action status quo; add-transporter actions are not applicable
//   (satisfied by non-application).
TEST(golden_two_resources_three_actions) {
    const FactoryState state = synthetic_state();
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    registry.add(make_buffer_constraint(LocationId{"buffer_a"}));

    const FeasibilityResult no_action_result = denso_d2::decision::evaluate_feasibility(
        state, make_no_action_action(), registry);
    check_eq(no_action_result.status, FeasibilityStatus::Feasible,
             "golden: no-action must be Feasible by construction");

    const FeasibilityResult add_one = denso_d2::decision::evaluate_feasibility(
        state, make_add_transporter(1), registry);
    check_eq(add_one.status, FeasibilityStatus::Feasible,
             "golden: add 1 transporter (2+1 <= 3) must be Feasible");

    const FeasibilityResult add_two = denso_d2::decision::evaluate_feasibility(
        state, make_add_transporter(2), registry);
    check_eq(add_two.status, FeasibilityStatus::Infeasible,
             "golden: add 2 transporters (2+2 > 3) must be Infeasible");
}

// ---------- property: registry reorder invariance ----------
TEST(property_registry_reorder_invariance) {
    const FactoryState state = synthetic_state();
    const Action action = make_add_transporter(1);

    ConstraintRegistry first;
    first.add(make_resource_count_constraint(ResourceId{"transporter"}));
    first.add(make_dispatch_constraint());
    ConstraintRegistry reordered;
    reordered.add(make_dispatch_constraint());
    reordered.add(make_resource_count_constraint(ResourceId{"transporter"}));

    const auto r_first =
        denso_d2::decision::evaluate_feasibility(state, action, first);
    const auto r_reordered =
        denso_d2::decision::evaluate_feasibility(state, action, reordered);

    // Status must be identical; evaluation order follows registration
    // order by design, so the evaluation vectors differ in order but the
    // multiset of (id, status) must be identical.
    check_eq(r_first.status, r_reordered.status,
             "registry insertion order must not change the verdict");
    std::vector<std::pair<std::string, ConstraintStatus>> first_pairs;
    std::vector<std::pair<std::string, ConstraintStatus>> reordered_pairs;
    for (const auto& e : r_first.evaluations) {
        first_pairs.emplace_back(e.constraint_id.value, e.status);
    }
    for (const auto& e : r_reordered.evaluations) {
        reordered_pairs.emplace_back(e.constraint_id.value, e.status);
    }
    std::sort(first_pairs.begin(), first_pairs.end());
    std::sort(reordered_pairs.begin(), reordered_pairs.end());
    check(first_pairs == reordered_pairs,
          "sorted (id,status) multiset must be order-invariant");
}

// ---------- property: unknown never improves to feasible by removing evidence ----------
TEST(property_missing_evidence_never_feasible) {
    // For the same action, a state WITH evidence can be Feasible while a
    // state WITHOUT evidence must be at best Unknown. Removing knowledge
    // must never upgrade the verdict from Unknown to Feasible.
    FactoryState with_evidence = synthetic_state();
    with_evidence.simulated_buffer_content[LocationId{"buffer_a"}] = 10;
    FactoryState without_evidence = synthetic_state();

    const Action action = make_adjust_buffer(LocationId{"buffer_a"}, 5);
    ConstraintRegistry registry;
    registry.add(make_buffer_constraint(LocationId{"buffer_a"}));

    const auto r_with =
        denso_d2::decision::evaluate_feasibility(with_evidence, action, registry);
    const auto r_without =
        denso_d2::decision::evaluate_feasibility(without_evidence, action, registry);
    check_eq(r_with.status, FeasibilityStatus::Feasible,
             "evidence within capacity must be Feasible");
    check_eq(r_without.status, FeasibilityStatus::Unknown,
             "no evidence must stay Unknown");
}

// ---------- property: adding a confirmed violation never improves feasibility ----------
TEST(property_confirmed_violation_never_improves) {
    const FactoryState state = synthetic_state();
    const Action action = make_add_transporter(2);  // violates the ceiling

    ConstraintRegistry base;
    base.add(make_resource_count_constraint(ResourceId{"transporter"}));
    ConstraintRegistry extended = base;
    extended.add(make_dispatch_constraint());

    const auto r_base = denso_d2::decision::evaluate_feasibility(state, action, base);
    const auto r_extended =
        denso_d2::decision::evaluate_feasibility(state, action, extended);
    check_eq(r_base.status, FeasibilityStatus::Infeasible, "base must be Infeasible");
    check_eq(r_extended.status, FeasibilityStatus::Infeasible,
             "adding constraints must not turn a violation feasible");
}

// ---------- property loop: tightening monotonicity over generated actions ----------
TEST(property_loop_tightening_monotonicity) {
    const FactoryState state = synthetic_state();
    std::vector<Action> actions;
    for (int units = 0; units <= 4; ++units) {
        actions.push_back(make_add_transporter(units));
    }
    actions.push_back(make_adjust_buffer(LocationId{"buffer_a"}, -30));
    actions.push_back(make_change_priority("fifo"));

    ConstraintRegistry base;
    base.add(make_resource_count_constraint(ResourceId{"transporter"}));
    ConstraintRegistry tightened = base;
    tightened.add(make_buffer_constraint(LocationId{"buffer_a"}));
    ConstraintRegistry tightest = tightened;
    tightest.add(make_dispatch_constraint());

    const auto base_results = denso_d2::decision::evaluate_all(state, actions, base);
    const auto tightened_results =
        denso_d2::decision::evaluate_all(state, actions, tightened);
    const auto tightest_results =
        denso_d2::decision::evaluate_all(state, actions, tightest);

    auto verdict_rank = [](FeasibilityStatus s) {
        // Feasible is the "widest" verdict; Infeasible/Unknown restrict.
        // Monotonicity: an action feasible under H' must be feasible
        // under H (subset relation on the feasible set).
        return s == FeasibilityStatus::Feasible ? 0 : 1;
    };

    for (std::size_t i = 0; i < actions.size(); ++i) {
        check(verdict_rank(tightened_results[i].status)
                  >= verdict_rank(base_results[i].status),
              "tightened registry must not make more actions feasible (index "
                  + std::to_string(i) + ")");
        check(verdict_rank(tightest_results[i].status)
                  >= verdict_rank(tightened_results[i].status),
              "tightest registry must not make more actions feasible (index "
                  + std::to_string(i) + ")");
    }
}

// ---------- six canonical synthetic scenarios ----------
// Lightweight decision-state fixtures per the decision model report
// 6; they are named exactly as in the repository docs.

// balanced_flow: no bottleneck pressure; states fully known.
TEST(scenario_balanced_flow) {
    FactoryState state = synthetic_state();
    state.response_deadline_min = 30;
    // Fully-known state: the dispatch change also has a known lead time
    // (SYNTHETIC value) so the deadline constraint can verify it.
    state.action_lead_times_min[ActionId{"change-priority"}] = 5;
    ConstraintRegistry registry = default_registry();
    registry.add(make_deadline_constraint());

    const auto results = denso_d2::decision::evaluate_all(
        state,
        {make_no_action_action(), make_add_transporter(1),
         make_change_priority("fifo")},
        registry);
    check_eq(results[0].status, FeasibilityStatus::Feasible,
             "balanced_flow: no-action must be feasible");
    check_eq(results[1].status, FeasibilityStatus::Feasible,
             "balanced_flow: in-ceiling add must be feasible");
    check_eq(results[2].status, FeasibilityStatus::Feasible,
             "balanced_flow: allowed rule change must be feasible");
}

// known_transport_bottleneck: decision-core slice of the canonical
// scenario. FactoryState cannot express utilization/bottleneck signals;
// this test exercises the feasibility of the canonical add action only.
TEST(scenario_known_transport_bottleneck) {
    FactoryState state = synthetic_state();
    const auto results = denso_d2::decision::evaluate_all(
        state, {make_add_transporter(1)}, default_registry());
    check_eq(results[0].status, FeasibilityStatus::Feasible,
             "known_transport_bottleneck: feasible add-transporter must exist");
}

// buffer_overflow: queue at/over capacity; the buffer constraint sees
// evidence beyond capacity => violation reported for non-adjusting
// actions too; adjust-buffer upward with fitting evidence is feasible.
TEST(scenario_buffer_overflow) {
    FactoryState state = synthetic_state();
    state.simulated_buffer_content[LocationId{"buffer_a"}] = 25;  // > 20
    ConstraintRegistry registry;
    registry.add(make_buffer_constraint(LocationId{"buffer_a"}));

    const FeasibilityResult status_quo = denso_d2::decision::evaluate_feasibility(
        state, make_no_action_action(), registry);
    // a_0 with evidence of an existing overflow: the invariant is
    // violated in the current state; the report must show it.
    check_eq(status_quo.status, FeasibilityStatus::Infeasible,
             "buffer_overflow: existing overflow must be reported for a_0");

    const FeasibilityResult expand = denso_d2::decision::evaluate_feasibility(
        state, make_adjust_buffer(LocationId{"buffer_a"}, 10), registry);
    // 20 + 10 = 30 >= 25 content: satisfies with evidence.
    check_eq(expand.status, FeasibilityStatus::Feasible,
             "buffer_overflow: capacity expansion fitting content must be feasible");
}

// demand_spike: decision-core slice of the canonical scenario. FactoryState
// cannot express forecast pressure (p90); this test exercises the
// feasibility core under a tight response deadline instead.
TEST(scenario_demand_spike) {
    FactoryState state = synthetic_state();
    state.response_deadline_min = 15;
    ConstraintRegistry registry = default_registry();
    registry.add(make_deadline_constraint());

    // Expected values: add 1 => ceiling fits (2+1 <= 3) and lead time fits
    // (10 <= 15) => Feasible; add 2 => exceeds ceiling 3 => Infeasible.
    const auto results = denso_d2::decision::evaluate_all(
        state, {make_add_transporter(1), make_add_transporter(2)}, registry);
    check_eq(results[0].status, FeasibilityStatus::Feasible,
             "demand_spike: lead time 10 <= deadline 15 must be feasible");
    check_eq(results[1].status, FeasibilityStatus::Infeasible,
             "demand_spike: over-ceiling add must stay infeasible");
}

// resource_shortage: pool ceiling makes ADD_RESOURCE infeasible.
TEST(scenario_resource_shortage) {
    FactoryState state = synthetic_state();
    // Ceiling equals current count: no headroom.
    state.resources[ResourceId{"transporter"}].max_count = 2;
    const FeasibilityResult result = denso_d2::decision::evaluate_feasibility(
        state, make_add_transporter(1), default_registry());
    check_eq(result.status, FeasibilityStatus::Infeasible,
             "resource_shortage: zero headroom must make add infeasible");
}

// all_actions_infeasible: the valid output is every change-carrying
// action excluded with named violations, while a_0 stays feasible.
TEST(scenario_all_actions_infeasible) {
    FactoryState state = synthetic_state();
    state.resources[ResourceId{"transporter"}].max_count = 2;  // no headroom
    state.allowed_dispatch_rules.clear();  // allowed set unknown
    ConstraintRegistry registry = default_registry();

    const auto results = denso_d2::decision::evaluate_all(
        state,
        {make_add_transporter(1), make_change_priority("bottleneck-first"),
         make_no_action_action()},
        registry);
    check_eq(results[0].status, FeasibilityStatus::Infeasible,
             "all_actions_infeasible: over-ceiling add must be infeasible");
    check_eq(results[1].status, FeasibilityStatus::Unknown,
             "all_actions_infeasible: unknown allowed rules must be UNKNOWN");
    check_eq(results[2].status, FeasibilityStatus::Feasible,
             "all_actions_infeasible: a_0 must remain feasible");
    const auto blocking = denso_d2::decision::blocking_constraints(results[0]);
    check(!blocking.empty() && blocking[0].value == "C-PHY-002-transporter",
          "all_actions_infeasible: violations must name the constraint");
}

// ---------- C-OPS-007 deadline evaluation ----------
TEST(deadline_constraint_behavior) {
    FactoryState state = synthetic_state();
    state.response_deadline_min = 5;  // tighter than the 10-min lead time
    ConstraintRegistry registry;
    registry.add(make_deadline_constraint());

    const FeasibilityResult late = denso_d2::decision::evaluate_feasibility(
        state, make_add_transporter(1), registry);
    check_eq(late.status, FeasibilityStatus::Infeasible,
             "lead time 10 > deadline 5 must be INFEASIBLE");

    const FactoryState no_deadline_state = [] {
        FactoryState s = synthetic_state();
        s.response_deadline_min = std::nullopt;
        return s;
    }();
    const FeasibilityResult unknown_deadline = denso_d2::decision::evaluate_feasibility(
        no_deadline_state, make_add_transporter(1), registry);
    check_eq(unknown_deadline.status, FeasibilityStatus::Unknown,
             "absent deadline must be UNKNOWN");
}

// ---------- C-OPS-004 replenishment bounds ----------
TEST(replenishment_bounds_behavior) {
    const FactoryState state = synthetic_state();
    ConstraintRegistry registry;
    registry.add(make_replenishment_constraint(MaterialId{"part_x"}));

    const Action in_bounds{
        ActionId{"replenish-30"},
        ChangeReplenishmentInterval{MaterialId{"part_x"}, 30},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    const Action too_fast{
        ActionId{"replenish-1"},
        ChangeReplenishmentInterval{MaterialId{"part_x"}, 1},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    const Action too_slow{
        ActionId{"replenish-120"},
        ChangeReplenishmentInterval{MaterialId{"part_x"}, 120},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    check_eq(denso_d2::decision::evaluate_feasibility(state, in_bounds, registry).status,
             FeasibilityStatus::Feasible, "interval 30 in [5,60] must be feasible");
    check_eq(denso_d2::decision::evaluate_feasibility(state, too_slow, registry).status,
             FeasibilityStatus::Infeasible, "interval 120 > 60 must be infeasible");
    const auto infeasible_result =
        denso_d2::decision::evaluate_feasibility(state, too_fast, registry);
    check_eq(infeasible_result.status, FeasibilityStatus::Infeasible,
             "interval 1 < 5 must be infeasible");
    const auto blocking = denso_d2::decision::blocking_constraints(infeasible_result);
    check(!blocking.empty() && blocking[0].value == "C-OPS-004-part_x",
          "replenishment violation must name C-OPS-004");
}

// ---------- C-PHY-005 route connectivity ----------
TEST(route_known_behavior) {
    const FactoryState state = synthetic_state();
    ConstraintRegistry registry;
    registry.add(make_route_constraint(RouteId{"route_main"}));

    const Action known{
        ActionId{"route-main"},
        ChangeRoute{ResourceId{"agv"}, RouteId{"route_main"}},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    const Action unknown_route{
        ActionId{"route-bad"},
        ChangeRoute{ResourceId{"agv"}, RouteId{"route_missing"}},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    check_eq(denso_d2::decision::evaluate_feasibility(state, known, registry).status,
             FeasibilityStatus::Feasible, "known route must be feasible");
    // route_missing is not the single-route registry's payload target, so
    // that registry does not bind it (NotApplicable => satisfied). Register
    // a constraint for the requested route to observe the real verdict.
    // P1-4 semantics: absent from a NON-EMPTY known-routes map is a
    // documentation gap, not a confirmed prohibition => Unknown.
    ConstraintRegistry full_registry;
    full_registry.add(make_route_constraint(RouteId{"route_main"}));
    full_registry.add(make_route_constraint(RouteId{"route_missing"}));
    const auto bad_bound =
        denso_d2::decision::evaluate_feasibility(state, unknown_route, full_registry);
    check_eq(bad_bound.status, FeasibilityStatus::Unknown,
             "route absent from non-empty known map must be UNKNOWN, not infeasible");

    // A route recorded as inadmissible (false) is a confirmed violation.
    FactoryState state_rejects = synthetic_state();
    state_rejects.known_routes[RouteId{"route_missing"}] = false;
    const auto rejected = denso_d2::decision::evaluate_feasibility(
        state_rejects, unknown_route, full_registry);
    check_eq(rejected.status, FeasibilityStatus::Infeasible,
             "route recorded as inadmissible must be infeasible");
}

// ---------- P0-1: soft constraint with unknown data never blocks ----------
TEST(soft_unknown_does_not_block) {
    FactoryState state = synthetic_state();  // budget deliberately nullopt
    Constraint soft_budget = make_budget_constraint();
    soft_budget.severity = denso_d2::decision::Severity::Soft;
    ConstraintRegistry registry;
    registry.add(soft_budget);
    const Action action = make_add_transporter(1);
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check_eq(result.status, FeasibilityStatus::Feasible,
             "soft unknown must not force UNKNOWN verdict");
    bool soft_unknown_visible = false;
    for (const auto& evaluation : result.evaluations) {
        if (evaluation.status == ConstraintStatus::Unknown) {
            soft_unknown_visible = true;
        }
    }
    check(soft_unknown_visible, "the soft unknown stays visible in evaluations");
    check(denso_d2::decision::blocking_constraints(result).empty(),
          "soft unknown must never appear as a blocking constraint");
}

// ---------- P0-3: evidence-aware a_0 on state constraints ----------
TEST(no_action_state_violation_is_reported) {
    // Known over-allocated state: count 5 > ceiling 3. The status quo
    // itself violates the count bound; eq:phistatus requires a reported
    // violation even for a_0, not a blanket-satisfied.
    FactoryState violating = synthetic_state();
    violating.resources[ResourceId{"transporter"}].current_count = 5;
    violating.resources[ResourceId{"transporter"}].max_count = 3;
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    const FeasibilityResult result = denso_d2::decision::evaluate_feasibility(
        violating, make_no_action_action(), registry);
    check_eq(result.status, FeasibilityStatus::Infeasible,
             "a_0 on a known violating state must report the violation");
    const auto blocking = denso_d2::decision::blocking_constraints(result);
    check(!blocking.empty() && blocking[0].value == "C-PHY-002-transporter",
          "the blocking constraint must be C-PHY-002-transporter");

    // Unknown ceiling: the delta=0 branch keeps the status quo feasible;
    // unknown change-data never blocks a_0.
    FactoryState unknown_ceiling = synthetic_state();
    unknown_ceiling.resources[ResourceId{"transporter"}].max_count = std::nullopt;
    const FeasibilityResult unknown_result =
        denso_d2::decision::evaluate_feasibility(
            unknown_ceiling, make_no_action_action(), registry);
    check_eq(unknown_result.status, FeasibilityStatus::Feasible,
             "unknown ceiling must not block the status quo");
}

// ---------- P2-20: ReallocateResource family coverage ----------
TEST(reallocate_resource_behavior) {
    // Known-good state: count unchanged (2 <= 3) => feasible.
    const FactoryState state = synthetic_state();
    const Action good{
        ActionId{"reallocate-good"},
        ReallocateResource{ResourceId{"transporter"}, LocationId{"line_a"},
                            LocationId{"line_b"}},
        ClaimStatus::Synthetic,
        synthetic_provenance(),
    };
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    check_eq(denso_d2::decision::evaluate_feasibility(state, good, registry).status,
             FeasibilityStatus::Feasible,
             "reallocation on a known-good state must be feasible");

    // Known violating state (count 5 > ceiling 3): reallocation does not
    // change the count, so the same bound is violated.
    FactoryState violating = synthetic_state();
    violating.resources[ResourceId{"transporter"}].current_count = 5;
    violating.resources[ResourceId{"transporter"}].max_count = 3;
    check_eq(denso_d2::decision::evaluate_feasibility(violating, good, registry).status,
             FeasibilityStatus::Infeasible,
             "reallocation on a violating state must report the violation");
}

// ---------- P2-10: negative AddTemporaryCapacity is domain-invalid ----------
TEST(negative_additional_capacity_rejected) {
    const FactoryState state = synthetic_state();
    const Action action = make_add_transporter(-1);
    ConstraintRegistry registry;
    registry.add(make_resource_count_constraint(ResourceId{"transporter"}));
    const FeasibilityResult result =
        denso_d2::decision::evaluate_feasibility(state, action, registry);
    check_eq(result.status, FeasibilityStatus::Infeasible,
             "negative additional units are a provable violation, never feasible");
}

// ---------- P1-4: dispatch rules absent vs recorded-inadmissible ----------
TEST(dispatch_rule_absent_vs_rejected) {
    const FactoryState state = synthetic_state();
    ConstraintRegistry registry;
    registry.add(make_dispatch_constraint());

    // Rule absent from a non-empty allowed set: documentation gap => Unknown.
    const Action unknown_rule = make_change_priority("unknown-rule");
    check_eq(denso_d2::decision::evaluate_feasibility(state, unknown_rule, registry).status,
             FeasibilityStatus::Unknown,
             "rule absent from non-empty map must be UNKNOWN");

    // Rule recorded as inadmissible: confirmed violation => Infeasible.
    FactoryState state_rejects = synthetic_state();
    state_rejects.allowed_dispatch_rules["unknown-rule"] = false;
    check_eq(denso_d2::decision::evaluate_feasibility(state_rejects, unknown_rule, registry).status,
             FeasibilityStatus::Infeasible,
             "rule recorded as inadmissible must be infeasible");
}

// ---------- P2-14: evaluate_for_no_action=false skips a_0 ----------
TEST(no_action_evaluation_opt_out) {
    // A constraint that opts out of a_0 evaluation: the status quo takes
    // the not-applicable branch instead of being evaluated.
    FactoryState state = synthetic_state();
    Constraint opt_out = make_dispatch_constraint();
    opt_out.evaluate_for_no_action = false;
    ConstraintRegistry registry;
    registry.add(opt_out);
    const FeasibilityResult result = denso_d2::decision::evaluate_feasibility(
        state, make_no_action_action(), registry);
    check_eq(result.status, FeasibilityStatus::Feasible,
             "a_0 must remain feasible when the constraint opts out");
    bool reason_ok = false;
    for (const auto& evaluation : result.evaluations) {
        if (evaluation.constraint_id.value == "C-OPS-003"
            && evaluation.status == ConstraintStatus::Satisfied
            && evaluation.reason
                   == denso_d2::decision::ConstraintReason::NotApplicableToActionFamily) {
            reason_ok = true;
        }
    }
    check(reason_ok, "the opt-out branch must use NotApplicableToActionFamily");
}

}  // namespace

int main() {
    for (const TestCase& test : test_cases()) {
        const int before_failures = checks_failed;
        test.body();
        std::cout << (checks_failed == before_failures ? "PASS " : "FAIL ")
                  << test.name << "\n";
    }
    std::cout << checks_run << " checks, " << checks_failed << " failed\n";
    return checks_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
