#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "action.hpp"
#include "simulation_result.hpp"

namespace denso_d2::decision {

enum class TaskPriority { Normal, Urgent };

enum class AmrStatus { Available, Busy, Charging, Unavailable };

// Decision-facing task data. Ingestion, normalization, and physical task
// evolution remain owned by the data and simulation modules.
struct TransportTaskSnapshot {
    TransportTaskId id;
    LocationId pickup;
    LocationId destination;
    int quantity;
    std::optional<int> release_minute;
    std::optional<int> due_minute;
    TaskPriority priority;
    std::optional<AmrId> assigned_amr;
    std::optional<std::string> required_resource_type;
    std::optional<RouteId> current_route;
};

struct AmrSnapshot {
    AmrId id;
    LocationId location;
    AmrStatus status;
    std::optional<int> battery_percent;
    std::optional<int> payload_capacity;
    std::optional<std::string> resource_type;
};

struct RouteSnapshot {
    RouteId id;
    LocationId origin;
    LocationId destination;
    std::optional<int> expected_travel_minutes;
    bool available;
};

struct LogisticsSnapshot {
    std::string snapshot_id;
    std::uint64_t version;
    std::vector<TransportTaskSnapshot> transport_tasks;
    std::vector<AmrSnapshot> amrs;
    std::vector<RouteSnapshot> routes;
};

// Evidence belongs to one action evaluation, not to the epoch snapshot.
// Identity mismatches must be rejected by the post-simulation gate.
struct ActionEvidenceIdentity {
    std::string snapshot_id;
    std::uint64_t snapshot_version;
    ActionId action_id;
    ActionPayload action_payload;
    ScenarioId scenario;
    Seed seed;
    std::string model_version;

    friend bool operator==(const ActionEvidenceIdentity&, const ActionEvidenceIdentity&) = default;
};

struct ActionSimulationEvidence {
    ActionEvidenceIdentity identity;
    std::optional<int> projected_shortage_minutes;
    std::optional<int> projected_task_lateness_minutes;
    std::optional<int> projected_travel_minutes;
};

}  // namespace denso_d2::decision
