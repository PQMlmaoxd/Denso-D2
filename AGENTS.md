# AGENTS.md

This file defines how coding agents should work in this repository.

The project is a hackathon/research prototype for **DENSO Factory Hacks 2026 – D2: Simulate & Forecast Logistics, Recommend Actions**. The goal is to build a reliable end-to-end decision workflow, not a general-purpose manufacturing framework.

The current target flow is:

```text
factory configuration / event data
            ↓
         forecast
            ↓
        simulation
            ↓
   bottleneck detection
            ↓
     candidate actions
            ↓
   feasibility + KPI evaluation
            ↓
      recommendation
```

Before the Factory Tour, all DENSO-specific values are synthetic or explicitly marked assumptions. Never invent factory facts and present them as real data.

---

## 1. Repository Ownership

| Area | Primary owner | Scope |
|---|---|---|
| `src/denso_d2/data/` | Nguyễn Quốc Khánh | schemas, configuration, synthetic data, validation, provenance |
| `src/denso_d2/forecast/` | Đồng Minh Đức | forecasting baselines, uncertainty, scenario generation |
| `src/denso_d2/simulation/` | Đào Văn Đức | discrete-event simulation, simulation KPIs, bottleneck logic |
| `src/denso_d2/optimization/` | Phạm Quang Minh | actions, constraints, feasibility, ranking/optimization |
| `src/denso_d2/integration/` | Nguyễn Thế Hưng | orchestration, shared-contract integration, E2E checks |
| `src/denso_d2/shared/` | Khánh + Hưng + affected owners | cross-module contracts and domain types |
| `tests/` | corresponding module owner | unit/module tests |
| `tests/integration/` | Hưng + affected owners | cross-module and E2E tests |

Ownership matters.

- Do not silently redesign another owner's domain logic.
- When an integration failure is inside another module, produce a reproducible input, expected output, actual output, and relevant logs; then return the issue to that module owner.
- Hưng owns integration and failure triage, not the internal logic of every module.
- A shared-contract change requires review from every affected module owner.
- Agents do not become temporary owners merely because they generated the code.

---

## 2. Engineering Priorities

Use this priority order when trade-offs appear:

1. **Correct domain behavior**
2. **Reproducibility**
3. **A working end-to-end path**
4. **Clear module boundaries**
5. **Tests for important behavior**
6. **Simple maintainable code**
7. Performance optimization
8. Extra abstraction or infrastructure

This is a hackathon project. Prefer the smallest design that makes the current requirement correct and testable.

Do not add infrastructure merely because it is common in production systems.

Avoid introducing, unless explicitly approved:

- microservices;
- message brokers;
- Kubernetes;
- a database when files/in-memory structures are enough;
- distributed orchestration;
- unnecessary web APIs between Python modules;
- complex dependency-injection frameworks;
- a second configuration system;
- heavy ML models before baselines exist.

A normal Python package call is preferred over an HTTP call when both modules run in the same process.

---

## 3. Dependency Direction

Keep dependencies one-way.

```text
shared
  ↑
  ├── data
  ├── forecast
  ├── simulation
  └── optimization
          ↑
      integration
          ↑
         demo
```

More precisely:

- `shared/` must not import project-specific modules.
- `data/`, `forecast/`, `simulation/`, and `optimization/` may import from `shared/`.
- `forecast/` may consume stable data-layer outputs, but must not depend on integration/UI code.
- `optimization/` should not directly own or reimplement simulation.
- `simulation/` must not know about the UI.
- `integration/` is the orchestration layer and may import all core modules.
- `demo.py` should call the integration layer rather than reconstruct the pipeline itself.

Avoid circular imports. If two modules need the same type, move the minimal contract to `shared/`.

If a dependency cycle appears, do not patch around it with local imports or duplicate types without first classifying the dependency and deciding which direction should own the shared concept.

---

## 4. Shared Contracts

Current shared concepts include:

- `FactoryConfig`
- logistics/event records
- `ForecastResult`
- `SimulationResult`
- `Action`
- `Recommendation`

Treat these as module boundaries, not as dumping grounds.

When changing a shared contract:

1. Identify all consumers.
2. Update or add tests first when practical.
3. Keep the change minimal.
4. Update all affected call sites in the same PR, or coordinate dependent PRs explicitly.
5. Document semantic changes, not only field-name changes.
6. Get approval from the affected owners.

Do not add a field because it "might be useful later."

Prefer explicit typed fields once the domain meaning is understood. Temporary `dict[str, Any]` structures are acceptable during early exploration but should not spread through the codebase.

For a refactor or replacement of an existing module, preserve the existing external contract and behavior first. Refactor internals only after the behavior is covered by tests. Do not combine a broad architectural cleanup with a behavior change unless the owners explicitly approve it.

---

## 5. Python Code Style

Use English for code, identifiers, docstrings, comments, logs, issue titles, and PR titles.

Follow PEP 8 and these project rules:

- 4-space indentation.
- `snake_case` for functions, methods, files, and variables.
- `PascalCase` for classes and dataclasses.
- `UPPER_SNAKE_CASE` for real constants.
- Prefer double quotes for strings.
- Use type hints for public functions, public methods, dataclasses, and shared contracts.
- Prefer `pathlib.Path` over manual path-string manipulation.
- Prefer standard-library types such as `list[str]`, `dict[str, float]`, and `X | None`.
- Avoid wildcard imports.
- Avoid mutable default arguments.
- Avoid module-level mutable state.
- Prefer keyword-only arguments for domain functions/builders with several parameters.
- Keep functions focused; split a function when it mixes unrelated responsibilities.
- Comments should explain **why** a non-obvious choice exists, not narrate obvious code.
- Public classes/functions with non-trivial behavior should have concise docstrings.
- Library/module code should use `logging`; reserve `print()` for demos/CLI output.

A long comment is not a substitute for correct code. If a workaround needs a paragraph explaining why it is supposedly safe, stop and reconsider the implementation. Long comments are acceptable when they document real domain semantics, mathematical assumptions, or a non-obvious external constraint.

When a formatter/linter is configured in `pyproject.toml`, its output is authoritative. Do not fight the formatter.

Do not introduce a new formatter, type checker, or package manager without maintainer approval.

---

## 6. Configuration and Domain Values

Never hard-code DENSO-specific assumptions inside algorithms.

Bad:

```python
FORKLIFT_COUNT = 3
TRAVEL_TIME = 5
```

Preferred:

```python
config.resources["forklift"].count
config.routes["A_B"].travel_time
```

Before validated DENSO data exists, parameters must be synthetic or carry provenance such as:

```text
observed
estimated
assumed
```

Where relevant, also retain:

- unit;
- confidence;
- source/note;
- whether mentor validation is needed.

Changing a config value should not require rewriting the simulation engine.

---

## 7. Randomness and Reproducibility

Simulation and forecasting experiments must be reproducible.

Rules:

- Every stochastic run must have an explicit seed.
- Prefer a local RNG object (for example `random.Random(seed)` or a NumPy generator) over hidden global random state.
- Do not generate random seeds from wall-clock time in reproducible experiments.
- Same config + same seed should produce the same result unless a documented dependency prevents it.
- Multi-run experiments must preserve the mapping between seed and result.
- Experiment output must record the configuration and seeds used.

For stochastic comparisons, do not make a claim from one run. Run multiple seeds and summarize the distribution when the experiment is intended as evidence.

---

## 8. Data Module Rules

`data/` owns the meaning and validation of input data.

Agents working here should:

- define canonical event/config structures;
- keep parsing/cleaning separate from downstream ML/simulation logic;
- validate required fields at module boundaries;
- fail clearly on impossible records rather than silently repairing them;
- keep provenance for observed/estimated/assumed parameters;
- create small deterministic synthetic fixtures for other modules;
- avoid leaking file-format details into every downstream module.

Do not make forecasting or simulation modules parse arbitrary raw CSV layouts directly if the data layer can normalize them first.

---

## 9. Forecast Module Rules

Start with baselines before complex models.

Expected progression:

1. naive baseline;
2. seasonal naive / moving average;
3. simple statistical or tree-based model;
4. more complex model only if justified by evidence.

A forecasting change should state:

- target;
- horizon;
- input features;
- train/validation/test split;
- metric;
- baseline;
- uncertainty output if applicable.

Prevent data leakage.

Forecast output should be usable by the simulation layer without needing model-specific knowledge.

Do not add a deep-learning model merely because it is more sophisticated.

---

## 10. Simulation Module Rules

The simulation module represents system behavior, not business recommendation logic.

Prefer small composable concepts such as:

- source;
- station/server;
- buffer/queue;
- transport resource;
- route;
- job/material;
- event;
- resource capacity.

Build the simplest model that captures the flow being studied.

Important rules:

- Simulation state must come from config/state inputs rather than hidden globals.
- Separate model construction from experiment execution.
- Keep event generation/logging explicit enough to inspect what happened.
- Return structured KPI/results rather than printing them inside the simulator.
- Maintain capacity, queue, timing, and routing invariants.
- Validate that scenario changes produce physically/logically plausible effects.
- Avoid premature C++/parallel optimization. Profile first.

For stochastic simulation, support repeated runs through a runner/builder pattern rather than duplicating setup code in experiment scripts.

---

## 11. Optimization and Recommendation Rules

`optimization/` owns decision logic, not factory-state reconstruction.

Keep these concepts separate:

1. candidate action generation;
2. feasibility/constraint checking;
3. KPI evaluation;
4. action ranking or optimization.

An LLM must never invent an operational constraint and encode it as DENSO truth.

Actions must be traceable to:

- a known generic action used only for synthetic development; or
- a Factory Tour / mentor-confirmed operational action.

Feasibility violations should be explicit.

Do not hide constraint violations by assigning a poor score; mark an action infeasible unless the model intentionally treats the constraint as soft.

The preferred closed loop is:

```text
generate candidates
      ↓
filter infeasible
      ↓
simulate/evaluate candidates
      ↓
compare KPI distributions
      ↓
rank
```

Do not reimplement the simulator inside the optimization module.

---

## 12. Integration Rules

`integration/` coordinates modules and is the only layer that should routinely know the whole pipeline.

Integration code should:

- depend on contracts, not private internals;
- provide mocks/fixtures when an upstream module is unfinished;
- preserve enough logging/context to reproduce E2E failures;
- keep orchestration readable;
- avoid embedding domain algorithms that belong to module owners.

When E2E fails:

1. reproduce the failure;
2. identify the failing boundary;
3. capture input/config/seed;
4. identify expected vs actual output;
5. assign the fix to the correct owner;
6. rerun E2E after the fix.

Do not "temporarily" patch another module's semantics inside integration code.

---

## 13. Tests Are the Behavioral Contract

Every meaningful behavior change should have a test at the lowest useful level.

Suggested layout as the repository grows:

```text
tests/
├── data/
├── forecast/
├── simulation/
├── optimization/
└── integration/
```

Test naming:

```text
test_<behavior>_<condition>()
```

Prefer behavior-focused tests over implementation-detail tests.

Important test categories:

### Shared/data
- schema construction;
- validation failures;
- provenance preservation;
- config parsing.

### Forecast
- deterministic baseline outputs;
- split/shape correctness;
- metric calculation;
- scenario generation.

### Simulation
- same seed → same result;
- different scenarios produce expected directional changes;
- capacities are never exceeded;
- queues/WIP never become invalid;
- event ordering is valid;
- empty/minimal configurations fail clearly or behave as specified.

### Optimization
- feasible action accepted;
- hard constraint violation rejected;
- ranking changes when KPI trade-offs change;
- no-action case handled cleanly.

### Integration
- vertical slice completes;
- contracts line up;
- mock upstream modules can unblock downstream modules;
- config + seed are propagated correctly.

Use `pytest.approx` or another explicit tolerance for floating-point assertions when exact equality is not semantically required.

Tests must not require Internet access or confidential DENSO data.

### Test integrity rules

Agents must not make the test suite green by:

- deleting a failing existing test;
- skipping or `xfail`-ing an existing test;
- weakening an assertion;
- replacing a behavioral test with a smoke test;
- changing expected values merely to match the new output;
- mocking away the behavior under test.

Any such change requires explicit approval from the relevant human owner and must explain why the old expectation was wrong.

A smoke test is a milestone, not proof of correctness.

After running tests, verify that the intended tests actually executed and were not silently deselected or skipped.

---

## 14. Experiments vs Production Logic

Reusable logic belongs in `src/denso_d2/`.

Do not make notebooks or one-off experiment scripts the canonical implementation.

When experiment folders are introduced, use:

```text
experiments/
├── forecast/
├── simulation/
└── decision/
```

Experiment scripts should:

- import reusable code from `src/`;
- load config instead of duplicating constants;
- set explicit seeds;
- save results outside source directories;
- record enough metadata to rerun the experiment.

Notebook use is acceptable for exploration/visualization. Once logic is used by the system or by repeated experiments, move it into `src/`.

Generated outputs, large artifacts, local datasets, and private data must stay out of Git.

---

## 15. Logging and Observability

Use module-level loggers:

```python
import logging

logger = logging.getLogger(__name__)
```

Useful run-level context includes:

- config/scenario name;
- seed;
- forecast horizon;
- selected action;
- simulation runtime when relevant.

Do not spam per-event logs by default. Detailed event logs should be opt-in or stored as structured artifacts.

Errors should state what failed and which input/config caused it.

Machine-readable failures are valuable work items. Preserve enough context so failures can be grouped by module/file/contract instead of fixed randomly.

---

## 16. Git and Parallel Agent Safety

Do not commit directly to `main`.

Recommended branch prefixes:

```text
feat/<issue>-short-name
fix/<issue>-short-name
exp/<issue>-short-name
docs/<issue>-short-name
```

### Parallel work

When multiple agents are working concurrently:

- use separate branches or worktrees for independent implementation streams;
- avoid assigning overlapping files to parallel implementers;
- one agent should own one bounded change at a time;
- integrate only after each bounded change has passed its review gate;
- prefer a few isolated worktrees over many agents sharing one working directory.

Agents must not run destructive or history-rewriting Git commands unless a human explicitly asks for that exact operation.

Forbidden by default:

```text
git reset --hard
git clean -fd / -fdx
git stash
git stash pop
git rebase
git push --force
git push --force-with-lease
git commit --amend
```

Agents must not switch another worker's branch, delete worktrees, or discard uncommitted changes they did not create.

Do not use Git as an implicit synchronization mechanism between parallel agents.

### Pull requests

Keep PRs small and focused.

Prefer Conventional Commit-style PR titles because the repository may use squash merging:

```text
feat(simulation): add finite-capacity buffer
fix(forecast): prevent train-test leakage
test(optimization): cover infeasible action
docs: clarify synthetic-data policy
```

A PR should include:

- related issue;
- what changed;
- why;
- how to test;
- shared-contract changes;
- agent usage if relevant;
- known limitations.

Cross-module PRs require review from all affected owners.

Do not mix an unrelated refactor, a behavior change, and documentation cleanup into one large PR unless they cannot be separated safely.

---

## 17. Agent Execution Workflow

Coding agents should work in explicit loops rather than receiving broad prompts such as "implement the feature" or "make the project work."

The default loop is:

```text
task/spec
   ↓
plan
   ↓
implement
   ↓
automated checks
   ↓
independent review
   ↓
fix review findings
   ↓
human/module-owner review
   ↓
merge
```

### Step 1 — Read the task as a contract

Before editing code, identify:

- goal;
- files/modules in scope;
- input;
- expected output/behavior;
- Definition of Done;
- dependencies;
- owner;
- forbidden assumptions.

If these are unclear, stop before making a broad implementation.

### Step 2 — Write a short plan

For a non-trivial task, state:

- what will change;
- what will not change;
- contracts touched;
- tests to add/run;
- risks.

Do not expand scope without approval.

### Step 3 — De-risk large work with a trial slice

Before scaling a repeated or high-volume change across many files/modules:

1. implement one representative slice;
2. run the relevant checks;
3. review the result;
4. fix the workflow;
5. only then repeat it.

Do not launch a large agent batch based on an unvalidated prompt.

### Step 4 — Implement the smallest complete behavior

Do not create placeholders merely to silence lint/type/test errors.

Stubs such as `pass`, unconditional dummy returns, fake constant outputs, or broad `except Exception` blocks are not acceptable unless the task explicitly calls for a temporary mock and the mock is clearly isolated/labeled.

### Step 5 — Use deterministic failures as a work queue

When there are many failures:

- capture the full output once;
- group failures by module/file/contract;
- fix one bounded group at a time;
- rerun focused checks;
- do not let multiple agents repeatedly run expensive global commands against the same worktree.

Examples of useful work queues:

- failing pytest nodes;
- lint errors;
- type-check errors;
- contract-validation errors;
- E2E failures grouped by boundary.

### Step 6 — Independent adversarial review

For meaningful agent-authored code, the reviewer should be a separate agent/context from the implementer.

The reviewer should receive:

- the task/acceptance criteria;
- the diff;
- relevant contracts/tests;

and should be instructed to **assume the implementation is wrong and find why**.

The reviewer should focus on:

- regressions;
- incorrect assumptions;
- missing edge cases;
- contract mismatch;
- data leakage;
- nondeterminism;
- invalid test modifications;
- hidden coupling;
- silent failure;
- physically implausible simulation behavior;
- infeasible recommendations.

Do not ask the implementer to be its own adversarial reviewer.

### Step 7 — Fix findings, then verify again

Review comments are not completion. Apply valid findings and rerun the appropriate checks.

If the same class of failure keeps recurring, improve the prompt, test, contract, fixture, or this `AGENTS.md` rather than hand-fixing the same mistake indefinitely.

### Step 8 — Human owner remains final authority

Independent agent review increases confidence but does not replace module-owner review for domain-sensitive work.

The human owner must be able to understand and maintain the final code.

---

## 18. Review Depth by Risk

Do not use the same expensive workflow for every typo.

### Green changes

Examples:

- docs;
- straightforward boilerplate;
- already-specified dataclass;
- simple utility;
- additional unit test for known behavior.

Required:
- normal automated checks;
- human/owner review as appropriate.

Independent adversarial review is optional.

### Yellow changes

Examples:

- ETL;
- feature pipeline;
- KPI calculation;
- simulation plumbing;
- experiment runner;
- integration orchestration;
- config migration;
- performance refactor.

Required:
- implementer;
- automated checks;
- **at least one independent adversarial review**;
- module-owner review.

### Red or cross-module/high-impact implementation

Humans must first define the domain decision. Once implementation is authorized, use:

- one implementer context;
- **two independent adversarial reviewer contexts when practical**;
- one fixer pass;
- all affected human owners;
- full relevant tests + E2E verification.

Examples:

- shared-contract redesign;
- simulation semantic change;
- action/constraint semantics;
- KPI formula change;
- optimization objective implementation;
- forecast target/horizon implementation;
- architecture changes affecting multiple modules.

---

## 19. Verification Ladder

Use the cheapest meaningful check first, then increase scope.

Typical order:

```text
syntax/import
    ↓
formatter/linter/type checks (if configured)
    ↓
focused test
    ↓
module test suite
    ↓
cross-module/integration tests
    ↓
E2E demo
    ↓
experiment/regression suite when relevant
```

Current minimal repository commands are:

```bash
python -m pip install -e ".[dev]"
pytest
python -m denso_d2.demo
```

For a small module change:

1. run the focused test(s);
2. run the affected module tests;
3. run the full test suite before merge.

For a shared-contract or integration change:

1. run all tests;
2. run the end-to-end demo;
3. confirm downstream mocks/fixtures still work.

If the repository later adopts Ruff/type checking/pre-commit, agents must run the checks defined by the repository configuration. Do not invent commands that are not configured.

Passing one smoke test does not mean the task is complete.

Before merge, verify that the relevant test set is actually running and not being silently skipped.

---

## 20. Coding Agent Permission Levels

### Green — agents may implement with normal owner review

- boilerplate;
- straightforward dataclasses/contracts already specified by humans;
- unit tests for already-defined behavior;
- documentation;
- small utility functions;
- API adapters;
- repetitive parsing;
- simple UI/plumbing;
- refactors with unchanged behavior.

### Yellow — agents may implement, but require careful domain review

- ETL and validation logic;
- forecasting feature pipelines;
- simulation plumbing;
- KPI calculation code;
- experiment runners;
- integration orchestration;
- config migrations;
- performance refactors.

### Red — agents must not independently define the solution

Humans must decide:

- DENSO/factory assumptions;
- KPI definitions and priorities;
- simulation semantics;
- real bottleneck interpretation;
- action space;
- operational constraints;
- optimization objective;
- forecast target and useful decision horizon;
- experimental claims;
- system architecture changes with cross-module impact.

An agent may propose options for a Red task, but must not silently choose one and encode it as project truth.

---

## 21. Anti-Patterns

Do not:

- invent DENSO data;
- hard-code temporary factory assumptions into core algorithms;
- add a sophisticated model before a baseline exists;
- create a second copy of shared domain types inside another module;
- call another module's private functions;
- create circular dependencies;
- put core logic only in notebooks;
- use one stochastic simulation run as evidence;
- silently swallow invalid data or infeasible actions;
- patch domain bugs in the integration layer;
- rewrite an entire module for a small issue;
- introduce a large dependency without explaining why;
- create architecture "for future scale" that is not currently required;
- commit secrets, private factory data, generated large artifacts, or local environments;
- stub out behavior just to make checks pass;
- delete/skip/weaken tests merely to obtain green CI;
- let an implementer agent approve its own non-trivial change;
- run many agents in one mutable working tree;
- use destructive Git commands to resolve agent conflicts;
- use large comments to rationalize broken code;
- treat generated code volume as progress if behavior is not working.

---

## 22. Definition of Done for Agent Work

Agent work is not complete when code is merely generated.

A task is done only when:

- the requested behavior is implemented;
- ownership boundaries are respected;
- relevant tests pass;
- the intended tests actually executed;
- the code is understandable by the human owner;
- config/seed requirements are explicit;
- no confidential or invented factory data is introduced;
- shared contracts are updated only with required approvals;
- documentation is updated when behavior/contracts change;
- the relevant module owner can reproduce the result;
- integration still works when the task affects the pipeline;
- required adversarial review for the task risk level has been completed;
- review findings have been addressed, not merely recorded.

If any requirement or factory assumption is unclear, stop at the smallest safe implementation and ask the relevant owner rather than guessing.
