#include "wallet.h"
#include "crypto.h"
#include "../third_party/json.hpp"
#include <fstream>
#include <iostream>
#include <string>

bool Wallet::create_wallet(std::string name) {

    // generate keypair 
    KeyPair keys = generate_keypair();

    // Create wallets directory if it doesn't exist
    std::filesystem::create_directories("wallets");
    
    // generate an adress for it 
    std::string address = hex(hash160(keys.pub));
    
    nlohmann::json wallet_json;
    wallet_json["name"] = name;
    wallet_json["private_key"] = hex(keys.priv);
    wallet_json["public_key"] = hex(keys.pub);
    wallet_json["address"] = address;

    if (std::filesystem::exists("wallets/" + name + ".json")) { 
        std::cout << "Wallet '" << name << "' already exists\n";
        return false;
    }

    // save it to ./wallet/${name}.json
    std::ofstream file("wallets/" + name + ".json");
    file << wallet_json.dump(4);
    file.close();

    std::cout << "✅ Wallet '" << name << "' created!\n";
    std::cout << "   Address: " << address << "\n";

    // if success return true 
    return true;
}

bool Wallet::delete_wallet(std::string name ){

    // delete the wallet from the wallets directory
    if (std::filesystem::remove("wallets/" + name + ".json")) {
        std::cout << "Wallet '" << name << "' deleted successfully\n";
        return true;
    } else {
        std::cout << "Failed to delete wallet '" << name << "'\n";
        return false;
    }   
}

KeyPair Wallet::load_wallet(std::string name ) {
    // retrieve the wallet from the wallets folder 
    std::ifstream file("wallets/" + name + ".json");

    if (!file) {
        std::cout << "Wallet '" << name << "' not found\n";
        return KeyPair();
    }

    // NTS- learn how to manage files w ifstream api
    nlohmann::json wallet_json;
    file >> wallet_json;
    file.close();

    // we need to convert the JSON to Bytes
    Bytes priv = from_hex(wallet_json["private_key"]);
    Bytes pub = from_hex(wallet_json["public_key"]);
    
    KeyPair keys;
    
    keys.priv = priv;
    keys.pub = pub;
    return keys;
}