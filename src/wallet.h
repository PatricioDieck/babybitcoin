


#include "crypto.h"

class Wallet {
    KeyPair keys;
    std::string name; 
    Bytes adress;

public:
    bool create_wallet(std::string name);

    static bool delete_wallet(std::string name);




};

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