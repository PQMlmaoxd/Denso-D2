# DENSO D2 – Logistics Decision Twin

Repository for **D2 – Simulate & Forecast Logistics, Recommend Actions** (DENSO Factory Hacks 2026).
The system predicts logistics state, detects bottlenecks, and recommends feasible actions after
simulation-based evaluation. Before the Factory Tour (2026-09-11), everything runs on clearly
labeled synthetic data; no real DENSO data is used.

> Do not commit DENSO data that is not explicitly allowed to be shared; the shared repository
> contains only synthetic data and clearly shareable content.

## Repository Structure

```text
├── src/denso_d2/          Python package (reference implementation)
│   ├── shared/            shared contracts (FactoryConfig, Action, SimulationResult, ...)
│   ├── data/              schemas, configuration, synthetic data, validation
│   ├── forecast/          forecasting baselines, scenario generation
│   ├── simulation/        logistics simulation, bottleneck detection
│   ├── optimization/      actions, constraints, feasibility, ranking
│   ├── integration/       end-to-end orchestration
│   └── demo.py            vertical-slice demo
├── cpp/                   C++20 decision core (executable specification of feasibility
│                         semantics, action catalog, three-valued constraint logic)
├── docs/
│   ├── decision/modeling/ canonical decision model (LaTeX + PDF)
│   └── factory_tour/      Factory Tour question checklist
├── configs/synthetic/     synthetic example configuration (schema example;
│                         not yet consumed by code)
└── tests/                Python tests
```

## Quick Start

Python (3.11+):

```bash
python -m pip install -e ".[dev]"
python -m denso_d2.demo
pytest
```

C++ decision core (CMake ≥ 3.16, C++20, no external dependencies):

```bash
cmake -S cpp -B cpp-build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp-build --parallel
ctest --test-dir cpp-build --output-on-failure
```

## Architecture

The target pipeline (canonical wording in the decision model, §1):

```text
factory state / forecast → forecast → simulation + bottleneck detection
        → candidate actions → feasibility → simulated KPI outcomes
        → ranking → recommendation
```

The mathematical semantics of the decision layer — three-valued feasibility
(satisfied / violated / unknown), the explicit no-action alternative, the constraint taxonomy,
objective structures, and the simulator–optimizer boundary — are specified normatively in
[`docs/decision/modeling/d2_decision_model.tex`](docs/decision/modeling/d2_decision_model.tex)
(PDF alongside). The C++ code under `cpp/` is the approved executable specification of those
feasibility semantics; its tests double as the semantic reference. Python remains the reference
language for the rest of the pipeline.

The current Python demo is a placeholder vertical slice on synthetic data — it is **not** a model
of the real DENSO factory. The C++ core already implements the Baseline 0–2 decision pipeline
(no-action reference, rule-based candidate generation, greedy evaluation under an explicitly
synthetic preference policy, differential-tested against a brute-force oracle). The next planned
step is an action-capable simulator from the simulation owner so candidates can be evaluated
against a real Digital-Twin boundary instead of the current test double.

## Ownership

Canonical ownership table: see [`AGENTS.md`](AGENTS.md) §2. Summary:

| Area | Primary owner |
|---|---|
| `src/denso_d2/data/` | Nguyễn Quốc Khánh |
| `src/denso_d2/forecast/` | Đồng Minh Đức |
| `src/denso_d2/simulation/` | Đào Văn Đức |
| `src/denso_d2/optimization/` | Phạm Quang Minh |
| `src/denso_d2/integration/` | Nguyễn Thế Hưng |
| `src/denso_d2/shared/` | Khánh + Hưng + affected owners |
| `cpp/` | Phạm Quang Minh |
| `tests/` | module owners + Hưng (E2E) |

Rules: each owner is responsible for their module's logic, tests, and documentation; Hưng owns
integration triage; shared contracts change only with agreement from all affected owners; no
direct commits to `main`.

## Data and Confidentiality

Do not commit internal DENSO data, mentor-provided data, confidential documents, or secrets.
Only commit synthetic data, schemas, and explicitly shareable content. If real operational data
arrives (e.g., Top-10 phase), store it outside Git or follow DENSO's data-handling requirements.
