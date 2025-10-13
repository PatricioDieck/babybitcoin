#include "block.h"
#include "chain.h"
#include "common.h"
#include "crypto.h"
#include "script_min.h"
#include "p2p_minimal.h"
#include "tx.h"
#include "p2p_minimal.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>


int main(int argc, char** argv) {
    std::cout << "BABYBITCOIN IGNITE\n";

    // --- CLI (optional) ---
    uint16_t listen_port = 18444;
    std::vector<PeerAddress> peers;

    // starting on arg 1 bc arg 0 is program name
    for (int i=1;i<argc;i++) {
        std::string a = argv[i];
        if (a == "--port" && i+1 < argc) {
            listen_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (a == "--connect" && i+1 < argc) {
            std::string arg = argv[++i];
            PeerAddress peer;
            
            auto pos = arg.find(':');

            peer.host = arg.substr(0, pos);
            peer.port = static_cast<uint16_t>(std::stoi(arg.substr(pos+1)));
            peers.push_back(peer);
            
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
        }
    }

    // --- Chain + P2P ---
    ChainState cs;

    P2PNode p2p(cs);
    P2PConfig cfg;
    cfg.demo_mode = true;            // <= turn on demo
    cfg.demo_tx_to_send = 5;         // send 5 demo TXs
    cfg.demo_ms_between = 800;       // ~0.8s apart
    cfg.listen_port = listen_port;
    for (auto& peer : peers) cfg.peers.push_back(peer);
    if (!p2p.start(cfg)) {
        std::cerr << "Failed to start P2P node on port " << listen_port << "\n";
        return 1;
    }

    // --- Genesis (coinbase -> miner) ---
    KeyPair miner = generate_keypair();
    KeyPair alice = generate_keypair();

    constexpr int64_t COIN = 100'000'000;

    Transaction coinbase_tx = make_coinbase(miner.pub, 50 * COIN, Bytes{0x01,0x02,0x03});

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

    std::cout << "miner balance: " << cs.balance_for_pubkey(miner.pub) << "\n";
    std::cout << "alice balance: " << cs.balance_for_pubkey(alice.pub) << "\n";

    // --- Spend from genesis to Alice (signed before gossip) ---
    Transaction spend;

    TxIn in0; in0.prevout = OutPoint{ coinbase_tx.txid(), 0 };
    spend.vin.push_back(in0);

    TxOut to_alice; to_alice.value = 4 * COIN; to_alice.scriptPubKey = p2pkh_script(alice.pub);
    TxOut change;   change.value   = 1 * COIN; change.scriptPubKey   = p2pkh_script(miner.pub);
    spend.vout.push_back(to_alice);
    spend.vout.push_back(change);

    const Bytes prevout_spk = coinbase_tx.vout[0].scriptPubKey;
    Hash256 h = sighash_all(spend, 0, prevout_spk);
    Bytes sig_der = ecdsa_sign(h, miner.priv);
    spend.vin[0].scriptSig = build_p2pkh_scriptSig(sig_der, miner.pub);

    bool ok = verify_p2pkh_input(spend, 0, prevout_spk);
    std::cout << "Spend valid (local sanity): " << (ok ? "YES" : "NO") << "\n";

    // Gossip the signed transaction (no validation at P2P)
    p2p.broadcast_local_tx(spend);

    // --- Next block with new coinbase + spend ---
    Transaction coinbase2 = make_coinbase(miner.pub, 50 * COIN, Bytes{0x04,0x05,0x06});

    Block block1;
    block1.header.version     = 1;
    block1.header.prev_block  = genesis.header.hash();
    block1.header.timestamp   = static_cast<uint32_t>(std::time(nullptr));
    block1.header.bits        = 18;
    block1.vtx.push_back(coinbase2);
    block1.vtx.push_back(spend);

    bool mined1 = mine_block(block1);
    std::cout << "Block1 mined: " << (mined1 ? "YES" : "NO") << "\n";

    bool accepted1 = cs.accept_block(block1);
    std::cout << "Block1 accepted: " << (accepted1 ? "YES" : "NO") << "\n";
    if (accepted1) p2p.broadcast_local_block(block1);

    std::cout << "Balances after Block1:\n";
    std::cout << "  miner: " << cs.balance_for_pubkey(miner.pub) << "\n";
    std::cout << "  alice: " << cs.balance_for_pubkey(alice.pub) << "\n";

    // Keep node alive so peers can connect / exchange
    for (;;) std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
