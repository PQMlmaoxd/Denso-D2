# Factory Tour Questions — Optimization/Decision Role (Phạm Quang Minh)

- **Tour date:** 2026-09-11. This document is the decision-layer question capture plan.
- **Usage rules:** every answer is recorded with provenance (`observed` / `estimated` / `assumed` / `mentor-confirmed` — the canonical data-parameter enum, decision model report eq:srcset), sample basis (how many times seen/who said it), and confidence. Raw notes stay outside Git (personal notes); sanitized, approved findings flow into the decision model report afterward.
- **Priority legend:** **MUST** — model cannot be built without it; **SHOULD** — materially improves quality; **NICE** — refinement.
- Each question states: why it matters, what breaks without it, fallback if undisclosed.

## 1. MUST obtain

### M1. What is the single most painful recurring logistics problem?

- **Why:** selects the decision family (decision model report, candidate families section); everything downstream keys off this.
- **Breaks without:** we model the wrong problem; all optimization effort is misdirected.
- **Fallback:** pick the family with best data visibility from other answers; state the assumption explicitly.

### M2. Which 2–3 actions are actually executable when that problem occurs? (exact levers)

- **Why:** defines the candidate action space; generic actions without executable semantics cannot be recommended.
- **Record per action:** what changes (count? route? priority? interval?), who executes it, approval needed, how fast it takes effect, is it reversible, typical cost (even ordinal).
- **Breaks without:** action catalog stays synthetic; no real recommendation possible.
- **Fallback:** use the 2–3 actions the team already plans generically (add capacity, change priority, adjust replenishment) and mark all effects as assumed.

### M3. What KPIs do they judge logistics performance by, and what are the priorities/thresholds?

- **Why:** objective function selection is a Red decision requiring their input; thresholds (e.g., "lead time must stay under X") enable epsilon-constraint methods.
- **Also capture:** current baseline values of those KPIs (today's throughput, lead time, etc.) — needed to size any future "improvement" claim and to anchor M6's before/after evidence.
- **Breaks without:** no committed objective; we can only report Pareto/descriptive deltas.
- **Fallback:** report multi-KPI deltas descriptively; ask mentor to rank the KPI list afterward by email.

### M4. What is forbidden? (constraints we must never violate)

- **Why:** hard-constraint list — safety rules, union/labor rules, route restrictions, customer priorities. Violating one in a recommendation destroys credibility.
- **Breaks without:** infeasible recommendations; credibility loss.
- **Fallback:** treat safety as hard by default; flag everything else `needs mentor validation`.

### M5. Decision cadence and response deadline

- **Why:** how often is a decision made (per shift? per day? real-time?) and how fast must a recommendation be produced? Sets horizon `T`, epoch design, and runtime budget.
- **Breaks without:** wrong temporal granularity; unusable response-time requirements.
- **Fallback:** assume shift-level; verify via mentor email.

### M6. One historical incident walkthrough

- **Why:** a concrete past occurrence (what happened → what was decided → what changed → was it measured?) validates trigger structure, action effects, and gives our first real before/after observation.
- **Breaks without:** no ground-truth anchor for action effects; everything stays assumed.
- **Fallback:** ask for any chart/metric that moved after a past intervention.

## 2. SHOULD obtain

### S1. Current manual decision rule

- Who decides today, using what rule of thumb, with what information? (This is our Baseline 0/1 in real form; also reveals implicit constraints.)

### S2. Cost structure

- Even ordinal: which actions are cheap vs. expensive? What dominates cost (labor? equipment rental? delay penalties)? Enables cost tie-breaking (decision model report, objective structures section).

### S3. Resource flexibility

- Can transporters/operators/AGVs be borrowed across lines or shifts? Maximum pool? Rental/loan options? (ADD_RESOURCE feasibility bound, C-PHY-002 in the constraint registry.)

### S4. Data availability and latency

- What operational data exists (WMS/MES/Spreadsheets), at what latency, and could any of it be shared (anonymized) during the competition? (Feeds the data-validation layer C of the decision model report; also forecast owner's needs.)

### S5. Action lead times and stability rules

- Implementation delay per action; cooldown/stability periods (C-OPS-007/008 in the constraint registry); whether changes happen only at shift boundaries (C-OPS-002).

### S6. Demand pattern and forecast practice

- How do they currently predict demand (frozen schedules? takt? historical averages)? Known peak patterns? (Coordinates with Đức's forecast design; p10/p90 realism.)

### S7. Production–transport coupling (frontier-pass addition, 2026-09-03)

- Do material supply / transport decisions interlock with production scheduling (machine starts waiting on deliveries; kitting synchronized to line takt; tow-train departure tied to station consumption)? At what granularity (shift, hour, real-time)?
- **Why:** decides whether the integrated-scheduling family (candidate decision families in the decision model report; matheuristic/decomposition scale patterns, unverified pointers) is live or whether pure transport-side actions suffice — a large modeling-complexity fork.
- **Breaks without:** we may under- or over-model the coupling; wrong interface to Đào's DES.

### S8. Re-decision trigger in practice (frontier-pass addition, 2026-09-03)

- When conditions change mid-shift, who notices, and what actually happens: fixed re-plan cadence (rolling horizon) vs. event-triggered (breakdown/delay) vs. no re-decision until shift end?
- **Why:** selects between rolling-horizon re-solve, event-triggered fast adjustment (per Adamo et al. 2024, cited in the decision model report), and one-shot shift decisions — directly changes epoch design and runtime budget (M5 refines this).
- **Breaks without:** cadence assumptions may not match operational reality.

## 3. NICE to have

### N1. Existing/past improvement projects and their measured results (external validity anchors)

### N2. Buffer sizing rationale (why current capacities? what happens on overflow in practice?)

### N3. Dispatching system details (manual boards? WMS-driven? ties broken how?)

### N4. Layout map with distances/travel-time estimates (routing family enabler)

### N5. Seasonality/mix variation across weeks (scenario bank realism)

### N6. Contact for follow-up questions (huge for the 3-month window)

## 4. Post-tour deliverable (what this feeds)

1. **Decision-family selection matrix** (candidate families from the decision model report × observed pain point × data availability × simulate-ability × explainability × impact × implementation time × risk) with **Go/No-Go per family**.
2. **Action catalog v2:** tour-observed/mentor-confirmed actions replacing synthetic ones, with provenance fields filled.
3. **Constraint register update:** constraint-registry statuses flip from `tour question` → `mentor-confirmed` / rejected.
4. **Objective selection record:** chosen preference rule with the name/date of the DENSO approver.
5. **Data requirements refresh:** data-requirement map availability column updated; family gates re-evaluated.

## 5. Capture template (print/bring)

```text
Q#:
Answer:
Source (role/person):
Provenance:  observed / estimated / assumed / mentor-confirmed
Samples seen (n):
Confidence: high / medium / low
Fallback engaged? (Y/N — which):
Follow-up needed:
```
