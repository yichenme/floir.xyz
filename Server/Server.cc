#include <Server/Server.hh>

#include <Server/Game.hh>
#include <Server/Client.hh>

#include <Shared/Binary.hh>
#include <Shared/StaticData.hh>

#include <chrono>
#include <iostream>

namespace Server {
    uint8_t OUTGOING_PACKET[MAX_PACKET_LEN] = {0};
    GameInstance game;
}

using namespace Server;

void Server::tick() {
    using namespace std::chrono_literals;
    auto start = std::chrono::steady_clock::now();
    Server::game.tick();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> tick_time = end - start;
    // Only warn on ticks approaching the TPS budget (a healthy tick is well
    // under this); the old 5ms threshold logged every tick once populated.
    // 80% of the actual per-tick budget (1000/TPS), so this scales with TPS
    // instead of assuming the old fixed 20 TPS / 50ms budget.
    if (tick_time.count() > 800.0 / TPS) std::cout << "tick took " << tick_time << '\n';
}

void Server::init() {
    Server::game.init();
    Server::run();
}
