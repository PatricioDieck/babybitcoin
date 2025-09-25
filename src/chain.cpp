#include "chain.h"
#include "script_min.h"
#include <cstring>
#include <stdexcept>

// --- helpers ---

ChainState::UTXOKey ChainState::make_key(const Hash256& txid, uint32_t index) {
  // Binary key: 32B txid (LE) + 4B index (LE)
  std::string k; k.resize(36);
  std::memcpy(k.data(), txid.data(), 32);
  k[32] = (char)(index & 0xff);
  k[33] = (char)((index>>8) & 0xff);
  k[34] = (char)((index>>16)& 0xff);
  k[35] = (char)((index>>24)& 0xff);
  return k;
}

bool ChainState::have_utxo(const Hash256& txid, uint32_t index) const {
  return utxo_.find(make_key(txid, index)) != utxo_.end();
}

const UTXO* ChainState::get_utxo(const Hash256& txid, uint32_t index) const {
  auto it = utxo_.find(make_key(txid, index));
  if (it == utxo_.end()) return nullptr;
  return &it->second;
}

void ChainState::add_utxo(const Hash256& txid, uint32_t index, int64_t value, const Bytes& spk) {
  utxo_[make_key(txid, index)] = UTXO{ value, spk };
}

bool ChainState::spend_utxo(const Hash256& txid, uint32_t index) {
  return utxo_.erase(make_key(txid, index)) == 1;
}

ChainState::ChainState() {
  // Start with "no tip". In a full node we'd have a genesis block here.
  tip_hash_ = Hash256{}; // all zeros
  height_   = 0;
}

// --- TX Validation ---

bool ChainState::is_coinbase(const Transaction& tx) const {
  if (tx.vin.size() != 1) return false;
  const auto& in0 = tx.vin[0];
  // Coinbase has prevout hash = 0..0 and index = 0xffffffff
  bool zero_hash = true;
  for (auto b : in0.prevout.txid) { if (b != 0) { zero_hash = false; break; } }
  return zero_hash && (in0.prevout.index == 0xffffffff);
}

bool ChainState::validate_tx(const Transaction& tx, int64_t& fee) const {
  // Coinbase is allowed to have no inputs and arbitrary scriptSig; fees computed at block-level.
  if (is_coinbase(tx)) {
    // We'll check subsidy budget at block level; fee here is 0 for coinbase.
    fee = 0;
    return true;
  }

  // Regular TX: check all inputs exist and are authorized; values balance.
  int64_t sum_in = 0;
  for (size_t i = 0; i < tx.vin.size(); ++i) {
    const auto& in = tx.vin[i];
    const UTXO* u = get_utxo(in.prevout.txid, in.prevout.index);
    if (!u) return false; // double-spend or unknown input

    // P2PKH only (happy path)
    if (!verify_p2pkh_input(tx, i, u->scriptPubKey)) return false;

    sum_in += u->value;
    if (sum_in < 0) return false; // overflow guard
  }

  int64_t sum_out = 0;
  for (const auto& o : tx.vout) {
    if (o.value < 0) return false;
    sum_out += o.value;
    if (sum_out < 0) return false; // overflow guard
  }

  if (sum_in < sum_out) return false; // cannot create money
  fee = sum_in - sum_out;
  return true;
}

// --- Block Validation & Connection ---

bool ChainState::validate_block(const Block& b, int64_t& total_fees) const {
  // 1) Basic structure: at least 1 tx (coinbase)
  if (b.vtx.empty()) return false;
  if (!is_coinbase(b.vtx[0])) return false;

  // 2) PoW correct for the header we see
  if (!header_meets_pow(b.header)) return false;

  // 3) Merkle root matches txids
  if (merkle_root(b.vtx) != b.header.merkle_root) return false;

  // 4) Validate txs; accumulate fees from non-coinbase txs
  total_fees = 0;
  for (size_t i = 0; i < b.vtx.size(); ++i) {
    int64_t fee = 0;
    if (!validate_tx(b.vtx[i], fee)) return false;
    if (i != 0) total_fees += fee; // coinbase fee handled as subsidy budget
  }

  // 5) Coinbase value <= SUBSIDY + fees (no money printing)
  // Happy path: fixed SUBSIDY (no halvings yet).
  int64_t coinbase_out = 0;
  for (const auto& o : b.vtx[0].vout) {
    if (o.value < 0) return false;
    coinbase_out += o.value;
    if (coinbase_out < 0) return false;
  }
  if (coinbase_out > SUBSIDY + total_fees) return false;

  return true;
}

bool ChainState::connect_block(const Block& b, int64_t total_fees) {
  // Spend inputs of all non-coinbase txs; then add all outputs of every tx.
  // If any spend/add fails mid-way, we'd need to roll back; happy path assumes valid input.

  // Spend inputs (skip coinbase)
  for (size_t i = 1; i < b.vtx.size(); ++i) {
    const auto& tx = b.vtx[i];
    for (const auto& in : tx.vin) {
      if (!spend_utxo(in.prevout.txid, in.prevout.index)) return false;
    }
  }

  // Add outputs (including coinbase)
  for (size_t i = 0; i < b.vtx.size(); ++i) {
    const auto& tx = b.vtx[i];
    Hash256 id = tx.txid();
    for (uint32_t n = 0; n < tx.vout.size(); ++n) {
      const auto& o = tx.vout[n];
      add_utxo(id, n, o.value, o.scriptPubKey);
    }
  }

  // Advance tip (happy path: append only)
  tip_hash_ = b.header.hash();
  height_  += 1;
  (void)total_fees; // not used further yet
  return true;
}

bool ChainState::accept_block(const Block& b) {
  int64_t fees = 0;
  if (!validate_block(b, fees)) return false;
  return connect_block(b, fees);
}

// Balance helper: sum UTXOs that match HASH160(pub).
int64_t ChainState::balance_for_pubkey(const Bytes& pub) const {
  Bytes target = hash160(pub);
  int64_t sum = 0;
  for (const auto& [k, u] : utxo_) {
    // Try to parse a P2PKH and match h160
    try {
      Bytes h = p2pkh_hash_from_script(u.scriptPubKey);
      if (h == target) {
        sum += u.value;
        if (sum < 0) return 0; // overflow guard; shouldn't happen
      }
    } catch (...) {
      // Not a P2PKH; ignore (we only create P2PKH here anyway)
    }
  }
  return sum;
}
