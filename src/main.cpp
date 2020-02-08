#include "graphnet/graphnet.hpp"
#include <iostream>
using namespace graphnet;
int main() {
  GraphEngine engine;
  auto a = engine.insert_entity(
      Entity::create("Account", {{"balance", std::int64_t{100}}}));
  auto b = engine.insert_entity(
      Entity::create("Account", {{"balance", std::int64_t{50}}}));
  auto first = engine.apply(Mutation::create(
      {a}, {{a.entity_id, a.version, {{"balance", std::int64_t{90}}}}}));
  auto first_mutation = GraphElementId::mutation(first.mutation_id);
  auto first_state =
      GraphElementId::state(first.transitions.front().new_node_id);
  auto second = engine.apply(Mutation::create(
      {b}, {{b.entity_id, b.version, {{"balance", std::int64_t{60}}}}}));
  auto second_mutation = GraphElementId::mutation(second.mutation_id);
  std::cout << "M1 -> M2: "
            << to_string(
                   engine.causal_relation(first_mutation, second_mutation))
            << '\n';
  std::cout << "M1 -> resulting state: "
            << to_string(engine.causal_relation(first_mutation, first_state))
            << '\n';
  std::cout << "Causal graph vertices: "
            << engine.causal_graph().element_count()
            << ", edges: " << engine.causal_graph().edge_count() << '\n';
  std::cout << "Frontier size: " << engine.frontier().size() << '\n';
}
