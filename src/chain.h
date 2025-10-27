#pragma once
#include <unordered_map>
#include <string>
#include "utils/common.h"
#include "tx.h"
#include "block.h"

// UTXO entry holds value and locking script (we only support P2PKH outputs).
struct UTXO {
  int64_t value;
  Bytes   scriptPubKey;
};

// basically a spendableUTXO - OutPoint + value + scriptPubKey to build new TxIns 
struct Coin {
  OutPoint prevout;      // tx.h
  int64_t  value;
  Bytes    scriptPubKey; // for signing/verification
};

// A tiny happy-path chainstate: one active tip, an in-memory UTXO map.
// No forks, no disk persistence (yet). Perfect for understanding flow.
class ChainState {
public:
  ChainState();

  bool load(const std::string& datadir);
  bool save(const std::string& datadir) const;

  // Return current height (genesis would be 0; our first block is height 0 if we treat it as genesis-ish).
  uint32_t height() const { return height_; }
  Hash256  tip_hash() const { return tip_hash_; }

  // Validate (w/o mutating), connect a block (mutate UTXO set & tip) on success.
  // Returns true on success; false if invalid.
  bool accept_block(const Block& b);

  // Block accessor: return the block at the given index.
  const Block* block_by_index(uint32_t i) const {
    return (i < chain_.size()) ? &chain_[i] : nullptr;
  }

  // Simple balance helper: sum of all UTXOs whose script matches HASH160(pub).
  int64_t balance_for_pubkey(const Bytes& pub) const;

  // Gather spendable coins for a given public key (P2PKH) until reaching 'target'.
  // Returns a vector of tuples: (txid, vout_index, value, scriptPubKey).
  // Simple greedy scan; callers may select a prefix up to the target.
  std::vector<Coin> gather_coins(const Bytes& pub, int64_t target) const;

  // Config: block subsidy (we keep it constant for happy path).
  static constexpr int64_t COIN   = 100'000'000;
  static constexpr int64_t SUBSIDY = 50 * COIN;

private:
  // --- UTXO Set ---
  // Key = 32-byte txid (LE) + 4-byte vout index (LE). Stored in a binary std::string for simplicity.
  using UTXOKey = std::string;
  static UTXOKey make_key(const Hash256& txid, uint32_t index);

  bool have_utxo(const Hash256& txid, uint32_t index) const;
  const UTXO* get_utxo(const Hash256& txid, uint32_t index) const;
  void add_utxo(const Hash256& txid, uint32_t index, int64_t value, const Bytes& spk);
  bool spend_utxo(const Hash256& txid, uint32_t index);

  // --- TX Validation (pure checks, no mutation) ---
  bool is_coinbase(const Transaction& tx) const;
  bool validate_tx(const Transaction& tx, int64_t& fee) const; // checks sigs/values; returns fee

  // --- Block Validation (pure checks), then Connect (mutate) ---
  bool validate_block(const Block& b, int64_t& total_fees) const;
  bool connect_block(const Block& b, int64_t total_fees); // mutate UTXO & tip

  // State
  std::unordered_map<UTXOKey, UTXO> utxo_;
  Hash256  tip_hash_{};
  uint32_t height_{0};
  std::vector<Block> chain_; // height_ == chain_.size()
};
