# Simulation Module

Owner: **Đào Văn Đức**

Responsibilities:
- toy discrete-event simulation before the Factory Tour;
- later DENSO-specific logistics simulation;
- simulation KPIs;
- bottleneck detection;
- what-if scenarios.

The current implementation is only a placeholder for integration.

## Legacy simulator readiness probe

The current `toy.py` is a deterministic formula placeholder. A passing demo does
not verify events, finite buffers, resource contention, or physical KPI meaning.

From the repository root, after the installation in the root README:

```bash
python -m denso_d2.simulation.readiness --input configs/synthetic/simulation_readiness_probe.json --output outputs/simulation_readiness.json
python -m pytest tests/simulation -q
```

The report preserves the complete effective input per scenario, raw KPI deltas,
and identical-input replay checks. It includes the base case and eight changes.
It explicitly records that this engine has no seed/action input and unconfirmed
KPI units. The time fields in the fixture are diagnostic probes, not a new data
schema. Demand changes scale all three mock forecast quantiles; this is a stress
input, not a trained forecast. No-effect cases require source inspection and
cannot establish real-world ineffectiveness.

This tool does not implement the planned DES or optimizer adapter. Its validation
only protects this single-value synthetic probe; it is not factory-data
validation. See `docs/simulation/des_mvp_decisions.md` for owner decisions and
acceptance cases before DES implementation. Remove or migrate this diagnostic
when the legacy function is replaced; its metadata intentionally names that engine.
