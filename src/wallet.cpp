#include "wallet.h"
#include "crypto.h"
#include "../third_party/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
        std::cerr << "Wallet exists: " << path << "\n";
        throw std::runtime_error("Wallet already exists: " + path.string());
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

    std::cout << "✅ Wallet '" << name << "' created!\n";
    std::cout << "Address: " << wallet_json["address_hex"] << "\n";

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