


#include "block.h"
#include "chain.h"
#include "common.h"
#include "crypto.h"
#include "script_min.h"
#include "tx.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/types.h>


// this is the workspace where we think through the entirety of this mf in a step by step manner

int main(){
    std::cout << "BABYBITCOIN IGNITE \n";

    // -----------------------------------------------------------------------
    // PHASE 1: Genesis Block (coinbase + reward to miner) 
    // -----------------------------------------------------------------------

    /*
        Mocking the generation of babybitcoin to understand this ->
        - we need to get our hands dirty and mess around with stuff
            - thats why this is a test branch 
        
        The steps to initalize the chain are as follows:
        1) inialize chain state 
        2) create a keypair for all users (miner and alice for now) 
        3) generate a genesis block with inital coin mint
        4) create a coinbase transaction
            - TxIn, TxOut
            - for every TxOut we need to define a locking script for the new owner's privkey
        5) put the coinbase Tx into a block
        6) mine the block- proof of work
        7) check the balances of each agent
        8) if transaction went through --> success
    */

    // 1) inialize chain state 
    ChainState cs;

    // 2) create keypair for each user 
    KeyPair miner = generate_keypair();
    KeyPair alice = generate_keypair();

    // 3) generate genesis block (needs to have a coinbase transaction)
    Transaction coinbase_transaction;

    // create inputs- index is 0xffffffff to indicate a coinbase transaction (little endian at highest byte)
    TxIn cb_in;
    cb_in.prevout = OutPoint{Hash256{}, 0xffffffff};
    cb_in.scriptSig = Bytes{0x01, 0x02,0x03};
    coinbase_transaction.vin.push_back(cb_in);

    // create the outputs
    TxOut cb_out;
    cb_out.value = 500'000'000;
    cb_out.scriptPubKey = p2pkh_script(miner.pub);
    coinbase_transaction.vout.push_back(cb_out);

    // 5) lets add the transaction to a block and mine it 
    Block genesis;
    genesis.header.version = 1;
    genesis.header.prev_block = Hash256{};
    genesis.header.timestamp = static_cast<uint32_t>(std::time(nullptr));
    genesis.header.bits = 18;

    // add transaction to genesis block 
    genesis.vtx.push_back(coinbase_transaction);

    // 6) mine geneis block 
    bool mined = mine_block(genesis);

    std::cout << "Genesis block mined w/ coinbase: " <<( mined ? "YES" : "NO") << "\n";

    // add genesis to chain
    bool accepted = cs.accept_block(genesis);

    std::cout << "Genesis block accepted to blockchain: " <<( accepted ? "YES" : "NO")<< "\n";

    // 7) print out the balances of each person 
    int64_t miner_balance = cs.balance_for_pubkey(miner.pub);
    int64_t alice_balance = cs.balance_for_pubkey(alice.pub);

    std::cout << "miner's balance:" + std::to_string(miner_balance)<< "\n";
    std::cout << "alice's balance:" + std::to_string(alice_balance)<< "\n";

    // -----------------------------------------------------------------------
    // PHASE 2: Transaction 
    // -----------------------------------------------------------------------

    /*
        We now have a genesis block with a coinbase TX, time to add 
        a normal transaction from the miner to alice. this will be different
        due to the fact that we need to add a locking script and a OutPoint from
        which to connect the 1st block to the second (+ a new coinbase!)

        so the way we're going to do this:
        
        1) create a new coinbase
        2) create a new block
        3) add coinbase to it 
        4) create new transaction
        5) add the miner's UTxO's from genesis block to it 
        6) sign the transaction
        7) add to block and mine it
        8) check the balances 
    */

    /*

    Coinbase transaction- TxIn and TxOut executed together as a static block reward at this point (vs dynamic block reward) - practically mints coins/adds them to circulation
    */
    Transaction coinbase1;

    TxIn cb1_in;
    cb1_in.prevout = OutPoint{Hash256{}, 0xffffffff};
    cb1_in.scriptSig = Bytes{0x01,0x02,0x03};

    TxOut cb1_out;
    cb1_out.value = 500'000'000;
    cb1_out.scriptPubKey = p2pkh_script(miner.pub);

    //push inputs and outputs into transaction
    coinbase1.vin.push_back(cb1_in);
    coinbase1.vout.push_back(cb1_out);

    // -----------------------------------------------------------------------
    // Signing Transactions: Build a standard spend (miner -> alice) from the coinbase UTXO
    // -----------------------------------------------------------------------

    /*
        Signing transactions is not the easiest mental model. coinbase tx is different from normal txs since it doesn't have an input.
        Vocab ->
            - scriptPubkey (p2pkh_script)- lock an spend to the new owner (pubKey)
            - scriptSig (unlocking script)- new owner unlocks the spend for themselves 
    */

    // 1) Reference the UTXO we are spending (the coinbase output 0 we just created).
    //    Inputs carry the "key" (scriptSig) proving we can spend a previous output.
    Transaction spend;
    
    // 1.a) Create the input pointing to the previous transaction's output (UTXO).
    TxIn spend_inputs;
    spend_inputs.prevout = OutPoint{coinbase1.txid(), 0}; // prev txid + output index
    // NOTE: scriptSig stays empty for now; we'll fill it AFTER we compute the sighash and sign.
    spend.vin.push_back(spend_inputs);

    // 2) Create new outputs for this transaction.
    //    Outputs carry the "lock" (scriptPubKey) for the next owner(s).
    //    Each output's scriptPubKey is a P2PKH lock built from the reciever's pubkey.
    TxOut to_alice;
    to_alice.value = 400'000'000; // amount to Alice (4.00 BBTC here)
    to_alice.scriptPubKey = p2pkh_script(alice.pub); // LOCK: "prove you own Alice's privkey"
    spend.vout.push_back(to_alice);

    // Optional: change back to miner (remaining 1.00 BBTC in this example; fee = 0).
    TxOut change;
    change.value = 100'000'000;   // change back to miner
    change.scriptPubKey = p2pkh_script(miner.pub);   // LOCK: "prove you own miner's privkey"
    spend.vout.push_back(change);

    // 3) Connecting the spend transaction to the previous transaction
    // Sighash commits whole tx. 
        //For the input being signed (index 0), set its scriptSig to the PREVIOUS OUTPUT'S scriptPubKey,
        //pass the PREVOUT's scriptPubKey (the lock we are trying to open)

    const Bytes prevout_spk = coinbase1.vout[0].scriptPubKey;
    Hash256 h = sighash_all(spend, 0, prevout_spk);

    // 4) UNLOCK: tx is valid w prev owner's private key 
    Bytes sig_der = ecdsa_sign(h, miner.priv);
    
    // 5) Build the unlocking script (scriptSig) to new owner:
        // signing the hash of the entire transaction with the private key of the owner of the previous output
        //scriptSig = <signature+SIGHASH_ALL> <pubkey>
        //Inputs carry the "key"; this does NOT carry forward to future spends.
    spend.vin[0].scriptSig = build_p2pkh_scriptSig(sig_der, miner.pub);

    // 6) (Optional) Sanity-check the spend against the prevout's lock.
        //Verifier recomputes the same SIGHASH and checks the signature and pubkey-hash match.
    bool ok = verify_p2pkh_input(spend, 0, prevout_spk);
    std::cout << "Spend valid (input 0): " << (ok ? "YES" : "NO") << "\n";


    // ------------------------------------------------------------
    // Add the coinbase + spend transaction to a block and mine it
    // ------------------------------------------------------------

    // create the block after genesis (add coinbase and p2p spend transaction)
    Block block1;
    block1.header.prev_block = genesis.header.hash();
    block1.header.timestamp = static_cast<uint32_t>(std::time(nullptr));
    // not including number of bits or version as they already have default values
    block1.vtx.push_back(coinbase1);
    block1.vtx.push_back(spend);

    // mine the block and then get it into the chain
    bool mined1 = mine_block(block1);
    std::cout << "First block mined w/ coinbase + spend: " <<( mined1 ? "YES" : "NO") << "\n";

    // add block 1 to chain
    bool accepted1 = cs.accept_block(block1);
    std::cout << "First block accepted to blockchain: " <<( accepted1 ? "YES" : "NO")<< "\n";

    // 7) print out the balances of each person 
    int64_t miner_balance1 = cs.balance_for_pubkey(miner.pub);
    int64_t alice_balance1 = cs.balance_for_pubkey(alice.pub);

    std::cout << "Balances after BLOCK 1- with a coinbase and a spend transaction \n";
    std::cout << "miner's balance:" + std::to_string(miner_balance1)<< "\n";
    std::cout << "alice's balance:" + std::to_string(alice_balance1)<< "\n";

    /*
    Output should be:
    BABYBITCOIN IGNITE 
        Genesis block mined w/ coinbase: YES
        Genesis block accepted to blockchain: YES
        miner's balance:500000000
        alice's balance:0
        Spend valid (input 0): YES
        First block mined w/ coinbase + spend: YES
        First block accepted to blockchain: YES
        Balances after BLOCK 1- with a coinbase and a spend transaction
        miner's balance:600000000
        alice's balance:400000000
    */
    
}