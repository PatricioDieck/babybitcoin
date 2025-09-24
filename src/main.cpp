
#include <cstdint>
#include <iostream>
#include <chrono>
#include "common.h"
#include "crypto.h"
#include "script_min.h"
#include "tx.h"
#include "block.h"

// Pretty-print a 32-byte hash in the common BE (human) form.
// Internally we store Hash256 in little-endian; reversing makes it readable.
static std::string hex_hash(const Hash256& h) {
    Bytes be(h.rbegin(), h.rend());
    return hex(be);
  }

static Transaction make_coinbase_tx ( const Bytes &miner_pubkey, int64_t reward, const Bytes &arbitrary) {
    Transaction tx;
    TxIn in;
    in.prevout = OutPoint{Hash256{}, 0xffffffff};
    in.scriptSig = arbitrary;
    tx.vin.push_back(in);

    TxOut out;
    out.value = reward;
    out.scriptPubKey = p2pkh_script(miner_pubkey);
    tx.vout.push_back(out);

    return tx;
}

int main() {
    try{
        std::cout << "== Babycoin sanity check ==\n";

        // 1) Generate keypairs: miner and recipient 
        KeyPair miner = generate_keypair();
        KeyPair alice = generate_keypair();
        std::cout << "Miner pub (33B): " << miner.pub.size() << " bytes\n";
        std::cout << "Alice pub (33B): " << alice.pub.size() << " bytes\n";

        // 2) build a minimal coinbase tx paying miner (50 BTC)
        constexpr int64_t COIN = 100'000'000;
        Transaction coinbase = make_coinbase_tx(miner.pub, 50*COIN, Bytes{0x01,0x02,0x03});
        Hash256 cb_txid = coinbase.txid();
        std::cout << "Coinbase txid:" << hex_hash(cb_txid) << "\n";

        // 3) signature sanity check - create a mock spend from a p2pkh prevout owned by miner
        // build a prevout's scriptPubKey (lock to miner's pubkey hash)
        Bytes prev_spk = p2pkh_script(miner.pub);

        // new tx spending that fake prevout--> to Alice
        Transaction spend;
        // input 0: fake prevout
        TxIn in0;
        in0.prevout = OutPoint{ cb_txid, 0};
        // scriptSig empty for now- we'll fill after signing 
        spend.vin.push_back(in0);

        // Output 0 : pay Alice 10 BTC
        TxOut out0;
        out0.value = 10 * COIN;
        out0.scriptPubKey = p2pkh_script(alice.pub);
        spend.vout.push_back(out0);

        // Change back to  miner (40 BTC) just to show typical structure
        TxOut change;
        change.value = 40 * COIN;
        change.scriptPubKey = p2pkh_script(miner.pub);
        spend.vout.push_back(change);

        // Compute SIGHASH_ALL for input 0 using the prevout's locking script
        Hash256 h = sighash_all(spend, 0, prev_spk);
        // sign with miner's private key (owner of prevout)
        Bytes sig_der = ecdsa_sign(h, miner.priv);
        // build scriptSig: <sig+SIGHASH_ALL><pubkey>
        spend.vin[0].scriptSig = build_p2pkh_scriptSig(sig_der, miner.pub);

        // verify the P2PKH input 0 using the prevout's scriptPubKey 
        bool ok = verify_p2pkh_input(spend, 0, prev_spk);
        std::cout << "Spend tx input verify: " << (ok ? "OK" : "FAIL") << "\n";
        Hash256 spend_txid = spend.txid();
        std::cout << "Spend txid: " << hex_hash(spend_txid) << "\n";

        //  4) Build a block with just the coinbase (happy path), mine it
        Block b;
        b.header.version = 1;
        b.header.prev_block = Hash256{};
        b.header.timestamp = static_cast<uint32_t>(std::chrono::seconds(std::time(nullptr)).count());
        b.header.bits = 18; // easy difficulty for quick demo (adjust to mine faster)
        b.vtx.push_back(coinbase);

        bool mined = mine_block(b);
        std::cout << "Mined: " << (mined ? "YES" : "NO") << "\n";
        std::cout << "Block merkle root: " << hex_hash(b.header.merkle_root) << "\n";
        std::cout << "Block hash:        " << hex_hash(b.header.hash()) << "\n";
        std::cout << "Nonce:             " << b.header.nonce << "\n";

        if (!mined) { 
            std::cout << "Tip: lower b.header.bits if this takes too long (e.g. 16 or 14)\n";
        }

        std::cout << "==Done== \n";
        return mined ? 0 :1;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}