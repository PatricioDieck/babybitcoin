


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

int main(){
    std::cout << "BABYBITCOIN IGNITE \n";

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
    
}