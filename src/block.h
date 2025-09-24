#pragma once
#include "common.h"
#include "tx.h"

// A block header commits to: version, previous block, Merkle root of all txs,
// a timestamp, difficulty (we'll interpret 'bits' as a simple "leading zero
// bits"), and a nonce for PoW search.
//
// Whitepaper link: §3 (Timestamp Server) & §4 (Proof-of-Work)
//
// In full Bitcoin, 'bits' encodes a compact target; to keep happy-path minimal
// and readable, we use "number of required leading zero bits" (integer).
struct BlockHeader {
  int32_t version{1};
  Hash256 prev_block{};  // hash of previous block header (LE bytes)
  Hash256 merkle_root{}; // merkle root of txids (LE bytes)
  uint32_t timestamp{0}; // Unix epoch seconds
  uint32_t bits{0};      // simplified difficulty: "leading-zero bits" required
  uint32_t nonce{0};     // PoW search variable

  // Serialize header exactly in Bitcoin's order and endianness.
  Bytes serialize() const;

  // Double-SHA256 of the serialized header; this is the block header hash.
  Hash256 hash() const;
};

// A block is a header plus a list of transactions.
// The first transaction should be a coinbase (incentive), but we'll enforce
// that in the validation/chain module; here we just carry the data.
struct Block {
  BlockHeader header;
  std::vector<Transaction> vtx;

  // Full serialization: header + varint(tx count) + each tx serialization.
  Bytes serialize() const;
};

// Compute the Bitcoin-style merkle root over txids (duplicating the last hash
// when the layer has an odd count). Returns LE 32-byte hash.
Hash256 merkle_root(const std::vector<Transaction> &vtx);

// Scan nonces to find a header hash with at least 'bits' leading zero bits.
// Returns true if found a solution (header.nonce set), false otherwise.
bool mine_block(Block &b);
