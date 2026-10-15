#include "graphnet/graphnet.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace graphnet;
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
template<class F> void throws(F&& f,const char* message){++checks;try{f();}catch(const std::exception&){return;}std::cerr<<"FAIL (expected exception): "<<message<<'\n';std::exit(1);}
Payload payload(std::int64_t value){return {{"value",value}};}
}
int main(){
    GraphEngine e;
    auto a=e.insert_entity(Entity::create("Thing",payload(1)));
    auto b=e.insert_entity(Entity::create("Thing",payload(10)));
    check(e.state().current_state(a.entity_id).version.value==1,"initial version");
    auto c1=e.apply(Mutation::create({},{{a.entity_id,a.version,payload(2)}}));
    auto m1=GraphElementId::mutation(c1.mutation_id); auto a2=GraphElementId::state(c1.transitions[0].new_node_id);
    check(e.causal_relation(GraphElementId::state(a.node_id),m1)==CausalRelation::Before,"predecessor precedes mutation");
    check(e.causal_relation(m1,a2)==CausalRelation::Before,"mutation precedes output state");
    check(e.causal_relation(m1,m1)==CausalRelation::Same,"same element relation");
    auto c2=e.apply(Mutation::create({},{{b.entity_id,b.version,payload(11)}}));
    auto m2=GraphElementId::mutation(c2.mutation_id);
    check(e.causal_relation(m1,m2)==CausalRelation::Concurrent,"independent writes concurrent");
    check(e.causal_graph().ancestors(a2).size()==2,"transitive ancestors include initial state and mutation");
    check(e.causal_graph().descendants(GraphElementId::state(a.node_id)).contains(a2),"transitive descendants");
    check(e.state().history(a.entity_id).size()==2,"entity history");
    auto frontier=e.frontier(); check(frontier.contains(a2)&&frontier.contains(GraphElementId::state(c2.transitions[0].new_node_id)),"causal heads in frontier");
    DependencyGraph cyclic; auto x=GraphElementId::state(NodeId{"x"}); auto y=GraphElementId::state(NodeId{"y"}); cyclic.add_element(x); cyclic.add_element(y); cyclic.add_edge(x,y); throws([&]{cyclic.add_edge(y,x);},"cycle rejected");
    auto before=e.causal_graph().edge_count();
    throws([&]{e.apply(Mutation::create({},{{a.entity_id,Version{99},payload(3)}}));},"stale write rejected");
    check(e.causal_graph().edge_count()==before,"failed mutation leaves graph unchanged");
    throws([&]{e.apply(Mutation::create({},{{a.entity_id,a.version,payload(4)},{a.entity_id,a.version,payload(5)}}));},"duplicate writes rejected");
    throws([&]{e.apply(Mutation::create({},{}));},"empty mutation rejected");
    throws([&]{e.causal_relation(GraphElementId::state(NodeId{"missing"}),m1);},"unknown graph node rejected");
    std::cout<<"Passed "<<checks<<" checks\n";
}
