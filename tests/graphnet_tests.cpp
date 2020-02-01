#include "graphnet/graphnet.hpp"
#include <cassert>
#include <iostream>

using namespace graphnet;
static Payload account(std::int64_t n) { return {{"balance", Value{n}}}; }
static std::int64_t balance(const Payload& p) { return std::get<std::int64_t>(p.at("balance")); }

static void transfer_commits_two_versions() {
    GraphState g;
    auto a = Entity::create("Account", account(100));
    auto b = Entity::create("Account", account(50));
    auto aid = a.id; auto bid = b.id;
    g.insert_entity(a); g.insert_entity(b);
    auto m = Mutation::create({{aid, Version::initial()}, {bid, Version::initial()}},
        {{aid, Version::initial(), account(80)}, {bid, Version::initial(), account(70)}});
    auto mid = m.id;
    auto c = g.apply(m);
    assert(c.transitions.size() == 2 && c.dependencies.size() == 2);
    assert(g.current_state(aid).version.value == 2 && g.current_state(bid).version.value == 2);
    assert(balance(g.current_state(aid).payload) == 80 && balance(g.current_state(bid).payload) == 70);
    assert(g.history(aid).size() == 2 && g.history(bid).size() == 2);
    assert(g.commit(mid) && g.mutation(mid) && g.dependencies_of(mid)->size() == 2);
}
static void stale_mutation_is_rejected_without_changes() {
    GraphState g; auto a = Entity::create("Account", account(100)); auto id = a.id; g.insert_entity(a);
    g.apply(Mutation::create({{id, Version::initial()}},
        {{id, Version::initial(), account(90)}}));
    bool rejected = false;
    try { g.apply(Mutation::create({{id, Version::initial()}},
            {{id, Version::initial(), account(80)}})); }
    catch (const GraphError& e) { rejected = e.code() == ErrorCode::VersionConflict; }
    assert(rejected && balance(g.current_state(id).payload) == 90);
}
static void multi_entity_validation_is_atomic() {
    GraphState g; auto a = Entity::create("Account", account(100)); auto b = Entity::create("Account", account(50));
    auto aid = a.id; auto bid = b.id; g.insert_entity(a); g.insert_entity(b);
    bool rejected = false;
    try { g.apply(Mutation::create({{aid, Version::initial()}, {bid, Version::initial()}},
        {{aid, Version::initial(), account(80)}, {bid, Version{999}, account(70)}})); }
    catch (const GraphError& e) { rejected = e.code() == ErrorCode::VersionConflict; }
    assert(rejected);
    assert(g.current_state(aid).version.value == 1 && g.current_state(bid).version.value == 1);
    assert(balance(g.current_state(aid).payload) == 100 && balance(g.current_state(bid).payload) == 50);
}
static void unknown_dependency_is_rejected() {
    GraphState g; auto a = Entity::create("Account", account(100)); auto id = a.id; g.insert_entity(a);
    bool rejected = false;
    try { g.apply(Mutation::create({{id, Version{999}}}, {{id, Version::initial(), account(90)}})); }
    catch (const GraphError& e) { rejected = e.code() == ErrorCode::StateReferenceNotFound; }
    assert(rejected && g.current_state(id).version.value == 1);
}
static void duplicate_and_empty_writes_are_rejected() {
    GraphState g; auto a = Entity::create("Account", account(100)); auto id = a.id; g.insert_entity(a);
    bool duplicate = false, empty = false;
    try { g.apply(Mutation::create({}, {{id, Version::initial(), account(90)}, {id, Version::initial(), account(80)}})); }
    catch (const GraphError& e) { duplicate = e.code() == ErrorCode::DuplicateWrite; }
    try { g.apply(Mutation::create({}, {})); }
    catch (const GraphError& e) { empty = e.code() == ErrorCode::EmptyMutation; }
    assert(duplicate && empty && g.current_state(id).version.value == 1);
}
static void disjoint_mutations_commit_independently() {
    GraphState g; auto a = Entity::create("Account", account(100)); auto b = Entity::create("Account", account(50));
    auto aid = a.id; auto bid = b.id; g.insert_entity(a); g.insert_entity(b);
    g.apply(Mutation::create({{aid, Version::initial()}}, {{aid, Version::initial(), account(95)}}));
    g.apply(Mutation::create({{bid, Version::initial()}}, {{bid, Version::initial(), account(60)}}));
    assert(balance(g.current_state(aid).payload) == 95 && balance(g.current_state(bid).payload) == 60);
}
int main() {
    transfer_commits_two_versions();
    stale_mutation_is_rejected_without_changes();
    multi_entity_validation_is_atomic();
    unknown_dependency_is_rejected();
    duplicate_and_empty_writes_are_rejected();
    disjoint_mutations_commit_independently();
    std::cout << "All GraphNet C++ milestone 0.1 tests passed.\\n";
}
