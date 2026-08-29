"""Synthetic configuration placeholder owned by Nguyễn Quốc Khánh."""

from denso_d2.shared import FactoryConfig


def make_synthetic_config() -> FactoryConfig:
    return FactoryConfig(
        name="toy-factory",
        parameters={
            "transporter_count": 2,
            "buffer_capacity": 20,
            "jobs_per_hour": 100,
        },
    )
