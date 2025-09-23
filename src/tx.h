#pragma once
#include "common.h"
#include "crypto.h"

struct OutPoint {
  Hash256 txid;
  uint32_t index;
};

// Input: what we’re spending + the proof (scriptSig) we provide.
struct TxIn {
  OutPoint prevout;
  Bytes scriptSig; // for P2PKH: <sig (DER) + 1-byte SIGHASH_ALL> <pubkey>
  uint32_t sequence{0xffffffff};
};

// Output: value + locking script (scriptPubKey).
struct TxOut {
  int64_t value{0};   // satoshis
  Bytes scriptPubKey; // P2PKH script (25 bytes)
};

// A transaction: version, inputs, outputs, locktime.
struct Transaction {
  int32_t version{1};
  std::vector<TxIn> vin;
  std::vector<TxOut> vout;
  uint32_t locktime{0};

  Bytes serialize(bool with_witness = false) const; // minimal: no witness
  Hash256 txid() const; // dbl_sha256(serialized bytes)
};

// Build scriptSig for P2PKH: <sig DER + 0x01 SIGHASH_ALL> <pubkey>
Bytes build_p2pkh_scriptSig(const Bytes& sig_der, const Bytes& pub);

// Compute the SIGHASH_ALL digest for input i (legacy): all scriptSigs empty,
// except input i replaced with its prevout's scriptPubKey, then append
// 0x01000000.
Hash256 sighash_all(const Transaction& tx, size_t input_index, const Bytes &prevout_scriptPubKey);

// Verify a P2PKH input against its referenced output's scriptPubKey.
bool verify_p2pkh_input(const Transaction& tx, size_t i, const Bytes& prevout_scriptPubKey);

