#include "graphnet/graphnet.hpp"

#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

namespace graphnet {

std::string make_id() {
    // Random identifier for local uniqueness; not intended as a cryptographic token.
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (int i = 0; i < 2; ++i) out << std::setw(16) << rng();
    return out.str();
}
EntityId EntityId::generate() { return {make_id()}; }
MutationId MutationId::generate() { return {make_id()}; }
NodeId NodeId::generate() { return {make_id()}; }
std::size_t EntityIdHash::operator()(const EntityId& id) const noexcept {
    return std::hash<std::string>{}(id.value);
}
std::size_t MutationIdHash::operator()(const MutationId& id) const noexcept {
    return std::hash<std::string>{}(id.value);
}

StateNode StateNode::initial(const EntityId& entity_id, Payload payload) {
    return {NodeId::generate(), entity_id, Version::initial(), std::move(payload),
            std::nullopt, std::nullopt};
}
StateNode StateNode::successor(const StateNode& current, Payload payload,
                               const MutationId& mutation_id) {
    return {NodeId::generate(), current.entity_id, current.version.next(),
            std::move(payload), current.version, mutation_id};
}
StateRef StateNode::as_ref() const { return {entity_id, version}; }

Entity Entity::create(std::string type, Payload payload) {
    return {EntityId::generate(), std::move(type), std::move(payload)};
}
Entity Entity::with_id(EntityId id, std::string type, Payload payload) {
    return {std::move(id), std::move(type), std::move(payload)};
}
Mutation Mutation::create(std::vector<StateRef> reads, std::vector<EntityUpdate> writes) {
    return {MutationId::generate(), std::move(reads), std::move(writes)};
}
Mutation Mutation::with_id(MutationId id, std::vector<StateRef> reads,
                           std::vector<EntityUpdate> writes) {
    return {std::move(id), std::move(reads), std::move(writes)};
}
GraphError::GraphError(ErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}
ErrorCode GraphError::code() const noexcept { return code_; }

StateRef GraphState::insert_entity(const Entity& entity) {
    if (current_.contains(entity.id))
        throw GraphError(ErrorCode::DuplicateEntity, "entity already exists: " + entity.id.value);
    auto node = StateNode::initial(entity.id, entity.initial_state);
    const auto ref = node.as_ref();
    current_.emplace(entity.id, node);
    history_.emplace(entity.id, std::vector<StateNode>{std::move(node)});
    return ref;
}

const StateNode& GraphState::current_state(const EntityId& id) const {
    const auto it = current_.find(id);
    if (it == current_.end())
        throw GraphError(ErrorCode::EntityNotFound, "entity not found: " + id.value);
    return it->second;
}
const std::vector<StateNode>& GraphState::history(const EntityId& id) const {
    const auto it = history_.find(id);
    if (it == history_.end())
        throw GraphError(ErrorCode::EntityNotFound, "entity not found: " + id.value);
    return it->second;
}
const Mutation* GraphState::mutation(const MutationId& id) const noexcept {
    const auto it = mutations_.find(id); return it == mutations_.end() ? nullptr : &it->second;
}
const Commit* GraphState::commit(const MutationId& id) const noexcept {
    const auto it = commits_.find(id); return it == commits_.end() ? nullptr : &it->second;
}
const std::vector<StateRef>* GraphState::dependencies_of(const MutationId& id) const noexcept {
    const auto it = dependencies_.find(id);
    return it == dependencies_.end() ? nullptr : &it->second;
}

bool GraphState::state_ref_exists(const StateRef& ref) const {
    const auto it = history_.find(ref.entity_id);
    if (it == history_.end()) return false;
    for (const auto& node : it->second) if (node.version == ref.version) return true;
    return false;
}

void GraphState::validate_mutation(const Mutation& mutation) const {
    if (mutation.writes.empty())
        throw GraphError(ErrorCode::EmptyMutation, "mutation must contain at least one write");

    std::unordered_set<EntityId, EntityIdHash> written;
    for (const auto& update : mutation.writes) {
        if (!written.insert(update.entity_id).second)
            throw GraphError(ErrorCode::DuplicateWrite,
                             "duplicate write for entity: " + update.entity_id.value);
        const auto& current = current_state(update.entity_id);
        if (!(current.version == update.expected_version)) {
            throw GraphError(ErrorCode::VersionConflict,
                "version conflict for entity " + update.entity_id.value +
                ": expected " + std::to_string(update.expected_version.value) +
                ", actual " + std::to_string(current.version.value));
        }
    }
    for (const auto& dependency : mutation.reads) {
        if (!state_ref_exists(dependency))
            throw GraphError(ErrorCode::StateReferenceNotFound,
                "referenced state does not exist for entity: " + dependency.entity_id.value +
                " version " + std::to_string(dependency.version.value));
    }
}

Commit GraphState::apply(const Mutation& mutation) {
    validate_mutation(mutation);

    // Prepare all successor states and transitions before changing the current state.
    std::vector<StateNode> new_states;
    std::vector<StateTransition> transitions;
    new_states.reserve(mutation.writes.size());
    transitions.reserve(mutation.writes.size());
    for (const auto& update : mutation.writes) {
        const auto& current = current_.at(update.entity_id);
        auto successor = StateNode::successor(current, update.new_payload, mutation.id);
        transitions.push_back({update.entity_id, current.version, successor.version, successor.id});
        new_states.push_back(std::move(successor));
    }

    Commit result{mutation.id, mutation.reads, std::move(transitions)};

    // Publication phase. This is logical atomicity for validation failures,
    // not a crash-safe or allocation-failure-atomic transaction.
    for (const auto& state : new_states) {
        current_.insert_or_assign(state.entity_id, state);
        history_.at(state.entity_id).push_back(state);
    }
    dependencies_.insert_or_assign(mutation.id, mutation.reads);
    commits_.insert_or_assign(mutation.id, result);
    mutations_.insert_or_assign(mutation.id, mutation);
    return result;
}

} // namespace graphnet
