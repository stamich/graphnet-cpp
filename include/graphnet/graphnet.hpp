#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace graphnet {

/// Generates a dependency-free random hexadecimal identifier.
std::string make_id();

/// Strongly typed identifier wrappers prevent accidental cross-use.
struct EntityId {
    std::string value;
    static EntityId generate();
    friend bool operator==(const EntityId&, const EntityId&) = default;
};
struct MutationId {
    std::string value;
    static MutationId generate();
    friend bool operator==(const MutationId&, const MutationId&) = default;
};
struct NodeId {
    std::string value;
    static NodeId generate();
    friend bool operator==(const NodeId&, const NodeId&) = default;
};

struct EntityIdHash { std::size_t operator()(const EntityId& id) const noexcept; };
struct MutationIdHash { std::size_t operator()(const MutationId& id) const noexcept; };

/// Per-entity monotonically increasing logical version.
struct Version {
    std::uint64_t value{1};
    static constexpr Version initial() { return Version{1}; }
    constexpr Version next() const { return Version{value + 1}; }
    friend bool operator==(const Version&, const Version&) = default;
    friend auto operator<=>(const Version&, const Version&) = default;
};

/// Payload values supported by milestone 0.1.
using Bytes = std::vector<std::uint8_t>;
using Value = std::variant<std::int64_t, std::string, bool, Bytes>;
using Payload = std::map<std::string, Value>;

/// Reference to an exact historical version of an entity.
struct StateRef {
    EntityId entity_id;
    Version version;
    friend bool operator==(const StateRef&, const StateRef&) = default;
};

struct StateNode {
    NodeId id;
    EntityId entity_id;
    Version version;
    Payload payload;
    std::optional<Version> previous_version;
    std::optional<MutationId> caused_by;

    static StateNode initial(const EntityId&, Payload);
    static StateNode successor(const StateNode&, Payload, const MutationId&);
    StateRef as_ref() const;
};

struct Entity {
    EntityId id;
    std::string entity_type;
    Payload initial_state;

    static Entity create(std::string type, Payload payload);
    static Entity with_id(EntityId id, std::string type, Payload payload);
};

struct EntityUpdate {
    EntityId entity_id;
    Version expected_version;
    Payload new_payload;
};

struct Mutation {
    MutationId id;
    std::vector<StateRef> reads;
    std::vector<EntityUpdate> writes;

    static Mutation create(std::vector<StateRef> reads, std::vector<EntityUpdate> writes);
    static Mutation with_id(MutationId id, std::vector<StateRef> reads,
                            std::vector<EntityUpdate> writes);
};

struct StateTransition {
    EntityId entity_id;
    Version from_version;
    Version to_version;
    NodeId new_node_id;
};

struct Commit {
    MutationId mutation_id;
    std::vector<StateRef> dependencies;
    std::vector<StateTransition> transitions;
};

enum class ErrorCode {
    EntityNotFound, DuplicateEntity, VersionConflict,
    StateReferenceNotFound, DuplicateWrite, EmptyMutation
};

/// Domain exception carrying a stable machine-readable error code.
class GraphError : public std::runtime_error {
public:
    GraphError(ErrorCode code, std::string message);
    ErrorCode code() const noexcept;
private:
    ErrorCode code_;
};

/// Single-process in-memory state engine. Not thread-safe.
class GraphState {
public:
    GraphState() = default;

    StateRef insert_entity(const Entity& entity);
    const StateNode& current_state(const EntityId& id) const;
    const std::vector<StateNode>& history(const EntityId& id) const;
    const Mutation* mutation(const MutationId& id) const noexcept;
    const Commit* commit(const MutationId& id) const noexcept;
    const std::vector<StateRef>* dependencies_of(const MutationId& id) const noexcept;

    /// Validates all writes and dependencies before publishing any state changes.
    Commit apply(const Mutation& mutation);

private:
    std::unordered_map<EntityId, StateNode, EntityIdHash> current_;
    std::unordered_map<EntityId, std::vector<StateNode>, EntityIdHash> history_;
    std::unordered_map<MutationId, Mutation, MutationIdHash> mutations_;
    std::unordered_map<MutationId, Commit, MutationIdHash> commits_;
    std::unordered_map<MutationId, std::vector<StateRef>, MutationIdHash> dependencies_;

    bool state_ref_exists(const StateRef& ref) const;
    void validate_mutation(const Mutation& mutation) const;
};

} // namespace graphnet
