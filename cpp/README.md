# denso_d2::decision — C++ Decision Core

Executable specification of the decision semantics defined
canonically in `docs/decision/modeling/d2_decision_model.tex` (the LaTeX
report is the source of truth; this code must not silently diverge from it).
This core is the approved executable specification of feasibility semantics
and of the Baseline 0–2 decision pipelines. The post-tour logistics slice is
implemented beside, not inside, the stable generic gate.
It does not decide that every future kernel is C++; later components are
chosen per component.

## Responsibility

- Domain types for actions, constraints, provenance, and decision-facing
  factory state.
- A three-valued feasibility gate (`Feasible` / `Infeasible` / `Unknown`)
  implementing `eq:verdict` and `eq:phistatus`, including the `a_0`
  no-action rule (feasible by construction for change-dependent constraints;
  state-dependent constraints still evaluate `a_0` against real evidence, so
  an evidenced status-quo violation is reported) and the unknown-evidence
  rule (`eq:unknownrule`: missing required hard-constraint evidence yields
  `Unknown`, never implicit feasibility).
- A deterministic `ActionCatalog` (unique IDs, explicit no-action entry)
  and `ConstraintRegistry` (unique IDs, stable insertion order).
- A minimal typed simulation boundary (`SimulationResult` / `KpiOutcome`)
  and a `SimulationEvaluator` interface with a deterministic test double.
- Baseline 0–2 decision pipeline: Baseline 0 evaluates the no-action
  alternative through the normal gate and evaluator; Baseline 1 generates
  generic rule-based candidates; Baseline 2 gates, simulates only feasible
  candidates, and selects under an explicitly supplied preference policy.
  Selection is differential-tested against a brute-force oracle in the
  tests.
- A post-tour logistics boundary (`LogisticsSnapshot`) with typed task
  priority, assignment/reassignment, and route-selection actions. Its
  pipeline performs static admission, action-scoped simulation, evidence
  identity validation, final feasibility, then policy comparison. Missing
  required evidence remains `Unknown`; `Unknown` and `Infeasible` outcomes
  never enter ranking. An unknown no-action counterfactual returns the
  distinct `BaselineEvidenceUnknown` status and cannot produce a
  recommendation.

## Non-responsibility

No final optimizer, ranking, objective/weights beyond the explicitly
synthetic test policy, KPI evaluation, simulation, Digital-Twin behavior,
serialization, CLI, or I/O. Simulator semantics stay behind the
`SimulationEvaluator` or `PostTourSimulationEvaluator` interfaces; the
decision layer never re-implements them.
The built-in `SyntheticLexicographicPreference` is a test policy only; it
is not a DENSO objective and must not be presented as one.

## Build and test

```bash
cmake -S cpp -B cpp-build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp-build --parallel
ctest --test-dir cpp-build --output-on-failure
```

Requirements: CMake >= 3.16, a C++20 compiler (tested with GCC 13). No
external dependencies; tests use a minimal assert-based harness.

Sanitizer check (optional, local):

```bash
cmake -S cpp -B cpp-build-sanitize -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build cpp-build-sanitize --parallel
./cpp-build-sanitize/denso_d2_decision_tests
```

## Tests

Three suites run via `ctest`.

`cpp/tests/decision_tests.cpp` covers the required behaviors T1–T14
(no-action semantics, hard violation, missing evidence, determinism, no
hidden defaults, soft-violation handling, duplicate-ID rejection, batch
consistency), a golden two-resource fixture, property tests (tightening
monotonicity, registry reorder invariance, confirmed-violation never
improves), behavior tests (deadline, replenishment bounds, route
knowledge, dispatch knowledge, reallocation), and the six canonical
synthetic scenarios (`balanced_flow`, `known_transport_bottleneck`,
`buffer_overflow`, `demand_spike`, `resource_shortage`,
`all_actions_infeasible`). Two of the scenarios
(`known_transport_bottleneck`, `demand_spike`) exercise the decision-core
slice only: `FactoryState` cannot express utilization or forecast
pressure, so those tests check the feasibility of the canonical actions
under the scenario's state, not the full scenario semantics.

`cpp/tests/baseline_tests.cpp` covers the Baseline 0–2 pipeline: the
no-action alternative evaluated through the normal path, rule-based
candidate generation, infeasible candidates never simulated, unknown
candidates never treated as eligible, policy-driven selection with the
deterministic recording evaluator, unknown cost never read as zero,
determinism, deterministic tie-breaking by action ID, and a brute-force
oracle cross-check (the greedy selection equals the exhaustive-best
selection on the same instance).

`cpp/tests/post_tour_tests.cpp` covers deterministic typed candidate
generation, the explicit-risk-task Baseline 1 rule, duplicate snapshot IDs,
duplicate operation payloads, malformed identifiers and out-of-range enums,
hard-violation precedence, missing static and dynamic evidence, action-bound
evidence identity, negative dynamic durations, simulation admission,
status-quo ties, unknown-baseline blocking, final-feasibility selection,
alternate-route invariants, the cost-independent synthetic preference
policy, and agreement with an exhaustive synthetic selection oracle.

All fixture values are SYNTHETIC and labeled as such; none is a DENSO fact.

## Performance characteristics

No benchmark claims are made (none have been run). Complexity: catalog and
registry insert is O(1) amortized, lookup O(1); evaluating one action is
O(|registry|) with O(1) applicability checks per constraint; evaluating N
actions is O(N × |registry|); the Baseline 2 selection loop is a linear
max-scan over feasible candidates (≤ 4 policy calls per candidate, O(N)
total). The post-tour pipeline additionally verifies that the supplied
preference policy is a total preorder: the relation is sampled once per
ordered pair (O(N^2) policy calls) and the reflexivity/completeness/
transitivity checks then run on the cached boolean matrix. This validation
is intended for the small candidate sets of a single decision epoch.
Practical bottlenecks must be established by profiling; once a
real simulation evaluator is integrated, simulation cost is expected to
dominate the evaluation loop.

## Known limitations

- `Constraint::category`, `Constraint::check_layer`, `Constraint::provenance`
  and `Action::claim_status` are carried as design metadata. Evaluation does
  not yet enforce them (a constraint registered with the "wrong" layer
  evaluates identically). Enforcing provenance/category semantics in the
  gate is a pending owner decision, not something this core decides alone.
- The Python shared contract `Action` carries `estimated_cost: float = 0.0`
  (a silent default), while this core deliberately has no cost field because
  its unit is UNKNOWN pre-tour. That cross-language contract difference is
  an open item for the shared-contract owners; the two layers must not be
  silently reconciled by either side.
- The stable generic `run_greedy_evaluation` still has no post-simulation
  evidence path. The separate post-tour logistics pipeline closes that gap
  for its typed action families without changing generic gate semantics.
- Post-tour battery thresholds, action lead times, dynamic limits, and model
  version are caller-owned rules. No DENSO value or KPI hierarchy is built
  into the core. Resource compatibility remains explicit typed evidence and
  missing values produce `Unknown`. When a task has no known current route,
  assignment and reassignment defer route resolution to the simulator: the
  presence of an available matching pickup->destination route is sufficient
  static evidence (owner-confirmed).
- `SyntheticPostTourPreference` is a total-preorder test policy over
  throughput, lead time, lateness, and WIP. It deliberately ignores optional
  cost because cost units and known-vs-unknown treatment remain unconfirmed.
- `run_greedy_evaluation` takes exactly one (scenario, seed) pair per run:
  the decision model report's scenario–seed bank with replication
  aggregation has no seam yet. Pre-tour scope; the bank API is a planned
  owner decision.

## Extension boundary

Future pipeline stages (candidate generation over real state, outcome
aggregation, real preference rules, a Digital-Twin adapter implementing
`SimulationEvaluator`) build on these types via new modules; the gate
itself is expected to stay semantically stable. Adding a constraint kind =
adding a typed payload in `constraint.hpp` and one evaluation branch in
`evaluation.cpp` — a deliberate, reviewed change.
