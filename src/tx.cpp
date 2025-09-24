
#include "tx.h"
#include "crypto.h"
#include "script_min.h"
#include <cstddef>
#include <stdexcept>

static void ser_outpoint(Bytes& out, const OutPoint& op){
  // txid is serialized LE on the wire; we already store LE bytes
  out.insert(out.end(), op.txid.begin(), op.txid.end());
  write_u32_le(out, op.index);
}
static void ser_txin(Bytes& out, const TxIn& in){
  ser_outpoint(out, in.prevout);
  write_varint(out, in.scriptSig.size());
  out.insert(out.end(), in.scriptSig.begin(), in.scriptSig.end());
  write_u32_le(out, in.sequence);
}
static void ser_txout(Bytes& out, const TxOut& o){
  write_i64_le(out, o.value);
  write_varint(out, o.scriptPubKey.size());
  out.insert(out.end(), o.scriptPubKey.begin(), o.scriptPubKey.end());
}

Bytes Transaction::serialize(bool) const {
  Bytes out;
  out.reserve(10 + vin.size()*41 + vout.size()*34);
  // version
  write_u32_le(out, (uint32_t)version);
  // vin
  write_varint(out, vin.size());
  for (auto& in : vin) ser_txin(out, in);
  // vout
  write_varint(out, vout.size());
  for (auto& o : vout) ser_txout(out, o);
  // locktime
  write_u32_le(out, locktime);
  return out;
}

Hash256 Transaction::txid() const {
  return dbl_sha256(serialize());
}

Bytes build_p2pkh_scriptSig(const Bytes& sig_der, const Bytes& pub) {
  Bytes s;
  // push sig+hashtype (DER || 0x01)
  Bytes sig_plus = sig_der; sig_plus.push_back(0x01); // SIGHASH_ALL
  s.push_back((uint8_t)sig_plus.size());
  s.insert(s.end(), sig_plus.begin(), sig_plus.end());
  // push pubkey
  s.push_back((uint8_t)pub.size());
  s.insert(s.end(), pub.begin(), pub.end());
  return s;
}

Hash256 sighash_all(const Transaction& tx, size_t i, const Bytes& prevout_scriptPubKey) {
  if (i >= tx.vin.size()) throw std::runtime_error("sighash index");
  // Serialize tx with all inputs' scriptSig empty, except i replaced with prevout_scriptPubKey
  Transaction t = tx; // copy
  for (size_t k=0;k<t.vin.size();++k) t.vin[k].scriptSig.clear();
  t.vin[i].scriptSig = prevout_scriptPubKey;

  Bytes ser = t.serialize();
  // append 4-byte SIGHASH type
  write_u32_le(ser, 1); // SIGHASH_ALL
  return dbl_sha256(ser);
}

bool verify_p2pkh_input(const Transaction& tx, size_t i, const Bytes& prevout_spk) {
  // parse scriptSig
  if (i >= tx.vin.size()) return false;
  const Bytes& ss = tx.vin[i].scriptSig;
  if (ss.size()<2) return false;
  size_t p=0;
  uint8_t siglen = ss[p++]; if (p+siglen>ss.size()) return false;
  Bytes sig_plus(ss.begin()+p, ss.begin()+p+siglen); p+=siglen;
  if (sig_plus.empty() || sig_plus.back()!=0x01) return false; // must be SIGHASH_ALL
  sig_plus.pop_back(); // DER only
  if (p>=ss.size()) return false;
  uint8_t publen = ss[p++]; if (p+publen>ss.size()) return false;
  Bytes pub(ss.begin()+p, ss.begin()+p+publen);

  // check pubkey hash matches output script
  Bytes h160 = p2pkh_hash_from_script(prevout_spk);
  if (hash160(pub) != h160) return false;

  // compute SIGHASH
  Hash256 h = sighash_all(tx, i, prevout_spk);
  return ecdsa_verify(h, sig_plus, pub);
}
