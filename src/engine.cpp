#include "graphnet/graphnet.hpp"
#include <utility>
namespace graphnet {
StateRef GraphEngine::insert_entity(const Entity& entity) { auto ref=state_.insert_entity(entity);causal_.add_element(GraphElementId::state(ref.node_id));return ref; }
Commit GraphEngine::apply(const Mutation& mutation) {
    auto staged_state=state_; auto staged_causal=causal_; auto commit=staged_state.apply(mutation); integrate_commit(staged_causal,commit); state_=std::move(staged_state);causal_=std::move(staged_causal);return commit;
}
void GraphEngine::integrate_commit(DependencyGraph& graph,const Commit& commit) {
    auto m=GraphElementId::mutation(commit.mutation_id);graph.add_element(m);
    for(auto const& dep:commit.dependencies){auto s=GraphElementId::state(dep.node_id);graph.add_element(s);graph.add_edge(s,m);}
    for(auto const& t:commit.transitions){auto s=GraphElementId::state(t.new_node_id);graph.add_element(s);graph.add_edge(m,s);}
}
CausalRelation GraphEngine::causal_relation(GraphElementId a,GraphElementId b) const { return causal_.relation(a,b); }
CausalFrontier GraphEngine::frontier() const { return {causal_.frontier_elements()}; }
} // namespace graphnet
