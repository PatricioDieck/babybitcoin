#include "block.h"
#include "utils/crypto.h"
#include <chrono>

// Internal helper: append a 32-byte hash in the form we store (LE bytes).
static void write_hash_le(Bytes& out, const Hash256& h) {
  out.insert(out.end(), h.begin(), h.end());
}

Bytes BlockHeader::serialize() const {
  Bytes out;
  out.reserve(80); // header is 80 bytes in Bitcoin
  write_u32_le(out, (uint32_t)version);
  write_hash_le(out, prev_block);
  write_hash_le(out, merkle_root);
  write_u32_le(out, timestamp);
  write_u32_le(out, bits);
  write_u32_le(out, nonce);
  return out;
}

Hash256 BlockHeader::hash() const {
  return dbl_sha256(serialize());
}

Bytes Block::serialize() const {
  Bytes out = header.serialize();
  write_varint(out, vtx.size());
  for (const auto& tx : vtx) {
    Bytes s = tx.serialize();
    out.insert(out.end(), s.begin(), s.end());
  }
  return out;
}

// Count leading zero bits by viewing the hash as big-endian bits for simplicity.
// Our Hash256 is stored LE by convention; reverse to BE view for bit counting.
static bool has_leading_zero_bits(const Hash256& h, uint32_t bits){
  // Convert to big-endian view of the 32-byte array
  Bytes be(h.rbegin(), h.rend()); // reverse the 32 bytes
  uint32_t full_zero_bytes = bits / 8;
  uint32_t extra_bits      = bits % 8;

  // Check full zero bytes
  for (uint32_t i=0;i<full_zero_bytes;i++){
    if (be[i] != 0) return false;
  }
  if (extra_bits == 0) return true;

  // Next byte must have its top 'extra_bits' zeroed.
  uint8_t mask = 0xFF << (8 - extra_bits); // e.g., extra_bits=3 => mask 0b1110_0000
  return (be[full_zero_bytes] & mask) == 0;
}

bool header_meets_pow(const BlockHeader& H) {
    return has_leading_zero_bits(H.hash(), H.bits);
  }


Hash256 merkle_root(const std::vector<Transaction>& vtx) {
  if (vtx.empty()) return Hash256{}; // all-zero if no txs (we'll avoid empty blocks in practice)

  // Start with txids (each already LE 32-byte hash)
  std::vector<Hash256> layer;
  layer.reserve(vtx.size());
  for (const auto& tx : vtx) layer.push_back(tx.txid());

  // While more than one hash remains, pairwise hash concatenations (dbl_sha256)
  while (layer.size() > 1) {
    std::vector<Hash256> next;
    next.reserve((layer.size()+1)/2);

    for (size_t i=0; i<layer.size(); i+=2) {
      const Hash256& a = layer[i];
      const Hash256& b = (i+1 < layer.size()) ? layer[i+1] : layer[i]; // duplicate last if odd

      Bytes cat;
      cat.insert(cat.end(), a.begin(), a.end());
      cat.insert(cat.end(), b.begin(), b.end());
      next.push_back(dbl_sha256(cat));
    }
    layer.swap(next);
  }
  return layer[0];
}

bool mine_block(Block& b){
  // Fill timestamp if caller didn't set it
  if (b.header.timestamp == 0) {
    b.header.timestamp = static_cast<uint32_t>(std::chrono::seconds(std::time(nullptr)).count());
  }
  // Recompute merkle root from current transactions
  b.header.merkle_root = merkle_root(b.vtx);

  // Simple nonce scan across 32-bit space
  for (uint32_t n = 0; n < 0xFFFFFFFFu; ++n) {
    b.header.nonce = n;
    Hash256 h = b.header.hash();
    if (has_leading_zero_bits(h, b.header.bits)) {
      return true; // found a valid PoW
    }
  }
  return false;
}
