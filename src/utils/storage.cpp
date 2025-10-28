#include "storage.h"
#include "serialize.h"
#include "common.h"
#include "../block.h"
#include "../chain.h"
#include "../../third_party/json.hpp"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

using nlohmann::json;


static std::string chain_file_path(const std::string& datadir) {
    std::filesystem::create_directories(datadir);
    return (std::filesystem::path(datadir) / "chain.json").string();
}

bool save_chain(const ChainState& cs, const std::string& datadir){
    //  we are going to save the Blocks as JSON
    try {
        json j;
        j["height"] = cs.height();
        // std::cout << "SUCCESS: Height saved to JSON object: " << j["height"] << "\n";

        // TIP -> BE hex for readability
        Hash256 tip = cs.tip_hash();
        Bytes be(tip.rbegin(), tip.rend());
        j["tip"] = hex(be);
        // std::cout << "SUCCESS: Tip saved to JSON object: " << j["tip"] << "\n";

        // BLOCK -> hex of serialized bytes
        json blocks = json::array();
        for (uint32_t i = 0 ; i< cs.height() ; ++i){
            const Block* b = cs.block_by_index(i);
            if (!b) break;
            Bytes ser = b->serialize();
            blocks.push_back(hex(ser));
        }
        j["blocks"] = blocks;
        // std::cout << "SUCCESS: Prepared blocks array of size " << blocks.size() << "\n";

        const std::string path = chain_file_path(datadir);
        std::ofstream f(path, std::ios::trunc);
        if (!f.good()) {
            std::cerr << "ERROR: Unable to open file for write: " << path << "\n";
            return false;
        }
        f << j.dump(2);
        f.flush();
        if (!f.good()) {
            std::cerr << "ERROR: Write failed for: " << path << "\n";
            return false;
        }
        std::cout << "SUCCESS: Chain written to " << path << "\n";
        return true;
    } catch (...) {
        std::cerr << "ERROR: Exception while saving chain JSON" << "\n";
        return false;
    }
}


bool load_chain(ChainState& cs, const std::string& datadir){
    // try to load the chain from the JSON file
    try {
        const std::string path = chain_file_path(datadir);
        std::ifstream f(path);
        if (!f) return false;
        json j; f >> j;
        if (!j.contains("blocks") || !j["blocks"].is_array()) return false;

        for (const auto& s : j["blocks"]) {
            std::string hexstr = s.get<std::string>();
            Bytes raw = from_hex(hexstr);
            const uint8_t* p = raw.data();
            const uint8_t* end = raw.data() + raw.size();
            Block b = parse_block(p, end);
            if (p != end) return false;
            if (!cs.accept_block(b)) return false;
        }
        std::cout << "SUCCESS: Chain loaded from " << path << "\n";
        return true;
    } catch (...) { return false; }
}
