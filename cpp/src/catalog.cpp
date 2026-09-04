#include "denso_d2/decision/catalog.hpp"

#include <stdexcept>
#include <utility>
#include <variant>

namespace denso_d2::decision {

namespace {

// The canonical no-action alternative: a domain value with a fixed id
// and structural provenance (it is a modeling convention, not factory
// data).
Action make_no_action() {
    return Action{
        ActionId{"no-action"},
        NoAction{},
        ClaimStatus::Generic,
        Provenance{SourceType::Synthetic, "explicit no-action alternative a_0", false},
    };
}

}  // namespace

ActionCatalog::ActionCatalog() {
    // Register a_0 directly: add() rejects NoAction payloads, so the
    // constructor is the single place that can create the entry.
    const Action a0 = make_no_action();
    index_.emplace(a0.id, actions_.size());
    actions_.push_back(a0);
}

void ActionCatalog::add(Action action) {
    if (index_.find(action.id) != index_.end()) {
        throw std::invalid_argument("duplicate ActionId: " + action.id.value);
    }
    if (std::holds_alternative<NoAction>(action.payload)) {
        // a_0 is a domain singleton: the constructor registers the one
        // canonical entry; accepting a second NoAction payload under a
        // different id would create two no-action alternatives.
        throw std::invalid_argument(
            "duplicate NoAction payload: a_0 is already registered");
    }
    index_.emplace(action.id, actions_.size());
    actions_.push_back(std::move(action));
}

const Action* ActionCatalog::find(const ActionId& id) const {
    const auto it = index_.find(id);
    if (it == index_.end()) {
        return nullptr;
    }
    return &actions_[it->second];
}

const Action& ActionCatalog::no_action() {
    static const Action fixed = make_no_action();
    return fixed;
}

}  // namespace denso_d2::decision
