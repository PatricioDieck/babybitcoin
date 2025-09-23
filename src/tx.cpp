
#include "tx.h"
#include "crypto.h"
#include "script_min.h"
#include <cstddef>
#include <stdexcept>

// --- Serialization helpers for deterministic hashing ---

static void ser_outpoint(Bytes &out, const OutPoint &op) {
  // We store txid as LE bytes to match the wire form we’ll emit.
  out.insert(out.end(), op.txid.begin(), op.txid.end());
  write_u32_le(out, op.index);
}

static void ser_txin(Bytes &out, const TxIn &in) {
  ser_outpoint(out, in.prevout);
  write_varint(out, in.scriptSig.size());
  out.insert(out.end(), in.scriptSig.begin(), in.scriptSig.end());
  write_u32_le(out, in.sequence);
}

static void ser_txout(Bytes &out, const TxOut &o) {
  write_i64_le(out, o.value);
  write_varint(out, o.scriptPubKey.size());
  out.insert(out.end(), o.scriptPubKey.begin(), o.scriptPubKey.end());
}

Bytes Transaction::serialize(bool) const {
  Bytes out;

  out.reserve(10 + vin.size() * 41 + vout.size() * 34);
  write_u32_le(out, (uint32_t)version);

  write_varint(out, vin.size());
  for (const auto &in : vin)
    ser_txin(out, in);

  write_varint(out, vout.size());
  for (const auto &o : vout)
    ser_txout(out, o);

  write_u32_le(out, locktime);

  return out;
}

Hash256 Transaction::txid() const { return dbl_sha256(serialize()); }

// --- Signing/Verification (P2PKH, SIGHASH_ALL) ---

Bytes build_p2pkh_scriptSig(const Bytes &sig_der, const Bytes &pub) {

  Bytes s;
  Bytes sig_plus = sig_der;
  sig_plus.push_back(0x01);
  s.push_back((uint8_t)sig_plus.size());
  s.insert(s.end(), pub.begin(), pub.end());

  s.push_back((uint8_t)pub.size());
  s.insert(s.end(), pub.begin(), pub.end());

  return s;
}

Hash256 sighash_all(const Transaction &tx, size_t i, const Bytes &prevout_scriptPubKey) {
  if (i >= tx.vin.size())
    throw std::runtime_error("sighash index out of range!");

  // Copy tx, blank all scriptSigs, then set the input i's scriptSig =
  // prevout_scriptPubKey
  Transaction t = tx;

  for (auto &in : t.vin) in.scriptSig.clear();
  t.vin[i].scriptSig = prevout_scriptPubKey;

  Bytes ser = t.serialize();
  // Append 4-byte hash type (SIGHASH_ALL = 1)
  write_u32_le(ser, 1);
  return dbl_sha256(ser);
}

bool verify_p2pkh_input(const Transaction &tx, size_t i, const Bytes &prevout_spk) {
  if (i >= tx.vin.size()) return false;

  // Parse scriptSig: <sig+type> <pub>
  const Bytes &ss = tx.vin[i].scriptSig;
  if (ss.size() < 2) return false;
  size_t p = 0;

  // read sig push
  uint8_t siglen = ss[p++];
  if (p + siglen > ss.size()) return false;
  Bytes sig_plus(ss.begin() + p, ss.begin() + p + siglen);
  p += siglen;

  // Must end with 0x01 (SIGHASH_ALL)
  if (sig_plus.empty() || sig_plus.back() != 0x01) return false;
  sig_plus.pop_back(); // leave pure DER

  // read pub push
  if (p >= ss.size()) return false;
  uint8_t publen = ss[p++];
  if (p + publen > ss.size()) return false;
  Bytes pub(ss.begin()+p, ss.begin()+p+publen);

  // Check pubkey matches the pubKeyHash in the scriptPubKey
  Bytes h160 = p2pkh_hash_from_script(prevout_spk);
  if (hash160(pub) != h160) return false;

  // Compute sighash and verify signature 
  Hash256 h = sighash_all(tx, i, prevout_spk);
  return ecdsa_verify(h, sig_plus, pub);
}