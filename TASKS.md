# GraphNet C++ milestone 0.2 — implementation tasks

Each task is intended to be represented by one commit on `milestone-0.2`.

## Task 1 — Project setup and implementation plan
- Establish C++20/CMake layout and compiler warnings.
- Document milestone scope and map Rust concepts to C++ equivalents.
- Commit: `chore: set up GraphNet C++ milestone 0.2`

## Task 2 — Core state model and mutation validation
- Add strong ID wrappers, `Version`, `StateRef`, immutable `StateNode`, entity/mutation/update/commit records and typed payloads.
- Port `GraphState`: initial entity insertion, immutable history, state reference validation, optimistic version checks, implicit write-predecessor dependencies, duplicate/empty-write rejection and commit generation.
- Ensure validation and successor construction happen before publishing state changes.
- Commit: `feat: port GraphNet core state model`

## Task 3 — Causal dependency DAG
- Add a shared vertex type for state nodes and mutations.
- Maintain incoming and outgoing adjacency indexes.
- Add idempotent edge insertion with self-edge and cycle prevention.
- Implement parents/children, BFS reachability, ancestors/descendants, causal relation classification and frontier discovery.
- Commit: `feat: implement causal dependency graph`

## Task 4 — Atomic GraphEngine integration
- Coordinate GraphState and DependencyGraph.
- Integrate each commit as `State -> Mutation -> State` causal edges.
- Use copy-on-write staging so a failed state or graph operation cannot partially publish changes.
- Commit: `feat: integrate state commits with causal graph`

## Task 5 — Demo, regression tests and developer docs
- Add an end-to-end example demonstrating independent mutations as concurrent.
- Cover state history, causal ordering, concurrency, frontier, cycle prevention and failed-mutation atomicity.
- Document build/test commands, architecture, complexity and milestone limitations.
- Commit: `test: cover GraphNet milestone 0.2 causal semantics`

## Acceptance criteria
- CMake configure and build succeed with C++20.
- `ctest --test-dir build --output-on-failure` passes.
- Demo reports independent mutations as `Concurrent` and mutation-to-result-state as `Before`.
- No WAL, persistence, networking, consensus, conflict analysis or reachability indexes are claimed for this milestone.
