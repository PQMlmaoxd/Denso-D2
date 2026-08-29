"""Simulation placeholder owned by Đào Văn Đức.

This is not a real discrete-event simulation. It only provides deterministic
outputs so the team can test integration before the simulation module is ready.
"""

from denso_d2.shared import FactoryConfig, ForecastResult, SimulationResult


def run_toy_simulation(
    config: FactoryConfig,
    forecast: ForecastResult,
) -> SimulationResult:
    demand = forecast.p50[0]
    transporter_count = max(
        int(config.parameters.get("transporter_count", 1)),
        1,
    )

    # Temporary deterministic formula used only to keep the E2E flow working.
    utilization = min(demand / (60.0 * transporter_count), 1.0)
    lead_time = 30.0 + max(0.0, demand - 80.0) * 0.15
    throughput = min(demand, 95.0 * transporter_count)
    wip = max(0.0, demand - throughput) * 0.5

    return SimulationResult(
        throughput=throughput,
        lead_time=lead_time,
        wip=wip,
        utilization={"transporter": utilization},
        queues={"buffer_a": wip},
    )


def detect_bottleneck(result: SimulationResult) -> str:
    if result.utilization.get("transporter", 0.0) >= 0.8:
        return "transporter"
    if result.queues.get("buffer_a", 0.0) >= 10:
        return "buffer_a"
    return "none"
