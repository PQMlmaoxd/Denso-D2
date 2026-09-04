#pragma once

#include <vector>

#include "denso_d2/decision/constraint.hpp"
#include "denso_d2/decision/ids.hpp"

namespace denso_d2::decision {

// Three-valued constraint status, LaTeX eq:phistatus.
enum class ConstraintStatus {
    Satisfied,  // verified evaluable with g_i(a, s_t) <= 0
    Violated,   // verified evaluable with g_i(a, s_t) > 0
    Unknown,    // data required to evaluate g_i is missing
};

// Structured evaluation of one constraint (LaTeX sec:hardsoft / eq:phistatus). Severity is
// carried so downstream aggregation (verdict, blocking ids) can honor the
// hard/soft distinction: eq:verdict quantifies over the hard set only,
// and soft violations must never be reported as blocking.
struct ConstraintEvaluation {
    ConstraintId constraint_id;
    ConstraintStatus status;
    ConstraintReason reason;
    Severity severity;
};

// Action-level verdict, LaTeX eq:verdict. Aggregation priority:
// violation > unknown > satisfied.
enum class FeasibilityStatus {
    Feasible,    // every required hard constraint verified satisfied
    Infeasible,  // some required hard constraint definitely violated
    Unknown,     // none violated, some required hard constraint unverifiable
};

// Aggregate result. The evaluations vector preserves registry order
// (deterministic; LaTeX sec:notation remark on deterministic external ordering).
struct FeasibilityResult {
    FeasibilityStatus status = FeasibilityStatus::Unknown;
    std::vector<ConstraintEvaluation> evaluations;

    // Convenience queries derived from evaluations; no independently
    // mutable duplicate state.
    [[nodiscard]] bool has_violated() const;
    [[nodiscard]] bool has_unknown() const;
};

// Structured explanation: returns the ids of hard constraints that
// determined the verdict (violated for Infeasible, unknown for Unknown).
[[nodiscard]] std::vector<ConstraintId> blocking_constraints(const FeasibilityResult& result);

// Equality is defined for determinism tests (same inputs => same output).
bool operator==(const ConstraintEvaluation& a, const ConstraintEvaluation& b);
bool operator==(const FeasibilityResult& a, const FeasibilityResult& b);

}  // namespace denso_d2::decision
