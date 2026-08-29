"""Shared contracts for the first integration skeleton.

These contracts are intentionally small. They may be revised after the Factory Tour,
but any cross-module change should be coordinated with the affected owners.
"""

from dataclasses import dataclass, field
from typing import Any


@dataclass
class FactoryConfig:
    name: str
    parameters: dict[str, Any] = field(default_factory=dict)


@dataclass
class ForecastResult:
    target: str
    horizon_min: int
    p10: list[float]
    p50: list[float]
    p90: list[float]


@dataclass
class SimulationResult:
    throughput: float
    lead_time: float
    wip: float
    utilization: dict[str, float] = field(default_factory=dict)
    queues: dict[str, float] = field(default_factory=dict)


@dataclass
class Action:
    action_id: str
    action_type: str
    target: str
    value: Any
    estimated_cost: float = 0.0


@dataclass
class Recommendation:
    action: Action
    feasible: bool
    score: float
    reason: str
