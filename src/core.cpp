#include "graphnet/graphnet.hpp"
#include <algorithm>
#include <atomic>
#include <deque>
#include <sstream>

namespace graphnet {
namespace {
std::atomic<std::uint64_t> sequence{1};
std::string make_id(const char *prefix) {
  return std::string(prefix) + "-" + std::to_string(sequence.fetch_add(1));
}

template <class Map, class Key>
auto require(Map const &map, Key const &key, const std::string &what) -> const
    typename Map::mapped_type & {
  auto it = map.find(key);
  if (it == map.end())
    throw GraphError(what + " not found");
  return it->second;
}
} // namespace

EntityId EntityId::generate() { return {make_id("entity")}; }

MutationId MutationId::generate() { return {make_id("mutation")}; }

NodeId NodeId::generate() { return {make_id("node")}; }

Version Version::next() const {
  if (value == UINT64_MAX)
    throw GraphError("version overflow");
  return {value + 1};
}

StateRef StateNode::ref() const { return {id, entity_id, version}; }

Entity Entity::create(std::string type, Payload payload) {
  return {EntityId::generate(), std::move(type), std::move(payload)};
}

Mutation Mutation::create(std::vector<StateRef> reads,
                          std::vector<EntityUpdate> writes) {
  return {MutationId::generate(), std::move(reads), std::move(writes)};
}

StateRef StateTransition::resulting_state() const {
  return {new_node_id, entity_id, to_version};
}

StateRef GraphState::insert_entity(const Entity &entity) {
  if (current_.contains(entity.id.value))
    throw GraphError("duplicate entity: " + entity.id.value);
  StateNode node{NodeId::generate(),   entity.id,    Version{1},
                 entity.initial_state, std::nullopt, std::nullopt};
  auto ref = node.ref();
  nodes_.emplace(node.id.value, node);
  current_.emplace(entity.id.value, node);
  history_[entity.id.value].push_back(node);
  return ref;
}

const StateNode &GraphState::current_state(const EntityId &id) const {
  return require(current_, id.value, "entity");
}

const StateNode *GraphState::node(const NodeId &id) const {
  auto it = nodes_.find(id.value);
  return it == nodes_.end() ? nullptr : &it->second;
}

const std::vector<StateNode> &GraphState::history(const EntityId &id) const {
  return require(history_, id.value, "history");
}

const Mutation *GraphState::mutation(const MutationId &id) const {
  auto it = mutations_.find(id.value);
  return it == mutations_.end() ? nullptr : &it->second;
}

const Commit *GraphState::commit(const MutationId &id) const {
  auto it = commits_.find(id.value);
  return it == commits_.end() ? nullptr : &it->second;
}

const std::vector<StateRef> *
GraphState::dependencies_of(const MutationId &id) const {
  auto it = dependencies_.find(id.value);
  return it == dependencies_.end() ? nullptr : &it->second;
}

bool GraphState::state_ref_exists(const StateRef &ref) const {
  auto n = node(ref.node_id);
  return n && n->entity_id == ref.entity_id && n->version == ref.version;
}

Commit GraphState::apply(const Mutation &mutation) {
  if (mutations_.contains(mutation.id.value))
    throw GraphError("duplicate mutation id: " + mutation.id.value);
  if (mutation.writes.empty())
    throw GraphError("mutation must contain at least one write");
  std::unordered_set<std::string> written;
  for (const auto &write : mutation.writes) {
    if (!written.insert(write.entity_id.value).second)
      throw GraphError("duplicate write for entity: " + write.entity_id.value);
    const auto &current = current_state(write.entity_id);
    if (current.version != write.expected_version)
      throw GraphError("version conflict for entity: " + write.entity_id.value);
  }
  for (const auto &read : mutation.reads)
    if (!state_ref_exists(read))
      throw GraphError("invalid state reference: " + read.node_id.value);
  std::vector<StateRef> dependencies = mutation.reads;
  std::unordered_set<std::string> seen;
  for (const auto &d : dependencies)
    seen.insert(d.node_id.value);
  for (const auto &write : mutation.writes) {
    auto ref = current_state(write.entity_id).ref();
    if (seen.insert(ref.node_id.value).second)
      dependencies.push_back(ref);
  }

  Commit commit{mutation.id, dependencies, {}};

  std::vector<StateNode> successors;
  successors.reserve(mutation.writes.size());

  for (const auto &write : mutation.writes) {
    const auto &current = current_state(write.entity_id);
    StateNode next{NodeId::generate(), write.entity_id, current.version.next(),
                   write.new_payload,  current.version, mutation.id};
    commit.transitions.push_back({write.entity_id, current.ref(),
                                  current.version, next.version, next.id});
    successors.push_back(std::move(next));
  }

  // Publish only after all validation and successor construction has succeeded.
  for (const auto &next : successors) {
    nodes_.emplace(next.id.value, next);
    current_[next.entity_id.value] = next;
    history_[next.entity_id.value].push_back(next);
  }

  dependencies_[mutation.id.value] = dependencies;
  commits_[mutation.id.value] = commit;
  mutations_[mutation.id.value] = mutation;
  return commit;
}

} // namespace graphnet
