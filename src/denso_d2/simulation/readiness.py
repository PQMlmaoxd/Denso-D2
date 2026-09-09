"""Probe the legacy formula placeholder without declaring DES readiness.

This diagnostic preserves the existing simulator and shared contracts. Repeated
formula evaluations are replay checks, not stochastic replications.
"""

from __future__ import annotations

import argparse
from copy import deepcopy
from dataclasses import asdict, dataclass, field
import json
import math
from pathlib import Path
from typing import Any

from denso_d2.shared import FactoryConfig, ForecastResult, SimulationResult
from denso_d2.simulation.toy import run_toy_simulation


@dataclass
class ProbeScenario:
    scenario_id: str
    demand_multiplier: float = 1.0
    parameter_overrides: dict[str, Any] = field(default_factory=dict)


def _finite_nonnegative(value: Any, *, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{name} must be a finite nonnegative number")
    try:
        finite = math.isfinite(value)
    except OverflowError:
        finite = False
    if not finite or value < 0:
        raise ValueError(f"{name} must be a finite nonnegative number")


def _validate_inputs(config: FactoryConfig, forecast: ForecastResult) -> None:
    if not isinstance(config.name, str) or not config.name.strip():
        raise ValueError("config.name must be nonempty")
    if not isinstance(config.parameters, dict):
        raise ValueError("config.parameters must be an object")
    count = config.parameters.get("transporter_count")
    if type(count) is not int or count < 1:
        raise ValueError("transporter_count must be an explicit positive integer")
    _finite_nonnegative(count, name="transporter_count")
    if forecast.target != "jobs_per_hour":
        raise ValueError("This legacy probe only supports the jobs_per_hour fixture")
    if type(forecast.horizon_min) is not int or forecast.horizon_min <= 0:
        raise ValueError("horizon_min must be a positive integer")
    for name in ("p10", "p50", "p90"):
        values = getattr(forecast, name)
        if not isinstance(values, list) or len(values) != 1:
            raise ValueError(f"{name} must contain exactly one value for the legacy probe")
        _finite_nonnegative(values[0], name=name)
    if not forecast.p10[0] <= forecast.p50[0] <= forecast.p90[0]:
        raise ValueError("Forecast quantiles must satisfy p10 <= p50 <= p90")


def _flatten(result: SimulationResult) -> dict[str, float]:
    values = {
        "throughput": result.throughput,
        "lead_time": result.lead_time,
        "wip": result.wip,
        **{f"utilization.{key}": value for key, value in result.utilization.items()},
        **{f"queues.{key}": value for key, value in result.queues.items()},
    }
    for key, value in values.items():
        _finite_nonnegative(value, name=f"output.{key}")
    return values


def run_readiness_probe(
    *,
    source: str,
    config: FactoryConfig,
    forecast: ForecastResult,
    scenarios: list[ProbeScenario],
) -> dict[str, Any]:
    """Return raw KPI deltas and replay evidence from synthetic formula inputs.

    An unchanged output means only that this input pair gave identical results.
    It is not, by itself, proof that a parameter is ignored or a real action is
    ineffective. No units, feasibility verdicts, or rankings are inferred.
    """
    if source != "synthetic":
        raise ValueError("This readiness probe accepts explicitly synthetic fixtures only")
    _validate_inputs(config, forecast)
    seen = {"base"}
    for scenario in scenarios:
        if not isinstance(scenario.scenario_id, str) or not scenario.scenario_id.strip():
            raise ValueError("scenario_id must be nonempty")
        if scenario.scenario_id in seen:
            raise ValueError(f"Duplicate or reserved scenario_id: {scenario.scenario_id}")
        seen.add(scenario.scenario_id)
        _finite_nonnegative(scenario.demand_multiplier, name="demand_multiplier")
        if not isinstance(scenario.parameter_overrides, dict):
            raise ValueError("parameter_overrides must be an object")

    runs = []
    baseline_values: dict[str, float] | None = None
    for scenario in [ProbeScenario("base"), *scenarios]:
        changed_config = deepcopy(config)
        changed_config.parameters.update(deepcopy(scenario.parameter_overrides))
        changed_forecast = deepcopy(forecast)
        for name in ("p10", "p50", "p90"):
            setattr(
                changed_forecast,
                name,
                [value * scenario.demand_multiplier for value in getattr(forecast, name)],
            )
        _validate_inputs(changed_config, changed_forecast)
        first = run_toy_simulation(deepcopy(changed_config), deepcopy(changed_forecast))
        replay = run_toy_simulation(deepcopy(changed_config), deepcopy(changed_forecast))
        values = _flatten(first)
        if baseline_values is None:
            baseline_values = values
        runs.append({
            "scenario_id": scenario.scenario_id,
            "seed": None,
            "effective_config": asdict(changed_config),
            "effective_forecast": asdict(changed_forecast),
            "kpis": asdict(first),
            "delta_vs_base": {key: value - baseline_values[key] for key, value in values.items()},
            "output_changed_vs_base": values != baseline_values,
            "identical_input_replay_equal": first == replay,
        })

    return {
        "schema_version": "legacy-probe-v1",
        "engine": "denso_d2.simulation.toy.run_toy_simulation",
        "engine_kind": "deterministic_formula_placeholder",
        "source": source,
        "seed_supported": False,
        "action_input_supported": False,
        "kpi_units": "unconfirmed; do not use as measured factory KPIs",
        "warning": "Diagnostic formula outputs only; this is not a DES or DENSO performance evidence.",
        "runs": runs,
    }


def main() -> None:
    """Read a probe fixture and write an inspectable JSON report."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        request = json.loads(args.input.read_text(encoding="utf-8"))
        report = run_readiness_probe(
            source=request["source"],
            config=FactoryConfig(**request["config"]),
            forecast=ForecastResult(**request["forecast"]),
            scenarios=[ProbeScenario(**item) for item in request["scenarios"]],
        )
        payload = json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(payload, encoding="utf-8")
        else:
            print(payload, end="")
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
