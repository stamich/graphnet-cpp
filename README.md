# GraphNet C++ — Milestone 0.1

C++20 port of the supplied Rust `graphnet-milestone-0.1` workspace.

## Scope
- Strongly typed entity, mutation and immutable state-node identifiers
- Immutable-by-convention state versions and per-entity history
- Typed payload values (`int64_t`, UTF-8-ish `std::string`, `bool`, byte vector)
- Explicit read dependencies and optimistic expected-version checks
- Atomic multi-entity mutation validation and commit records
- Current state, history, mutation, commit and dependency lookup
- Demo and regression tests corresponding to the Rust milestone

## Build
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/graphnet-demo
```

## Design notes
This is an in-memory, single-process foundation. It has no WAL, persistence, networking,
consensus, thread-safety or conflict-domain analysis. Identifier strings are generated
from random hexadecimal values for a dependency-free build; they are not cryptographic
identities. The public model uses `std::variant` and `std::map` to keep payload values
typed and deterministically ordered.

The engine validates the entire mutation and prepares successor nodes before publishing
logical changes. As in the Rust source, this is logical all-or-nothing behavior for
ordinary validation failures; it is not a crash-safe transaction protocol.
