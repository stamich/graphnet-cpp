#include "graphnet/graphnet.hpp"
#include <iostream>

using namespace graphnet;

static Payload account_payload(std::int64_t balance) {
    return {{"balance", Value{balance}}};
}
static std::int64_t balance_of(const Payload& payload) {
    return std::get<std::int64_t>(payload.at("balance"));
}

int main() {
    GraphState graph;
    auto alice = Entity::create("Account", account_payload(100));
    auto bob = Entity::create("Account", account_payload(50));
    const auto alice_id = alice.id;
    const auto bob_id = bob.id;
    graph.insert_entity(alice);
    graph.insert_entity(bob);

    std::cout << "Initial state:\n"
              << "  Alice v" << graph.current_state(alice_id).version.value << " = "
              << balance_of(graph.current_state(alice_id).payload) << '\n'
              << "  Bob   v" << graph.current_state(bob_id).version.value << " = "
              << balance_of(graph.current_state(bob_id).payload) << '\n';

    auto transfer = Mutation::create(
        {{alice_id, Version::initial()}, {bob_id, Version::initial()}},
        {{alice_id, Version::initial(), account_payload(80)},
         {bob_id, Version::initial(), account_payload(70)}});
    const auto committed_id = transfer.id;
    auto commit = graph.apply(transfer);

    std::cout << "\nCommitted mutation " << committed_id.value << '\n';
    for (const auto& t : commit.transitions)
        std::cout << "  " << t.entity_id.value << ": v" << t.from_version.value
                  << " -> v" << t.to_version.value << '\n';

    std::cout << "\nCurrent state:\n"
              << "  Alice v" << graph.current_state(alice_id).version.value << " = "
              << balance_of(graph.current_state(alice_id).payload) << '\n'
              << "  Bob   v" << graph.current_state(bob_id).version.value << " = "
              << balance_of(graph.current_state(bob_id).payload) << '\n';
}
