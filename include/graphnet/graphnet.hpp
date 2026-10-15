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

using PayloadValue = std::variant<std::int64_t, std::string, bool, std::vector<std::uint8_t>>;
using Payload = std::map<std::string, PayloadValue>;

struct EntityId { std::string value; static EntityId generate(); bool operator==(const EntityId&) const = default; };
struct MutationId { std::string value; static MutationId generate(); bool operator==(const MutationId&) const = default; };
struct NodeId { std::string value; static NodeId generate(); bool operator==(const NodeId&) const = default; };
struct Version { std::uint64_t value{1}; Version next() const; bool operator==(const Version&) const = default; auto operator<=>(const Version&) const = default; };

struct StateRef {
    NodeId node_id;
    EntityId entity_id;
    Version version;
    bool operator==(const StateRef&) const = default;
};
struct StateNode {
    NodeId id;
    EntityId entity_id;
    Version version;
    Payload payload;
    std::optional<Version> previous_version;
    std::optional<MutationId> caused_by;
    StateRef ref() const;
};
struct Entity { EntityId id; std::string type; Payload initial_state; static Entity create(std::string type, Payload initial_state); };
struct EntityUpdate { EntityId entity_id; Version expected_version; Payload new_payload; };
struct Mutation { MutationId id; std::vector<StateRef> reads; std::vector<EntityUpdate> writes; static Mutation create(std::vector<StateRef> reads, std::vector<EntityUpdate> writes); };
struct StateTransition { EntityId entity_id; StateRef previous_state; Version from_version; Version to_version; NodeId new_node_id; StateRef resulting_state() const; };
struct Commit { MutationId mutation_id; std::vector<StateRef> dependencies; std::vector<StateTransition> transitions; };

class GraphError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class GraphState {
public:
    StateRef insert_entity(const Entity& entity);
    const StateNode& current_state(const EntityId& id) const;
    const StateNode* node(const NodeId& id) const;
    const std::vector<StateNode>& history(const EntityId& id) const;
    const Mutation* mutation(const MutationId& id) const;
    const Commit* commit(const MutationId& id) const;
    const std::vector<StateRef>* dependencies_of(const MutationId& id) const;
    Commit apply(const Mutation& mutation);
private:
    std::unordered_map<std::string, StateNode> current_;
    std::unordered_map<std::string, std::vector<StateNode>> history_;
    std::unordered_map<std::string, StateNode> nodes_;
    std::unordered_map<std::string, Mutation> mutations_;
    std::unordered_map<std::string, Commit> commits_;
    std::unordered_map<std::string, std::vector<StateRef>> dependencies_;
    bool state_ref_exists(const StateRef& ref) const;
};

enum class ElementKind { State, Mutation };
struct GraphElementId {
    ElementKind kind;
    std::string value;
    static GraphElementId state(const NodeId& id);
    static GraphElementId mutation(const MutationId& id);
    bool operator==(const GraphElementId&) const = default;
};
struct GraphElementHash { std::size_t operator()(const GraphElementId& id) const noexcept; };
struct CausalEdge { GraphElementId from; GraphElementId to; };
enum class CausalRelation { Same, Before, After, Concurrent };
std::string to_string(CausalRelation relation);

class CausalGraphError : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class DependencyGraph {
public:
    void add_element(GraphElementId id);
    void add_edge(GraphElementId from, GraphElementId to);
    bool contains(GraphElementId id) const;
    std::size_t element_count() const;
    std::size_t edge_count() const;
    std::unordered_set<GraphElementId, GraphElementHash> parents(GraphElementId id) const;
    std::unordered_set<GraphElementId, GraphElementHash> children(GraphElementId id) const;
    bool is_reachable(GraphElementId source, GraphElementId target) const;
    CausalRelation relation(GraphElementId a, GraphElementId b) const;
    std::unordered_set<GraphElementId, GraphElementHash> ancestors(GraphElementId id) const;
    std::unordered_set<GraphElementId, GraphElementHash> descendants(GraphElementId id) const;
    std::unordered_set<GraphElementId, GraphElementHash> elements() const;
    std::unordered_set<GraphElementId, GraphElementHash> frontier_elements() const;
private:
    using Set = std::unordered_set<GraphElementId, GraphElementHash>;
    std::unordered_map<GraphElementId, Set, GraphElementHash> outgoing_;
    std::unordered_map<GraphElementId, Set, GraphElementHash> incoming_;
    void require_element(GraphElementId id) const;
    Set traverse(GraphElementId id, const std::unordered_map<GraphElementId, Set, GraphElementHash>& adjacency) const;
};
struct CausalFrontier {
    std::unordered_set<GraphElementId, GraphElementHash> elements;
    bool contains(GraphElementId id) const;
    std::size_t size() const;
    bool empty() const;
};
class GraphEngine {
public:
    StateRef insert_entity(const Entity& entity);
    Commit apply(const Mutation& mutation);
    CausalRelation causal_relation(GraphElementId a, GraphElementId b) const;
    CausalFrontier frontier() const;
    const GraphState& state() const { return state_; }
    const DependencyGraph& causal_graph() const { return causal_; }
private:
    GraphState state_;
    DependencyGraph causal_;
    static void integrate_commit(DependencyGraph& graph, const Commit& commit);
};

} // namespace graphnet

namespace std {
template<> struct hash<graphnet::GraphElementId> { size_t operator()(const graphnet::GraphElementId& id) const noexcept { return graphnet::GraphElementHash{}(id); } };
}
