#include "block.h"
#include "chain.h"
#include "utils/common.h"
#include "utils/crypto.h"
#include "p2p_minimal.h"
#include "tx.h"
#include "p2p_minimal.h"
#include "cli.cpp"
#include "wallet.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
       
    constexpr int64_t COIN = 100'000'000;

    // load the chain from the JSON file
    ChainState cs;
    std::string datadir = "data";
    cs.load(datadir) || std::cout << "Starting fresh - no existing chain data found\n";

    P2PConfig cfg = parse_command_line(argc, argv);
    P2PNode p2p(cs);
    if (!p2p.start(cfg)) {
        std::cerr << "Failed to start P2P node on port " << cfg.listen_port << '\n';
        return 1; // or handle error
    }

    WalletStore ws;
    // --- load the wallets  ---
    Wallet miner = ws.create_wallet("miner");
    Wallet alice = ws.create_wallet("alice");

    Transaction coinbase_tx = make_coinbase(miner.keys.pub, 50 * COIN, Bytes{0x01,0x02,0x03});

    Block genesis;
    genesis.header.version     = 1;
    genesis.header.prev_block  = Hash256{};
    genesis.header.timestamp   = static_cast<uint32_t>(std::time(nullptr));
    genesis.header.bits        = 18;
    genesis.vtx.push_back(coinbase_tx);

    bool mined = mine_block(genesis);
    std::cout << "Genesis mined: " << (mined ? "YES" : "NO") << "\n";

    bool accepted = cs.accept_block(genesis);
    std::cout << "Genesis accepted: " << (accepted ? "YES" : "NO") << "\n";
    if (accepted) p2p.broadcast_local_block(genesis);

   // Determine mode based on command line arguments
   bool interactive_mode = std::any_of(argv + 1, argv + argc, [](const char* arg) {
       return std::string(arg) == "--interactive" || std::string(arg) == "-i";
   });
   
   if (interactive_mode) {
       run_interactive_cli(cs, p2p);
       return 0;
   }

    // Keep node alive so peers can connect / exchange
    for (;;) std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
