#include "graphnet/graphnet.hpp"
#include <deque>
#include <functional>
namespace graphnet {
GraphElementId GraphElementId::state(const NodeId &id) {
  return {ElementKind::State, id.value};
}

GraphElementId GraphElementId::mutation(const MutationId &id) {
  return {ElementKind::Mutation, id.value};
}

std::size_t
GraphElementHash::operator()(const GraphElementId &id) const noexcept {
  return std::hash<std::string>{}(id.value) ^
         (static_cast<std::size_t>(id.kind) << 1);
}

std::string to_string(CausalRelation r) {
  switch (r) {
  case CausalRelation::Same:
    return "Same";
  case CausalRelation::Before:
    return "Before";
  case CausalRelation::After:
    return "After";
  case CausalRelation::Concurrent:
    return "Concurrent";
  }
  return "Unknown";
}

void DependencyGraph::add_element(GraphElementId id) {
  outgoing_.try_emplace(id);
  incoming_.try_emplace(id);
}

bool DependencyGraph::contains(GraphElementId id) const {
  return outgoing_.contains(id);
}

std::size_t DependencyGraph::element_count() const { return outgoing_.size(); }

std::size_t DependencyGraph::edge_count() const {
  std::size_t n = 0;
  for (auto const &[_, children] : outgoing_)
    n += children.size();
  return n;
}

void DependencyGraph::require_element(GraphElementId id) const {
  if (!contains(id))
    throw CausalGraphError("graph element not found: " + id.value);
}

void DependencyGraph::add_edge(GraphElementId from, GraphElementId to) {
  require_element(from);
  require_element(to);
  if (from == to)
    throw CausalGraphError("self edge is not allowed");
  if (outgoing_.at(from).contains(to))
    return;
  if (is_reachable(to, from))
    throw CausalGraphError("edge would create cycle");
  outgoing_.at(from).insert(to);
  incoming_.at(to).insert(from);
}

std::unordered_set<GraphElementId, GraphElementHash>
DependencyGraph::parents(GraphElementId id) const {
  require_element(id);
  return incoming_.at(id);
}

std::unordered_set<GraphElementId, GraphElementHash>
DependencyGraph::children(GraphElementId id) const {
  require_element(id);
  return outgoing_.at(id);
}

bool DependencyGraph::is_reachable(GraphElementId source,
                                   GraphElementId target) const {
  require_element(source);
  require_element(target);
  if (source == target)
    return true;
  std::unordered_set<GraphElementId, GraphElementHash> visited{source};
  std::deque<GraphElementId> q{source};

  while (!q.empty()) {
    auto cur = q.front();
    q.pop_front();
    for (auto next : outgoing_.at(cur)) {
      if (next == target)
        return true;
      if (visited.insert(next).second)
        q.push_back(next);
    }
  }
  return false;
}

CausalRelation DependencyGraph::relation(GraphElementId a,
                                         GraphElementId b) const {
  require_element(a);
  require_element(b);
  if (a == b)
    return CausalRelation::Same;
  if (is_reachable(a, b))
    return CausalRelation::Before;
  if (is_reachable(b, a))
    return CausalRelation::After;
  return CausalRelation::Concurrent;
}

DependencyGraph::Set DependencyGraph::traverse(
    GraphElementId id,
    const std::unordered_map<GraphElementId, Set, GraphElementHash> &adj)
    const {
  Set seen;
  std::deque<GraphElementId> q;
  for (auto x : adj.at(id))
    if (seen.insert(x).second)
      q.push_back(x);
  while (!q.empty()) {
    auto cur = q.front();
    q.pop_front();
    for (auto x : adj.at(cur))
      if (seen.insert(x).second)
        q.push_back(x);
  }
  return seen;
}

DependencyGraph::Set DependencyGraph::ancestors(GraphElementId id) const {
  require_element(id);
  return traverse(id, incoming_);
}

DependencyGraph::Set DependencyGraph::descendants(GraphElementId id) const {
  require_element(id);
  return traverse(id, outgoing_);
}

std::unordered_set<GraphElementId, GraphElementHash>
DependencyGraph::elements() const {
  Set result;
  for (auto const &[id, _] : outgoing_)
    result.insert(id);
  return result;
}

std::unordered_set<GraphElementId, GraphElementHash>
DependencyGraph::frontier_elements() const {
  Set result;
  for (auto const &[id, children] : outgoing_)
    if (children.empty())
      result.insert(id);
  return result;
}

bool CausalFrontier::contains(GraphElementId id) const {
  return elements.contains(id);
}

std::size_t CausalFrontier::size() const { return elements.size(); }

bool CausalFrontier::empty() const { return elements.empty(); }
} // namespace graphnet
