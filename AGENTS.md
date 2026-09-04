# AGENTS.md

How coding agents work in this repository.

Project: **DENSO Factory Hacks 2026 – D2: Simulate & Forecast Logistics, Recommend Actions**. Hackathon/research prototype; the goal is a reliable end-to-end decision workflow, not a general manufacturing framework:

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

Before the Factory Tour, all DENSO-specific values are synthetic or explicitly labeled assumptions. Never invent factory facts and present them as real data.

## 1. Canonical sources of truth

- **Decision-layer semantics (mathematical, normative):** `docs/decision/modeling/d2_decision_model.tex`.
- **Executable feasibility semantics:** `cpp/` — the C++20 decision core is the approved executable specification; its tests are the semantic reference.
- **Rest of pipeline:** Python package `src/denso_d2/` remains the reference implementation language.
- **Factory Tour checklist:** `docs/factory_tour/minh_optimization_questions.md`.
- **Research literature, paper notes, search records:** local only (`.local-research/`, git-ignored). Never commit them; never make the repo depend on them.

Do not duplicate domain semantics across documents, code, and comments: LaTeX = normative math; C++ tests = executable semantics; README = navigation/build/use; code comments = local invariants only; research notes = local-only evidence.

## 2. Repository ownership

| Area | Primary owner | Scope |
|---|---|---|
| `src/denso_d2/data/` | Nguyễn Quốc Khánh | schemas, configuration, synthetic data, validation, provenance |
| `src/denso_d2/forecast/` | Đồng Minh Đức | forecasting baselines, uncertainty, scenario generation |
| `src/denso_d2/simulation/` | Đào Văn Đức | discrete-event simulation, simulation KPIs, bottleneck logic |
| `src/denso_d2/optimization/` | Phạm Quang Minh | actions, constraints, feasibility, ranking/optimization |
| `src/denso_d2/integration/` | Nguyễn Thế Hưng | orchestration, shared-contract integration, E2E checks |
| `src/denso_d2/shared/` | Khánh + Hưng + affected owners | cross-module contracts and domain types |
| `cpp/` | Phạm Quang Minh | C++20 decision core (executable feasibility specification) |
| `tests/` | corresponding module owner | unit/module tests |
| `tests/integration/` | Hưng + affected owners | cross-module and E2E tests |

- Do not silently redesign another owner's domain logic.
- Integration failure inside another module: produce reproducible input, expected vs actual output, and logs; return the issue to that module's owner.
- Shared-contract changes require review from every affected owner.
- Agents do not become temporary owners by generating code.

## 3. Dependency direction

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

- `shared/` must not import project-specific modules.
- `data/`, `forecast/`, `simulation/`, `optimization/` may import from `shared/`.
- `forecast/` must not depend on integration code.
- `optimization/` must not own or reimplement simulation.
- `simulation/` must not depend on integration code.
- `integration/` may import all core modules; `demo.py` calls integration, not the pipeline modules directly.
- No circular imports. If two modules need the same type, move the minimal contract to `shared/` — do not patch around cycles with local imports or duplicate types.

## 4. Shared contracts

Current shared concepts: `FactoryConfig`, `ForecastResult`, `SimulationResult`, `Action`, `Recommendation` (an event/logistics-record type is planned but not yet defined).

- Treat them as module boundaries, not dumping grounds. No field because it "might be useful later."
- Prefer explicit typed fields once the domain meaning is understood; temporary `dict[str, Any]` only during early exploration.
- Changing a contract: identify consumers; update or add tests first when practical; keep the change minimal; update all affected call sites in the same PR; document semantic changes; get affected-owner approval.
- Refactors preserve the external contract first; internals only after behavior is covered by tests.
- Known divergence (unresolved, owner decision required): Python `Action.estimated_cost` defaults to `0.0` while the C++ core deliberately omits cost because its unit is UNKNOWN pre-tour. Do not silently reconcile either side.

## 5. Code style

- English for code, identifiers, docstrings, comments, logs, issue/PR titles.
- 4-space indent; `snake_case` for functions/methods/files/variables; `PascalCase` for classes/dataclasses; `UPPER_SNAKE_CASE` for real constants; double quotes for strings.
- Type hints for public functions/methods, dataclasses, and shared contracts.
- `pathlib.Path`; standard-library generics (`list[str]`, `X | None`); no wildcard imports; no mutable default arguments; no module-level mutable state; keyword-only arguments for domain functions/builders with several parameters.
- Library/module code uses `logging` (`logging.getLogger(__name__)`); `print()` only in demos/CLI.
- Comments explain **why** a non-obvious choice exists, never narrate code. If a workaround needs a paragraph to justify itself, reconsider the implementation.
- Do not introduce new formatters, type checkers, or package managers without maintainer approval. When one is configured in `pyproject.toml`, its output is authoritative.

## 6. Configuration and domain values

Never hard-code DENSO-specific assumptions inside algorithms; read them from configuration (`config.parameters["transporter_count"]` on the current `FactoryConfig`, not `TRANSPORTER_COUNT = 3`). Before validated DENSO data exists, parameters must be synthetic or carry provenance (`observed` / `estimated` / `assumed`), with unit and mentor-validation need where relevant. Changing a config value must not require rewriting the simulation engine.

## 7. Randomness and reproducibility

- Every stochastic run has an explicit seed; prefer local RNG objects (`random.Random(seed)`, NumPy generators) over hidden global state; never derive seeds from wall-clock time in reproducible experiments.
- Same config + same seed → same result, unless a documented dependency prevents it.
- Multi-run experiments preserve the seed→result mapping; experiment outputs record configuration and seeds.
- Never make a claim from one stochastic run; run multiple seeds and summarize the distribution.

## 8. Module rules

**data/** — owns meaning and validation of input data. Canonical event/config structures; parsing separate from downstream logic; validate required fields at boundaries; fail clearly on impossible records rather than silently repairing; keep provenance; deterministic synthetic fixtures for other modules; downstream modules must not parse raw CSV layouts directly.

**forecast/** — baselines before complex models (naive → seasonal naive / moving average → simple statistical or tree-based → more complex only with evidence). Any change states: target, horizon, input features, train/validation/test split, metric, baseline, uncertainty output. Prevent data leakage. Output must be usable by simulation without model-specific knowledge. No deep learning merely for sophistication.

**simulation/** — represents system behavior, not recommendation logic. State comes from config/state inputs, not hidden globals; separate model construction from execution; keep event generation inspectable; return structured KPI results, never print inside the simulator; maintain capacity, queue, timing, and routing invariants; validate that scenario changes produce plausible effects; profile before optimizing. Support repeated runs via builder/runner patterns.

**optimization/** — owns decision logic, not factory-state reconstruction. Keep separate: candidate generation, feasibility/constraint checking, KPI evaluation, ranking. Actions traceable to generic-synthetic or mentor-confirmed provenance only. Feasibility violations are explicit; never hide a hard violation behind a low score unless the constraint is intentionally soft. Preferred loop: generate → filter infeasible → simulate/evaluate → compare KPI distributions → rank. Never reimplement the simulator.

**integration/** — coordinates modules; depends on contracts, not private internals; provides mocks/fixtures for unfinished upstream; preserves enough logging/context to reproduce E2E failures; embeds no domain algorithms; never patches another module's semantics temporarily.

## 9. Tests are the behavioral contract

Every meaningful behavior change gets a test at the lowest useful level, in the owning module's subdirectory under `tests/` (create it when the module's first test appears). Name tests `test_<behavior>_<condition>`. Prefer behavior-focused over implementation-detail tests. Use explicit tolerances (`pytest.approx`) for float assertions. Tests must not require Internet or confidential DENSO data.

Test integrity — agents must NOT make the suite green by deleting a failing test, skipping/xfail-ing it, weakening an assertion, replacing a behavioral test with a smoke test, changing expected values to match new output, or mocking away the behavior under test. Any such change requires explicit approval from the human owner with a justification of why the old expectation was wrong.

## 10. Experiments and artifacts

Reusable logic lives in `src/denso_d2/`; notebooks and one-off scripts are exploration only — once logic is reused, move it into `src/`. Experiments load config instead of duplicating constants, set explicit seeds, save outputs outside source directories, and record enough metadata to rerun. Generated outputs, large artifacts, local datasets, and private data stay out of Git.

## 11. Logging

Module-level loggers. Useful run context: scenario name, seed, forecast horizon, selected action, runtime when relevant. No per-event spam by default (detailed event logs are opt-in or structured artifacts). Errors state what failed and which input/config caused it.

## 12. Git safety

- No direct commits to `main`. Branch prefixes: `feat/<issue>-…`, `fix/…`, `exp/…`, `docs/…`.
- Parallel agents: separate branches or worktrees; no overlapping files; one bounded change at a time; integrate only after each change passes its review gate.
- FORBIDDEN unless a human explicitly asks for that exact operation: `git reset --hard`, `git clean -fd/-fdx`, `git stash`/`git stash pop`, `git rebase`, `git push --force(-with-lease)`, `git commit --amend`.
- Never switch another worker's branch, delete worktrees, or discard changes you did not create. Git is not a synchronization mechanism between agents.
- PRs: small and focused; Conventional-Commit-style titles (`feat(simulation): …`); cross-module PRs need review from all affected owners.

## 13. Agent workflow

1. Read the task as a contract: goal, scope, inputs, expected behavior, Definition of Done, dependencies, forbidden assumptions. If unclear, stop before broad implementation and ask.
2. For non-trivial tasks, state a short plan first: what changes, what does not, contracts touched, tests to add/run, risks. Do not expand scope without approval.
3. De-risk repeated/high-volume changes with one trial slice before scaling.
4. Implement the smallest complete behavior. No `pass` stubs, dummy returns, fake constants, or broad `except Exception` blocks to silence checks.
5. Use deterministic failures as a work queue: capture output once, group by module/file/contract, fix one bounded group at a time, rerun focused checks.
6. Meaningful agent-authored code gets independent adversarial review from a separate context (given task, diff, contracts; instructed to assume the implementation is wrong and find why), covering regressions, wrong assumptions, missing edges, contract mismatch, data leakage, nondeterminism, invalid test changes, hidden coupling, silent failure, implausible simulation behavior, infeasible recommendations.
7. Fix findings and re-verify; recurring failure classes get fixed at the prompt/test/contract level.
8. The human module owner is the final authority and must be able to understand and maintain the code.

## 14. Risk tiers

**Red — agents must not independently decide** (propose options only, never silently encode one as project truth): DENSO/factory assumptions; KPI definitions and priorities; simulation semantics; real bottleneck interpretation; action space; operational constraints; optimization objective; forecast target and useful decision horizon; experimental claims; cross-module architecture changes.

**Yellow — implement with careful domain review:** ETL/validation logic; forecasting feature pipelines; simulation plumbing; KPI calculation; experiment runners; integration orchestration; config migrations; performance refactors.

**Green — normal owner review:** boilerplate; already-specified dataclasses; tests for defined behavior; documentation; small utilities; API adapters; behavior-preserving refactors.

Review depth: green → automated checks + owner review. Yellow → + at least one independent adversarial review. Red → humans define the domain decision first; then, when practical, two adversarial reviews.

## 15. Verification

Cheapest meaningful check first: syntax/import → focused test → module tests → integration → E2E demo → experiments. No lint/type checker is configured yet — do not invent one; if the maintainers configure one in `pyproject.toml`, its output is authoritative. Current commands:

```bash
python -m pip install -e ".[dev]"
pytest
python -m denso_d2.demo

# C++ decision core (CMake ≥ 3.16, C++20, no external dependencies)
cmake -S cpp -B cpp-build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp-build --parallel
ctest --test-dir cpp-build --output-on-failure
```

Do not invent commands that are not configured. Passing one smoke test is a milestone, not proof of correctness. Before merge, verify the intended tests actually ran and were not silently skipped. Shared-contract or integration changes additionally run the end-to-end demo and confirm downstream fixtures still work.

## 16. Rejected patterns

Inventing DENSO data; hard-coding factory assumptions into algorithms; sophisticated models before baselines exist; duplicating shared domain types across modules or languages; calling another module's private functions; circular dependencies; core logic only in notebooks; one stochastic run as evidence; silently swallowing invalid data or infeasible actions; patching domain bugs in the integration layer; rewriting whole modules for small issues; large dependencies without justification; speculative "future scale" architecture; committing secrets, private factory data, or generated artifacts; stubbing behavior to make checks pass; weakening tests for green CI; an implementer approving its own non-trivial change; destructive Git conflict resolution; treating generated code volume as progress.
