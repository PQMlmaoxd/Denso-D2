#pragma once

#include <unordered_map>
#include <vector>

#include "denso_d2/decision/action.hpp"
#include "denso_d2/decision/ids.hpp"

namespace denso_d2::decision {

// Deterministic catalog of candidate actions, LaTeX sec:catalog on
// ActionCatalog scope. Insertion order is preserved for iteration; ids
// are unique; duplicate insertion throws std::invalid_argument because a
// duplicate id is a configuration error, not an infeasibility.
// The explicit no-action entry is added by the constructor, cannot be
// removed (canonical a_0 must always be evaluable), and is unique: a
// second NoAction payload under any id is rejected.
class ActionCatalog {
  public:
    ActionCatalog();

    // Adds a catalog action. Throws std::invalid_argument on duplicate id
    // or on a second NoAction payload.
    void add(Action action);

    [[nodiscard]] const std::vector<Action>& actions() const noexcept { return actions_; }

    // Returns nullptr when the id is absent. The pointer stays valid
    // until the next add().
    [[nodiscard]] const Action* find(const ActionId& id) const;

    [[nodiscard]] static const Action& no_action();

  private:
    std::vector<Action> actions_;
    std::unordered_map<ActionId, std::size_t> index_;
};

}  // namespace denso_d2::decision
