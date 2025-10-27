

#pragma once
#include <string>
#include <vector>
#include <filesystem>

#include "utils/crypto.h"
#include "utils/common.h"
#include "tx.h"
class ChainState;
class P2PNode;

// simple value object holding a keypair and friendly name
// address is derived (not stored) to avoid inconsistency
struct Wallet {
    std::string name;
    KeyPair keys;
};

// persistence and lookup for wallets in a directory
class WalletStore{
    public:

    explicit WalletStore(std::filesystem::path dir="wallets");

    // create a new wallet file (fail if it exists)
    // return created Wallet on success
    Wallet create_wallet(const std::string& name);

    // load existing wallet (name-> file). null if not found
    Wallet load_wallet(const std::string& name) const;

    // delete wallet. return true if removed.
    bool remove_wallet(std::string name) const;

    //list known wallet names (best-effort)
    std::vector<std::string> list_wallets() const;

    // dev-friendly "address" as hex(HASH160(pub))
    // consider Base58Check later.
    static std::string address_hex(const Bytes& pub);

    // Build and sign a P2PKH transaction from 'fromName' to 'toAddressHex' for 'amount'.
    // Uses ChainState for UTXO selection; throws on insufficient funds or missing wallet.
    Transaction send(const std::string& fromName, const std::string& toAddressHex, int64_t amount, ChainState& cs, int64_t fee = 1000);

    // Convenience: build/sign then broadcast via P2P. Mining is external.
    Transaction send_and_broadcast(const std::string& fromName,
                                   const std::string& toAddressHex,
                                   int64_t amount,
                                   ChainState& cs,
                                   P2PNode& p2p,
                                   int64_t fee = 1000);

private:    
    std::filesystem::path dir_;
    // static bool valid_name(const std::string& name);
    // std::filesystem::path file_for(const std::string& name) const;
};


// class Wallet {
//     KeyPair keys;
//     std::string name; 
//     Bytes adress;

// public:
//     static bool create_wallet(std::string name);

//     static bool delete_wallet(std::string name);

//     // return keys to transact with them
//     static KeyPair load_wallet(std::string name);
// };

/*

things necessary to create a wallet 
- create a wallet
    - save it to a .json file 
- delete a wallet (the json file)
- name a wallet
- get the balances
- transact with other wallets 

# 💡 Pro Tip: Start Small
    Don't try to build the "perfect" wallet. Start with:
        1. Save one keypair to a file
        2. Load it back and verify it works
        3. Create one transaction manually
        4. Add CLI parsing for one command

//move the KeyPair struct into here?

class Wallet {
    privateKey 
    publicKey

    // save the wallet into a wallet/<name>.json file
    bool create_wallet(string name)

    // remove the wallet from there
    bool delete_wallet()

    int get_balance(pubKey)

    Transaction create_transaction(toKey, fromKey)

private

    Transaction sign_transaction(const Transaction& tx) const;

    std::string get_adress() const;



}

*/