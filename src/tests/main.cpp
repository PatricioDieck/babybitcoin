
#include <cstdint>
#include <iostream>
#include <chrono>
#include "../utils/common.h"
#include "../utils/crypto.h"
#include "../script_min.h"
#include "../tx.h"
#include "../block.h"
#include "../chain.h"


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
        Transaction coinbase = make_coinbase(miner.pub, 50*COIN, Bytes{0x01,0x02,0x03});
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

        Bytes from_spk = p2pkh_hash_from_script(prev_spk);
        Bytes from_pub = hash160(miner.pub);
        std::cout << "prevout h160: " << hex(from_spk) << "\n";
        std::cout << "pubkey  h160: " << hex(from_pub) << "\n";

        // verify the P2PKH input 0 using the prevout's scriptPubKey 
        bool ok = verify_p2pkh_input(spend, 0, prev_spk);
        std::cout << "Spend tx input verify: " << (ok ? "OK" : "FAIL") << "\n";
        Hash256 spend_txid = spend.txid();
        std::cout << "Spend txid: " << hex_hash(spend_txid) << "\n";

        //  4) Build a block with just the coinbase (happy path), mine it
        Block b;
        b.header.version = 1;
        b.header.prev_block = Hash256{};
        b.header.timestamp = static_cast<uint32_t>(std::time(nullptr));
        b.header.bits = 18; // easy difficulty for quick demo (adjust to mine faster)
        b.vtx.push_back(coinbase);

        bool mined = mine_block(b);
        std::cout << "------ Block 1 ------\n";
        std::cout << "Mined: " << (mined ? "YES" : "NO") << "\n";
        std::cout << "Block merkle root: " << hex_hash(b.header.merkle_root) << "\n";
        std::cout << "Block hash:        " << hex_hash(b.header.hash()) << "\n";
        std::cout << "Nonce:             " << b.header.nonce << "\n";
        
        ChainState cs;
        bool accepted = cs.accept_block(b);
        std::cout << "Genesis block accepted into the chain: " << (accepted ? "YES" : "NO") << "\n";
        std::cout << "\n\n";

        // 5) Build a block that processes the spending transaction 
        Block b1;
        b1.header.version = 1;
        b1.header.prev_block = b.header.hash();
        b1.header.timestamp = static_cast<uint32_t>(std::time(nullptr));
        b1.header.bits = 18;
        
         // Block 2: Spend transaction with its own coinbase  
        Transaction coinbase2 = make_coinbase(miner.pub, 50*COIN, Bytes{0x04,0x05,0x06});
        b1.vtx.push_back(coinbase2);  // Must have coinbase first
        b1.vtx.push_back(spend);

        bool mined1 = mine_block(b1);
        std::cout << "------ Block 2 ------\n";
        std::cout << "Mined Transaction: " << (mined1 ? "YES" : "NO") << "\n";
        std::cout << "Block merkle root: " << hex_hash(b1.header.merkle_root) << "\n";
        std::cout << "Block hash:        " << hex_hash(b1.header.hash()) << "\n";
        std::cout << "Nonce:             " << b1.header.nonce << "\n";
        bool accepted1 = cs.accept_block(b1);
        std::cout << "Transaction accepted into chain: " << (accepted1 ? "YES" : "NO") << "\n";
        std::cout << "\n\n";


        // Check miner + Alice's balance 
        int64_t bal = cs.balance_for_pubkey(miner.pub);
        std::cout << "Miner balance (sats): " << bal << "\n";
        int64_t alice_bal = cs.balance_for_pubkey(alice.pub);
        std::cout << "Alice's balance (sats): " << alice_bal << "\n";

        if (!mined) { 
            std::cout << "Tip: lower b.header.bits if this takes too long (e.g. 16 or 14)\n";
        }

        std::cout << "==Done== \n";
        return mined ? 0 : 1;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }
}