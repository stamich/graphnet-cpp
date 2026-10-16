# GraphNet C++ — Milestone 0.2

C++20 port of the supplied Rust GraphNet 0.2 workspace. This milestone adds an explicit causal DAG shared by immutable state nodes and mutations.

## Architecture

- `GraphState`: entities, immutable versions, optimistic validation, mutation/commit records and effective dependencies.
- `DependencyGraph`: directed graph with incoming/outgoing adjacency, cycle prevention, BFS reachability, parents/children, ancestors/descendants and causal relation classification.
- `GraphEngine`: coordinates state updates and causal topology using copy-on-write staging; failed operations publish neither side.
- `CausalFrontier`: snapshot of graph vertices with no outgoing edges.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/graphnet-demo
```

## Scope and limitations

The implementation is in-memory and single-process. Identifier generation is dependency-free and suitable for this prototype, not a cryptographic or globally unique ID service. It does not yet include conflict detection, conflict domains, WAL/recovery, networking, consensus, logical clocks, reachability indexes or content-addressed nodes. `DependencyGraph` traversal uses BFS (`O(V + E)` worst case); `GraphEngine::apply` copies state and graph to guarantee atomic publication at the cost of memory and time.

## Rust-to-C++ mapping

| Rust concept | C++ representation |
|---|---|
| `EntityId`, `MutationId`, `NodeId` | Strong wrapper structs around strings |
| `Payload` / `Value` | `std::map<std::string, std::variant<...>>` |
| `GraphState` | `GraphState` |
| `GraphElementId` | `GraphElementId` + `ElementKind` |
| `HashMap<T, HashSet<U>>` | `std::unordered_map<T, std::unordered_set<U>>` |
| `Result<T, E>` | Exceptions (`GraphError`, `CausalGraphError`) |
| copy-on-write staging | Copy local state/graph, then move into live engine on success |

## Known implementation notes

- Version increment checks for unsigned overflow.
- State references validate all three coordinates: node ID, entity ID and version.
- Writes implicitly depend on their current predecessor state even when not listed in explicit reads.
- Repeated edges are idempotent; self-edges and cycle-forming edges are rejected.
