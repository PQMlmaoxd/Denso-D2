#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "denso_d2/decision/ids.hpp"
#include "denso_d2/decision/provenance.hpp"

namespace denso_d2::decision {

// Minimal decision-facing state, LaTeX sec:state on FactoryState scope:
// only what constraints need to evaluate. No event queues, no clocks, no
// forecast internals, no physical evolution — those belong to the
// simulation layer.
//
// Every value is what the decision layer KNOWS. Missing knowledge is
// std::nullopt, never a silent default (LaTeX eq:unknownrule).
struct FactoryState {
    // Known current count and optional pool ceiling per resource
    // (C-PHY-002: 0 <= c_r + delta_r <= c_bar_r; ceiling UNKNOWN pre-tour).
    struct ResourceInfo {
        int current_count = 0;
        std::optional<int> max_count;
        Provenance provenance{SourceType::Synthetic, "", false};
    };
    std::unordered_map<ResourceId, ResourceInfo> resources;

    // Known buffer capacities (C-PHY-003). Absent entry = capacity
    // UNKNOWN; a proposal touching it then evaluates to Unknown.
    std::unordered_map<LocationId, int> buffer_capacities;

    // Allowed dispatch rule names (C-OPS-003 dispatch restrictions);
    // empty = allowed set UNKNOWN.
    std::unordered_map<std::string, bool> allowed_dispatch_rules;

    // Known admissible routes (C-PHY-005 route connectivity).
    std::unordered_map<RouteId, bool> known_routes;

    // Known replenishment interval bounds per material (C-OPS-004):
    // [min, max]. Absent = bounds UNKNOWN.
    struct IntervalBound {
        int min_min = 0;
        std::optional<int> max_min;
    };
    std::unordered_map<MaterialId, IntervalBound> replenishment_bounds;

    // Known action lead times in minutes per action id (C-OPS-007).
    // Absent = lead time UNKNOWN.
    std::unordered_map<ActionId, int> action_lead_times_min;

    // Known response deadline for the current epoch in minutes
    // (C-OPS-007). Absent = no deadline known.
    std::optional<int> response_deadline_min;

    // Known budget for the current epoch (C-BUS-001). Absent = budget
    // UNKNOWN pre-tour.
    std::optional<int> budget;

    // Simulation evidence supplied for the action under evaluation, used
    // by simulator-enforced constraints: capacity outcome after the
    // proposed change (buffer -> content). Absent = no evidence, so
    // simulator-enforced constraints evaluate to Unknown unless a
    // violation is provable from the proposal alone.
    std::unordered_map<LocationId, int> simulated_buffer_content;
};

}  // namespace denso_d2::decision
