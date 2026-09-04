#pragma once

#include <string>
#include <vector>

#include "denso_d2/decision/ids.hpp"
#include "denso_d2/decision/provenance.hpp"

namespace denso_d2::decision {

// Canonical constraint categories (LaTeX sec:constraints).
enum class ConstraintCategory {
    Physical,
    Operational,
    Business,
    DecisionPolicy,
};

// Canonical check layers (LaTeX sec:constraints).
// Static: evaluable from configuration before simulation.
// SimulatorEnforced: an invariant the simulator must maintain; the
//   decision layer may statically detect a violation when the proposed
//   change alone makes satisfaction impossible, but satisfaction can only
//   be confirmed with simulation evidence.
// Dynamic: checked on simulation outputs.
enum class ConstraintCheckLayer {
    Static,
    SimulatorEnforced,
    Dynamic,
};

enum class Severity {
    Hard,
    Soft,
};

// Reason codes attached to constraint evaluations. Core semantics only;
// presentation strings belong to the integration/UI boundary.
enum class ConstraintReason {
    VerifiedSatisfied,
    VerifiedViolated,
    MissingRequiredEvidence,
    NoActionTakesDeltaZeroBranch,
    ViolationProvableFromProposal,
    SimulatorEvidenceSupplied,
    SoftViolationFlagged,
    NotApplicableToActionFamily,
    PolicyRequiresUnknownValue,
};

// Typed constraint payload: what the gate checks and against which
// bounds. Missing evidence is expressed with std::optional on the bound,
// never with magic sentinel values (LaTeX sec:status, C-POL-003).
struct ResourceCountBound {
    ResourceId resource;
};

struct BufferCapacityBound {
    LocationId buffer;
};

struct DispatchCatalogOnly {
    // The catalog guarantees allowed rule names exist; the constraint
    // itself needs no additional parameters.
};

struct RouteKnown {
    RouteId route;
};

struct ReplenishmentIntervalBound {
    MaterialId material;
};

struct BudgetBound {
    // Total implementation cost of selected actions must not exceed the
    // budget. The bound value itself is UNKNOWN pre-tour (C-BUS-001).
};

struct ResponseDeadline {
    // Action lead time vs a known response deadline (C-OPS-007).
};

using ConstraintPayload = std::variant<
    ResourceCountBound,
    BufferCapacityBound,
    DispatchCatalogOnly,
    RouteKnown,
    ReplenishmentIntervalBound,
    BudgetBound,
    ResponseDeadline>;

// One registered constraint. Registration is a deliberate act: the
// caller states category, layer, severity, and payload.
struct Constraint {
    ConstraintId id;
    ConstraintCategory category;
    ConstraintCheckLayer layer;
    Severity severity;
    ConstraintPayload payload;
    Provenance provenance;
    // Whether a_0 (no-action) is evaluated against this constraint at
    // all. State constraints (count bounds, buffer bounds) set this true
    // so an evidenced violating state is reported even for the status
    // quo; purely change-dependent constraints may set it false, in
    // which case a_0 takes the delta=0 branch without evaluation.
    // Change-carrying actions are unaffected: their applicability is
    // derived from the payload, never from this flag.
    bool evaluate_for_no_action = true;
};

}  // namespace denso_d2::decision
