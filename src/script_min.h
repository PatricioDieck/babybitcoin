#pragma once
#include "utils/common.h"
#include "utils/crypto.h"

// Build standard P2PKH scriptPubKey: OP_DUP OP_HASH160 <20B> OP_EQUALVERIFY
// OP_CHECKSIG
Bytes p2pkh_script(const Bytes& pubkey);
 
// Extract the 20-byte pubKeyHash from a P2PKH scriptPubKey. Throws if not
// P2PKH.
Bytes p2pkh_hash_from_script(const Bytes& scriptPubKey);

Bytes build_p2pkh_script_from_h160(const Bytes& h160);