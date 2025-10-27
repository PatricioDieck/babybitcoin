

#include "wallet.h"
#include "utils/crypto.h"
#include "utils/common.h"
#include "tx.h"
#include "script_min.h"
#include "p2p_minimal.h"
#include "../third_party/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using nlohmann::json;

WalletStore::WalletStore(std::filesystem::path dir) : dir_(std::move(dir)) {
    std::filesystem::create_directories(dir_);
}

// dev-friendly "address"- consider Base58Check later.
std::string WalletStore::address_hex(const Bytes& pub) {
    return hex(hash160(pub)); // dev-only
  }

Wallet WalletStore::create_wallet(const std::string& name) {
    auto path = dir_ / (name + ".json");
    if (std::filesystem::exists(path)) {
        std::cout << "Wallet exists: " << path << "\n";
        std::cout << "Loading existing wallet: " << name << "\n";
        return WalletStore::load_wallet(name);
    }

    Wallet wallet;
    wallet.name = name;
    wallet.keys = generate_keypair();

    json wallet_json;
    wallet_json["version"]     = 1;
    wallet_json["name"]        = name;
    wallet_json["private_key"] = hex(wallet.keys.priv);
    wallet_json["public_key"]  = hex(wallet.keys.pub);
    wallet_json["address_hex"] = address_hex(wallet.keys.pub);

    // save it to ./wallet/${name}.json
    std::ofstream file(path);
    file << wallet_json.dump(4);
    file.close();

    return wallet;
}

Wallet WalletStore::load_wallet(const std::string& name) const {
    auto path = dir_ / (name + ".json");

    // retrieve the wallet from the wallets folder 
    std::ifstream file(path);

    if (!file) {
        throw std::runtime_error("Wallet not found: " + path.string());
    };

    // NTS- learn how to manage files w ifstream api
    json wallet_json;
    file >> wallet_json;
    file.close();

    Wallet wallet;
    // we need to convert the JSON to Bytes
    wallet.name = wallet_json["name"];
    wallet.keys.priv = from_hex(wallet_json["private_key"]);
    wallet.keys.pub = from_hex(wallet_json["public_key"]);
    
    return wallet;
}

bool WalletStore::remove_wallet(std::string name) const {
    auto path = dir_ / (name + ".json");
    return std::filesystem::remove(path);
}

std::vector<std::string> WalletStore::list_wallets() const {
    // get all of the names of files in wallets dir 
    std::vector<std::string> out;

    if (!std::filesystem::exists(dir_)) return out;

    for (auto& entry : std::filesystem::directory_iterator(dir_)){
        if (!entry.is_regular_file()) continue;
        auto path = entry.path();
        out.push_back(path.stem().string());
    }
    return out;
};

Transaction WalletStore::send(const std::string& fromName,
                              const std::string& toAddressHex,
                              int64_t amount,
                              ChainState& cs,
                              int64_t fee) {

    // Load sender wallet and derive h160 for selection and pubkey for signing
    Wallet from = load_wallet(fromName);
    const Bytes& fromPub = from.keys.pub;

    // Decode destination address (dev: it's hex of HASH160(pub))
    Bytes to_h160 = from_hex(toAddressHex);
    if (to_h160.size() != 20) throw std::runtime_error("toAddressHex must be 20-byte h160 hex" + std::to_string(to_h160.size()));
    std::cout << "to_h160: " << hex(to_h160) << "\n";

    // Ask chain for coins to cover amount + fee
    int64_t target = amount + fee;
    auto coins = cs.gather_coins(fromPub, target);
    int64_t gathered = 0;
    for (auto& c : coins) {
        gathered += c.value;
        std::cout << "COIN: " << c.value << "\n";
    }
    if (gathered < target) throw std::runtime_error("insufficient funds");
    // std::cout << "GATHERED: " << gathered << "\n";

    // Build outputs: main payment + change if needed
    Transaction tx;
    int64_t used = 0;
    for (const auto& c : coins) {
      // scriptSig is empty for now- we'll fill it after signing
      tx.vin.push_back(TxIn{ c.prevout, Bytes{}, 0xffffffff });
      std::cout << "COIN pushed into the tx: " << c.value << "\n";
      used += c.value;
      std::cout << "USED: " << used << "\n";
      if (used >= target) break;
    }

    // Payment output (to P2PKH of destination h160)
    Bytes to_spk = build_p2pkh_script_from_h160(to_h160);
    
    tx.vout.push_back(TxOut{amount, to_spk});

    // Change back to sender if any
    int64_t change = gathered - target;
    std::cout << "CHANGE: " << change << "\n";
    if (change > 0) {
        Bytes change_spk = p2pkh_script(fromPub);
        tx.vout.push_back(TxOut{change, change_spk});
        std::cout << "CHANGE pushed into the tx: " << change << "\n";
    }

    // Sign each input: need the prevout scriptPubKey for sighash
    // We can reconstruct spk as p2pkh(fromPub) because gather_coins filtered on it.
    Bytes prev_spk = p2pkh_script(fromPub);
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        Hash256 h = sighash_all(tx, i, prev_spk);
        Bytes sig = ecdsa_sign(h, from.keys.priv);
        tx.vin[i].scriptSig = build_p2pkh_scriptSig(sig, fromPub);
    }

    // Final sanity: verify inputs against prevout scripts (dev check)
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        bool verified = verify_p2pkh_input(tx, i, prev_spk);
        if (!verified) {
            std::cout << "TX VERIFIED: " << (verified ? "YES" : "NO") << "\n";
            throw std::runtime_error("internal signing verification failed");
        }
    }

    std::cout << "TX ID: " << be_hex_key_(tx.txid()) << "\n";
    std::cout << "TX SIZE: " << tx.serialize().size() << "\n";
    std::cout << "TX VIN SIZE: " << tx.vin.size() << "\n";
    std::cout << "TX VOUT SIZE: " << tx.vout.size() << "\n";
    
    return tx;
}

Transaction WalletStore::send_and_broadcast(const std::string& fromName,
                                            const std::string& toAddressHex,
                                            int64_t amount,
                                            ChainState& cs,
                                            P2PNode& p2p,
                                            int64_t fee) {
    Transaction tx = send(fromName, toAddressHex, amount, cs, fee);
    p2p.broadcast_local_tx(tx);
    return tx;
}

  

// bool Wallet::create_wallet(std::string name) {

//     // generate keypair 
//     KeyPair keys = generate_keypair();

//     // Create wallets directory if it doesn't exist
//     std::filesystem::create_directories("wallets");
    
//     // generate an adress for it 
//     std::string address = hex(hash160(keys.pub));
    
//     nlohmann::json wallet_json;
//     wallet_json["name"] = name;
//     wallet_json["private_key"] = hex(keys.priv);
//     wallet_json["public_key"] = hex(keys.pub);
//     wallet_json["address"] = address;

//     if (std::filesystem::exists("wallets/" + name + ".json")) { 
//         std::cout << "Wallet '" << name << "' already exists\n";
//         return false;
//     }

//     // save it to ./wallet/${name}.json
//     std::ofstream file("wallets/" + name + ".json");
//     file << wallet_json.dump(4);
//     file.close();

//     std::cout << "✅ Wallet '" << name << "' created!\n";
//     std::cout << "   Address: " << address << "\n";

//     // if success return true 
//     return true;
// }

// bool Wallet::delete_wallet(std::string name ){
//     return std::filesystem::remove("wallets/" + name + ".json");
// }

// KeyPair Wallet::load_wallet(std::string name ) {
//     // retrieve the wallet from the wallets folder 
//     std::ifstream file("wallets/" + name + ".json");

//     if (!file) {
//         std::cout << "Wallet '" << name << "' not found\n";
//         return KeyPair();
//     }

//     // NTS- learn how to manage files w ifstream api
//     nlohmann::json wallet_json;
//     file >> wallet_json;
//     file.close();

//     // we need to convert the JSON to Bytes
//     Bytes priv = from_hex(wallet_json["private_key"]);
//     Bytes pub = from_hex(wallet_json["public_key"]);
    
//     KeyPair keys;
    
//     keys.priv = priv;
//     keys.pub = pub;
//     return keys;
// }