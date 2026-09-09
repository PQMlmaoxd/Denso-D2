from copy import deepcopy
import math

import pytest

from denso_d2.shared import FactoryConfig, ForecastResult
from denso_d2.simulation.readiness import ProbeScenario, run_readiness_probe


def inputs():
    return (
        FactoryConfig("synthetic-probe", {"transporter_count": 2, "buffer_capacity": 20}),
        ForecastResult("jobs_per_hour", 30, [90.0], [100.0], [115.0]),
    )


def test_probe_preserves_inputs_and_is_replayable():
    config, forecast = inputs()
    scenarios = [ProbeScenario("demand_20", 1.2), ProbeScenario("resource_1", parameter_overrides={"transporter_count": 1})]
    before = deepcopy((config, forecast, scenarios))
    first = run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=scenarios)
    assert first == run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=scenarios)
    assert (config, forecast, scenarios) == before
    assert all(run["identical_input_replay_equal"] for run in first["runs"])
    assert first["seed_supported"] is False
    assert all(run["seed"] is None for run in first["runs"])


def test_probe_reports_formula_deltas_without_claiming_physical_effects():
    config, forecast = inputs()
    report = run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=[
        ProbeScenario("demand_20", 1.2),
        ProbeScenario("resource_1", parameter_overrides={"transporter_count": 1}),
        ProbeScenario("buffer_1", parameter_overrides={"buffer_capacity": 1}),
    ])
    base, demand, resource, buffer = report["runs"]
    assert base["kpis"]["throughput"] == pytest.approx(100.0)
    assert demand["delta_vs_base"]["throughput"] == pytest.approx(20.0)
    assert demand["delta_vs_base"]["lead_time"] == pytest.approx(3.0)
    assert resource["delta_vs_base"]["throughput"] == pytest.approx(-5.0)
    assert buffer["output_changed_vs_base"] is False
    assert "not a DES" in report["warning"]


@pytest.mark.parametrize("value", [-1, math.nan, math.inf, True, "1.2"])
def test_probe_rejects_invalid_demand_multiplier(value):
    config, forecast = inputs()
    with pytest.raises(ValueError, match="demand_multiplier"):
        run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=[ProbeScenario("invalid", value)])


@pytest.mark.parametrize("count", [0, -1, 1.5, True, "2"])
def test_probe_rejects_transporter_values_that_legacy_code_would_coerce(count):
    config, forecast = inputs()
    with pytest.raises(ValueError, match="transporter_count"):
        run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=[
            ProbeScenario("invalid", parameter_overrides={"transporter_count": count})
        ])


def test_probe_rejects_duplicate_and_reserved_scenario_ids():
    config, forecast = inputs()
    for scenarios in ([ProbeScenario("base")], [ProbeScenario("same"), ProbeScenario("same")]):
        with pytest.raises(ValueError, match="scenario_id"):
            run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=scenarios)


def test_probe_rejects_multistep_forecast_legacy_simulator_would_ignore():
    config, forecast = inputs()
    forecast.p50 = [100, 120]
    with pytest.raises(ValueError, match="exactly one"):
        run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=[])


def test_probe_rejects_quantile_order_and_overflow():
    config, forecast = inputs()
    forecast.p10 = [200]
    with pytest.raises(ValueError, match="quantiles"):
        run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=[])
    config, forecast = inputs()
    with pytest.raises(ValueError, match="finite"):
        run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=[ProbeScenario("overflow", 1e308)])


@pytest.mark.parametrize("source", ["observed", "estimated", None])
def test_probe_rejects_source_that_is_not_explicitly_synthetic(source):
    config, forecast = inputs()
    with pytest.raises(ValueError, match="explicitly synthetic"):
        run_readiness_probe(source=source, config=config, forecast=forecast, scenarios=[])


@pytest.mark.parametrize("field", ["p10", "p50", "p90", "transporter_count"])
def test_probe_rejects_numbers_that_cannot_be_represented_by_legacy_engine(field):
    config, forecast = inputs()
    if field == "transporter_count":
        config.parameters[field] = 10**400
    else:
        setattr(forecast, field, [10**400])
    with pytest.raises(ValueError, match=field):
        run_readiness_probe(source="synthetic", config=config, forecast=forecast, scenarios=[])
