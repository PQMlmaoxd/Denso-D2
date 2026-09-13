#pragma once

#include <string>
#include <variant>

#include "denso_d2/decision/ids.hpp"
#include "denso_d2/decision/provenance.hpp"

namespace denso_d2::decision {

// Explicit no-action alternative a_0. A domain value in its own right,
// never nullptr or an empty optional (LaTeX sec:generic: a_0 is feasible by
// construction; "no feasible improvement" is a valid outcome).
struct NoAction {
    friend bool operator==(const NoAction&, const NoAction&) = default;
};

// Candidate action families, LaTeX sec:families / sec:catalog. Each carries exactly the
// parameters its family needs; no parameter bags. All families are
// GENERIC/CANDIDATE structure: no DENSO-specific ids or values live here.
struct ReallocateResource {
    ResourceId resource;
    LocationId from;
    LocationId to;
    friend bool operator==(const ReallocateResource&, const ReallocateResource&) = default;
};

struct ChangeDispatchPriority {
    // Categorical rule selection, e.g. fifo, earliest-deadline-first,
    // bottleneck-first. The catalog owns which rule names exist.
    std::string rule;
    friend bool operator==(const ChangeDispatchPriority&, const ChangeDispatchPriority&) = default;
};

struct ChangeRoute {
    ResourceId resource;
    RouteId route;
    friend bool operator==(const ChangeRoute&, const ChangeRoute&) = default;
};

struct AdjustBuffer {
    LocationId buffer;
    // Signed capacity delta; the gate checks it against known bounds.
    int delta_units;
    friend bool operator==(const AdjustBuffer&, const AdjustBuffer&) = default;
};

struct ChangeReplenishmentInterval {
    MaterialId material;
    // Positive replenishment interval in minutes; sign checked by the gate.
    int interval_min;
    friend bool operator==(const ChangeReplenishmentInterval&, const ChangeReplenishmentInterval&) = default;
};

struct AddTemporaryCapacity {
    ResourceId resource;
    // Additional units requested for the current epoch; >= 0 checked by
    // the gate against the known pool ceiling.
    int additional_units;
    friend bool operator==(const AddTemporaryCapacity&, const AddTemporaryCapacity&) = default;
};

struct PrioritizeTransportTask {
    TransportTaskId task;
    friend bool operator==(const PrioritizeTransportTask&, const PrioritizeTransportTask&) = default;
};

struct AssignTransportTask {
    TransportTaskId task;
    AmrId amr;
    friend bool operator==(const AssignTransportTask&, const AssignTransportTask&) = default;
};

struct ReassignTransportTask {
    TransportTaskId task;
    AmrId from_amr;
    AmrId to_amr;
    friend bool operator==(const ReassignTransportTask&, const ReassignTransportTask&) = default;
};

struct SelectAlternateRoute {
    TransportTaskId task;
    RouteId route;
    friend bool operator==(const SelectAlternateRoute&, const SelectAlternateRoute&) = default;
};

// One payload per action instance. std::variant is justified here: the
// family set comes from the canonical spec, each
// family has distinct typed parameters, and exhaustive visitation lets the
// compiler enforce that every family is handled.
using ActionPayload = std::variant<
    NoAction,
    ReallocateResource,
    ChangeDispatchPriority,
    ChangeRoute,
    AdjustBuffer,
    ChangeReplenishmentInterval,
    AddTemporaryCapacity,
    PrioritizeTransportTask,
    AssignTransportTask,
    ReassignTransportTask,
    SelectAlternateRoute>;

// A catalog action: identity, payload, and provenance metadata.
// estimated_cost is deliberately absent pre-tour: its unit is UNKNOWN
// (LaTeX sec:objectives, ordinal-cost fallback), and no ranking consumer
// exists yet. Adding a cost field requires an owner decision.
struct Action {
    ActionId id;
    ActionPayload payload;
    ClaimStatus claim_status;
    Provenance provenance;
};

// True iff the action is the explicit no-action alternative.
[[nodiscard]] inline bool is_no_action(const Action& action) {
    return std::holds_alternative<NoAction>(action.payload);
}

[[nodiscard]] inline bool is_post_tour_action(const Action& action) {
    return std::holds_alternative<PrioritizeTransportTask>(action.payload) ||
           std::holds_alternative<AssignTransportTask>(action.payload) ||
           std::holds_alternative<ReassignTransportTask>(action.payload) ||
           std::holds_alternative<SelectAlternateRoute>(action.payload);
}

}  // namespace denso_d2::decision
