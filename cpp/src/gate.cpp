#include "denso_d2/decision/gate.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace denso_d2::decision {

void ConstraintRegistry::add(Constraint constraint) {
    if (registry_index_.find(constraint.id) != registry_index_.end()) {
        throw std::invalid_argument("duplicate ConstraintId: " + constraint.id.value);
    }
    registry_index_.emplace(constraint.id, constraints_.size());
    constraints_.push_back(std::move(constraint));
}

const Constraint* ConstraintRegistry::find(const ConstraintId& id) const {
    const auto it = registry_index_.find(id);
    if (it == registry_index_.end()) {
        return nullptr;
    }
    return &constraints_[it->second];
}

// Hard-only semantics: eq:verdict quantifies over the hard set, so
// "violated"/"unknown" queries on a result refer to hard constraints
// that determined (or would determine) the verdict. Soft findings stay
// visible in the evaluations list but never register here.
bool FeasibilityResult::has_violated() const {
    return std::any_of(
        evaluations.begin(), evaluations.end(),
        [](const ConstraintEvaluation& e) {
            return e.severity == Severity::Hard
                && e.status == ConstraintStatus::Violated;
        });
}

bool FeasibilityResult::has_unknown() const {
    return std::any_of(
        evaluations.begin(), evaluations.end(),
        [](const ConstraintEvaluation& e) {
            return e.severity == Severity::Hard
                && e.status == ConstraintStatus::Unknown;
        });
}

std::vector<ConstraintId> blocking_constraints(const FeasibilityResult& result) {
    std::vector<ConstraintId> blocking;
    const ConstraintStatus wanted =
        result.status == FeasibilityStatus::Infeasible ? ConstraintStatus::Violated
                                                       : ConstraintStatus::Unknown;
    if (result.status != FeasibilityStatus::Infeasible
        && result.status != FeasibilityStatus::Unknown) {
        return blocking;
    }
    for (const auto& evaluation : result.evaluations) {
        if (evaluation.severity == Severity::Hard && evaluation.status == wanted) {
            blocking.push_back(evaluation.constraint_id);
        }
    }
    return blocking;
}

bool operator==(const ConstraintEvaluation& a, const ConstraintEvaluation& b) {
    return a.constraint_id == b.constraint_id && a.status == b.status
        && a.reason == b.reason && a.severity == b.severity;
}

bool operator==(const FeasibilityResult& a, const FeasibilityResult& b) {
    return a.status == b.status && a.evaluations == b.evaluations;
}

}  // namespace denso_d2::decision
