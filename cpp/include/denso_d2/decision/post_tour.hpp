#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "denso_d2/decision/baselines.hpp"
#include "denso_d2/decision/logistics_snapshot.hpp"

namespace denso_d2::decision {

// Tour-specific policy values remain caller supplied: the decision core does
// not turn observations into unconfirmed DENSO thresholds.
struct PostTourRules {
    std::optional<int> minimum_battery_percent;
    std::optional<int> decision_minute;
    std::unordered_map<ActionId, int> action_lead_times_min;
    std::optional<int> maximum_projected_shortage_minutes;
    std::optional<int> maximum_projected_task_lateness_minutes;
    std::optional<int> maximum_projected_travel_minutes;
    std::string model_version;
};

struct PostTourSimulationRun {
    SimulationResult outcome;
    ActionSimulationEvidence evidence;
};

class PostTourSimulationEvaluator {
  public:
    virtual ~PostTourSimulationEvaluator() = default;

    [[nodiscard]] virtual PostTourSimulationRun evaluate(
        const LogisticsSnapshot& snapshot,
        const Action& action,
        const ScenarioId& scenario,
        Seed seed,
        const std::string& model_version) const = 0;
};

// Explicitly synthetic total preorder for tests: maximize throughput, then
// minimize lead time, lateness, and WIP. Cost is not compared because its
// unit and known-vs-unknown treatment remain unconfirmed.
class SyntheticPostTourPreference final : public PreferencePolicy {
  public:
    [[nodiscard]] bool at_least_as_preferred(
        const EvaluatedCandidate& a,
        const EvaluatedCandidate& b) const override;
};

[[nodiscard]] std::vector<Action> generate_post_tour_candidates(
    const LogisticsSnapshot& snapshot);

// Baseline 1: the caller supplies the task identified by its upstream risk
// rule. This layer does not invent inventory thresholds or task mappings.
[[nodiscard]] std::optional<Action> generate_post_tour_priority_rule_candidate(
    const LogisticsSnapshot& snapshot,
    const TransportTaskId& risk_task);

[[nodiscard]] FeasibilityResult evaluate_post_tour_pre_feasibility(
    const LogisticsSnapshot& snapshot,
    const Action& action,
    const PostTourRules& rules);

// Throws std::invalid_argument when evidence identity differs from the
// requested run. Stale or cross-candidate evidence is a boundary error.
[[nodiscard]] FeasibilityResult evaluate_post_tour_post_feasibility(
    const LogisticsSnapshot& snapshot,
    const Action& action,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const ActionSimulationEvidence& evidence);

struct PostTourEvaluatedCandidate {
    Action action;
    FeasibilityResult pre_feasibility;
    bool simulated = false;
    SimulationResult outcome{};
    std::optional<FeasibilityResult> post_feasibility;
    std::optional<FeasibilityResult> final_feasibility;
};

struct PostTourBaselineResult {
    std::vector<PostTourEvaluatedCandidate> candidates;
    std::size_t selected = 0;
    SelectionStatus status = SelectionStatus::NoAction;
};

// Baseline 0 is the first entry. Every pre-feasible action is simulated,
// post-gated, then only final-feasible outcomes enter policy comparison.
[[nodiscard]] PostTourBaselineResult run_post_tour_greedy_evaluation(
    const LogisticsSnapshot& snapshot,
    const std::vector<Action>& proposed_actions,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const PostTourSimulationEvaluator& evaluator,
    const PreferencePolicy& policy);

// Tiny-instance selection oracle used for differential tests. It performs a
// full pairwise exhaustive winner check rather than the production scan.
[[nodiscard]] PostTourBaselineResult exhaustive_post_tour_oracle(
    const LogisticsSnapshot& snapshot,
    const std::vector<Action>& proposed_actions,
    const ScenarioId& scenario,
    Seed seed,
    const PostTourRules& rules,
    const PostTourSimulationEvaluator& evaluator,
    const PreferencePolicy& policy);

}  // namespace denso_d2::decision
