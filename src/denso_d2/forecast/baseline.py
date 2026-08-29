"""Forecast placeholder owned by Đồng Minh Đức.

Replace this mock with real forecasting baselines while keeping the shared
`ForecastResult` contract unless the team explicitly agrees to change it.
"""

from denso_d2.shared import ForecastResult


def mock_forecast() -> ForecastResult:
    return ForecastResult(
        target="jobs_per_hour",
        horizon_min=30,
        p10=[90.0],
        p50=[100.0],
        p90=[115.0],
    )
