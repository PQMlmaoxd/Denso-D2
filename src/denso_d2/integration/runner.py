"""Minimal end-to-end integration runner owned by Nguyễn Thế Hưng."""

from denso_d2.data import make_synthetic_config
from denso_d2.forecast import mock_forecast
from denso_d2.simulation import run_toy_simulation, detect_bottleneck
from denso_d2.optimization import generate_candidate_actions, rank_actions


def run_vertical_slice() -> dict:
    config = make_synthetic_config()
    forecast = mock_forecast()
    simulation = run_toy_simulation(config, forecast)
    bottleneck = detect_bottleneck(simulation)
    actions = generate_candidate_actions(bottleneck)
    recommendations = rank_actions(actions, simulation)

    return {
        "factory": config.name,
        "forecast_target": forecast.target,
        "bottleneck": bottleneck,
        "simulation": simulation,
        "recommendations": recommendations,
    }
