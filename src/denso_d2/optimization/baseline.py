"""Action-generation and ranking placeholder owned by Phạm Quang Minh."""

from denso_d2.shared import Action, Recommendation, SimulationResult


def generate_candidate_actions(bottleneck: str) -> list[Action]:
    if bottleneck == "transporter":
        return [
            Action(
                action_id="add-transporter",
                action_type="ADD_RESOURCE",
                target="transporter",
                value=1,
                estimated_cost=10.0,
            ),
            Action(
                action_id="change-priority",
                action_type="CHANGE_PRIORITY",
                target="dispatch",
                value="bottleneck-first",
                estimated_cost=1.0,
            ),
        ]
    return []


def rank_actions(
    actions: list[Action],
    baseline: SimulationResult,
) -> list[Recommendation]:
    # Temporary score used only to validate the integration contract.
    recommendations = []
    for action in actions:
        score = 1.0 / (1.0 + action.estimated_cost)
        recommendations.append(
            Recommendation(
                action=action,
                feasible=True,
                score=score,
                reason=(
                    "Placeholder ranking only. Replace with real KPI and "
                    "constraint-aware decision logic."
                ),
            )
        )

    return sorted(
        recommendations,
        key=lambda item: item.score,
        reverse=True,
    )
