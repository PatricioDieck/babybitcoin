#include "script_min.h"
#include <stdexcept>

// Minimal opcodes we reference (for readability)
static constexpr uint8_t OP_DUP = 0x76, OP_HASH160 = 0xa9, OP_EQUALVERIFY = 0x88, OP_CHECKSIG = 0xac;

// P2PKH script from a 20-byte hash160.
// centralizing the common script construction logic.
Bytes build_p2pkh_script_from_h160(const Bytes& h160) {
    if (h160.size() != 20) throw std::runtime_error("h160 must be 20 bytes");
    Bytes s;
    s.push_back(OP_DUP);
    s.push_back(OP_HASH160);
    s.push_back(0x14);
    s.insert(s.end(), h160.begin(), h160.end());
    s.push_back(OP_EQUALVERIFY);
    s.push_back(OP_CHECKSIG);
    return s;
}

Bytes p2pkh_script(const Bytes& pubkey) {
    Bytes h160 = hash160(pubkey);
    return build_p2pkh_script_from_h160(h160);
}

Bytes p2pkh_hash_from_script(const Bytes& spk) {
    if (spk.size() == 25 && spk[0] == OP_DUP && spk[1] == OP_HASH160 &&
        spk[2] == 0x14 && spk[23] == OP_EQUALVERIFY && spk[24] == OP_CHECKSIG) {
        return Bytes(spk.begin() + 3, spk.begin() + 23);
    }
    throw std::runtime_error("not P2PKH");
}

