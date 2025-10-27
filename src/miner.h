#include "chain.h"
#include "utils/common.h"
#include "p2p_minimal.h"
#include <cstdint>


struct MinerConfig {
    Bytes coinbase_pub; //payout pubkey
    uint32_t bits = 18;
    int interval_ms = 100; //pause between attempts when no tx
};

void miner_loop(ChainState& cs, MinerConfig& cfg, P2PNode& p2p);
bool mine_once(ChainState& cs, MinerConfig& cfg, P2PNode& p2p);