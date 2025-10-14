#include "wallet.h"
#include "crypto.h"
#include "../third_party/json.hpp"
#include <fstream>

bool Wallet::create_wallet(std::string name) {

    // generate keypair 
    KeyPair keys = generate_keypair();

    // create a JSON object 
    nlohmann::json json;

    // generate an adress for it 
    std::string address = hex(hash160(keys.pub));

    // put the name on the object 
    json["name"] = name;
    json["private_key"] = keys.priv;
    json["public_key"] = keys.pub;
    json["address"] = address;

    // save it to ./wallet/${name}.json
    std::ofstream file("wallets/" + name + ".json");
    file << json.dump(4);
    file.close();


    // if success return true 
    return true;
}