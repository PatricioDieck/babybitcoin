

#include "miner.h"
#include "block.h"
#include "chain.h"
#include "storage.h"
#include "tx.h"
#include "common.h"
#include <vector>
#include <iostream>


bool mine_once(ChainState& cs, MinerConfig& cfg, P2PNode& p2p) {
    // snapshot mempool
    std::vector<Transaction> txs = p2p.mempool_snapshot();

    // create the coinbase of block- we can later set the block subsidy at ChainState::SUBSIDY
    Transaction coinbase = make_coinbase(cfg.coinbase_pub,0 , Bytes{0x00});

    // create the block 
    Block b;
    b.header.version = 1;
    b.header.prev_block = cs.tip_hash();
    b.header.timestamp = static_cast<uint32_t>(std::time(nullptr));
    b.header.bits = cfg.bits;

    b.vtx.clear();
    b.vtx.push_back(coinbase);

    // push all txs into the block 
    for (auto tx : txs) b.vtx.push_back(tx);

    if (!mine_block(b)){
        std::cerr << "ERROR: Failed to mine block" << "\n";
        return false;
    }
    if (!cs.accept_block(b)){
        std::cerr << "ERROR: Failed to accept block" << "\n";
        return false;
    }
    save_chain(cs, "data");
    p2p.broadcast_local_block(b);


    // Pretty-print mined block + transactions
    std::cout << "------ Block mined and accepted ------\n";

    // Block header hash (BE for readability)
    {
    Hash256 h = b.header.hash();
    Bytes be(h.rbegin(), h.rend());
    std::cout << "Block hash:        " << hex(be) << "\n";
    }

    // Merkle root (as stored, LE bytes)
    std::cout << "Merkle root (LE):  " 
            << hex(Bytes(b.header.merkle_root.begin(), b.header.merkle_root.end())) << "\n";

    std::cout << "Nonce:             " << b.header.nonce << "\n";
    std::cout << "Tx count:          " << b.vtx.size() << "\n";

    // List txs: txid (BE), total outputs
    for (size_t i = 0; i < b.vtx.size(); ++i) {
    const Transaction& tx = b.vtx[i];
    Hash256 txid = tx.txid();
    Bytes txid_be(txid.rbegin(), txid.rend());
    std::cout << "  tx[" << i << "] id: " << hex(txid_be)
                << "  vout=" << tx.vout.size() << "\n";
    for (size_t n = 0; n < tx.vout.size(); ++n) {
        const auto& o = tx.vout[n];
        std::cout << "    vout[" << n << "]: value=" << o.value
                << "  spk=" << hex(o.scriptPubKey) << "\n";
    }
    }
    p2p.prune_mined_txs(b);

    return true;   
};

// THIS MAY NOT WORK BC THE BITS NEEDED ARE DEFNED IN THE CONFIG FOR MINER AS WELL AS FOR THE BLOCK- TODO- DEFINE THIS IN ONE CENTRALIZED PLACE 

void miner_loop(ChainState& cs, MinerConfig& cfg, P2PNode& p2p){
    // loop over the mine once function 
    for(;;){
        if (!mine_once(cs, cfg, p2p)){
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.interval_ms));
        }
    }
}