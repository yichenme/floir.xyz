#pragma once
#include <cstdint>

class Client;

namespace TalentOps {
    void try_buy(Client *client, uint8_t tree, uint8_t target_rank);
    void reset(Client *client);
}
