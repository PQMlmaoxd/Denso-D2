# DENSO D2 – Logistics Decision Twin

Starter repository for **D2 – Simulate & Forecast Logistics, Recommend Actions** in DENSO Factory Hacks 2026.

The current goal before the Factory Tour is to build a minimal end-to-end skeleton using synthetic data:

`synthetic data → forecast/mock scenario → simulation → bottleneck detection → action generation → action ranking`

> This repository does **not** contain the team's internal task-allocation Markdown files and must not contain any DENSO data that is not explicitly allowed to be shared.

## Ownership

| Area | Primary owner | Responsibility |
|---|---|---|
| `src/denso_d2/data/` | Nguyễn Quốc Khánh | data schema, configuration, synthetic data, data validation |
| `src/denso_d2/forecast/` | Đồng Minh Đức | forecasting baselines, prediction intervals, scenario generation |
| `src/denso_d2/simulation/` | Đào Văn Đức | logistics simulation, simulation KPIs, bottleneck detection |
| `src/denso_d2/optimization/` | Phạm Quang Minh | actions, constraints, feasibility checks, action ranking |
| `src/denso_d2/integration/` | Nguyễn Thế Hưng | module integration, contract checks, end-to-end workflow |
| `src/denso_d2/shared/` | Khánh + Hưng + affected owners | shared contracts and shared domain types |
| `tests/` | each module owner + Hưng for E2E | module tests and integration tests |

### Responsibility rules

- Each module owner is responsible for the logic, tests, debugging, and documentation of their own module.
- Hưng is responsible for integration, identifying the boundary of integration failures, and rerunning E2E tests after the relevant module owner fixes the issue.
- Shared schemas/contracts must not be changed without agreement from the affected owners.
- Do not commit directly to `main`.

## Repository Structure

```text
denso-d2-starter/
├── .github/
│   ├── ISSUE_TEMPLATE/
│   │   └── task.md
│   ├── workflows/
│   │   └── ci.yml
│   └── pull_request_template.md
│
├── configs/
│   └── synthetic/
│       └── example_factory.json
│
├── src/
│   └── denso_d2/
│       ├── shared/
│       ├── data/
│       ├── forecast/
│       ├── simulation/
│       ├── optimization/
│       ├── integration/
│       └── demo.py
│
├── tests/
├── .gitignore
├── pyproject.toml
└── README.md
```

## Quick Start

Requires Python 3.11+.

```bash
python -m pip install -e ".[dev]"
python -m denso_d2.demo
pytest
```

The current demo only uses synthetic data and very small placeholders to verify the contracts between modules. It is **not** a model of the real DENSO factory.

## Work Before the Factory Tour

### Khánh
- finalize `FactoryConfig` and `LogisticsEvent`;
- implement a synthetic data generator;
- implement data validation;
- align the configuration format with Đào.

### Đồng Minh Đức
- add `naive`, `seasonal naive`, and `moving average` baselines;
- return outputs through `ForecastResult`;
- generate low/base/high scenarios.

### Đào Văn Đức
- replace the simulation placeholder with a toy discrete-event simulation;
- return `SimulationResult`;
- add a simple bottleneck detector.

### Minh
- define generic actions;
- implement a constraint filter;
- implement a KPI-based ranking baseline.

### Hưng
- review and simplify the repository structure if needed;
- revise shared contracts if needed;
- keep CI minimal and useful;
- maintain the E2E runner;
- ensure downstream modules can use mocks/fixtures when upstream modules are unfinished.

## Nearest Technical Milestone

The repository should be able to run:

```text
synthetic config
      ↓
simulation
      ↓
bottleneck detection
      ↓
action generation
      ↓
action ranking
```

The real forecasting module can be integrated afterward. Until then, the E2E flow may use a mock forecast.

## Data and Confidentiality

Do not commit:
- internal DENSO data;
- data provided by DENSO mentors;
- confidential documents;
- API keys, tokens, passwords, or other secrets.

Only commit:
- synthetic data;
- schemas;
- synthetic/example configurations;
- content that is explicitly safe to share.

If the team reaches Top 10 and receives real operational data, store it outside Git or follow DENSO's specific data-handling requirements.

## Coding Agent Guidelines

Coding agents may assist with:
- boilerplate;
- tests;
- documentation;
- API clients;
- small refactors.

Coding agents must **not** independently decide:
- KPIs;
- business assumptions;
- simulation semantics;
- optimization formulations;
- experimental claims;
- final system architecture.

All agent-generated code must be reviewed and tested by the relevant module owner before merge.
