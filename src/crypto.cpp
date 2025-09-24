#include "crypto.h"

#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>
#include <stdexcept>

Hash256 sha256(const Bytes &m) {
  Hash256 out{};
  ::SHA256(m.data(), m.size(), out.data());
  return out;
}

Hash256 dbl_sha256(const Bytes& m){
  auto h1 = sha256(m);
  Bytes tmp(h1.begin(), h1.end());
  return sha256(tmp);
}

Bytes ripemd160(const Bytes& m){
  Bytes out(20);
  ::RIPEMD160(m.data(), m.size(), out.data());
  return out;
}

Bytes hash160(const Bytes& m){
  auto h = sha256(m);
  Bytes tmp(h.begin(), h.end());
  return ripemd160(tmp);
}

// --- secp256k1 helper ---

static Bytes compress_pubkey(EC_KEY* key){
  // Force compressed form
  EC_KEY_set_conv_form(key, POINT_CONVERSION_COMPRESSED);
  int len = i2o_ECPublicKey(key, nullptr);
  if (len<=0) throw std::runtime_error("pub len");
  Bytes out(len);
  unsigned char* p = out.data();
  if (i2o_ECPublicKey(key, &p) != len) throw std::runtime_error("pub ser");
  return out;
}

// --- Keys & ECDSA ---

KeyPair generate_keypair(){
  KeyPair kp;
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_secp256k1);
  if(!key) throw std::runtime_error("ec new");
  // ensure compressed form on this key too
  EC_KEY_set_conv_form(key, POINT_CONVERSION_COMPRESSED);

  if(EC_KEY_generate_key(key)!=1){ EC_KEY_free(key); throw std::runtime_error("ec gen"); }

  const BIGNUM* priv_bn = EC_KEY_get0_private_key(key);
  Bytes priv(32,0);
  BN_bn2binpad(priv_bn, priv.data(), 32);
  kp.priv = std::move(priv);

  // serialize compressed public key
  kp.pub  = compress_pubkey(key);
  EC_KEY_free(key);
  return kp;
}

Bytes ecdsa_sign(const Hash256 &h, const Bytes &priv) {
  // Build EC_KEY from private scalar
  EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
  if (!key)
    throw std::runtime_error("ec new");
  BIGNUM *d = BN_bin2bn(priv.data(), priv.size(), nullptr);
  if (!d) {
    EC_KEY_free(key);
    throw std::runtime_error("bn");
  }

  EC_KEY_set_private_key(key, d);

  // Derive public Q = d*G (needed for OpenSSL to sign in some configs)
  const EC_GROUP *grp = EC_GROUP_new_by_curve_name(NID_secp256k1);
  EC_POINT *Q = EC_POINT_new(grp);
  EC_POINT_mul(grp, Q, d, nullptr, nullptr, nullptr);
  EC_KEY_set_group(key, grp);
  EC_KEY_set_public_key(key, Q);

  ECDSA_SIG *sig = ECDSA_do_sign(h.data(), h.size(), key);
  if (!sig) {
    EC_POINT_free(Q);
    EC_GROUP_free((EC_GROUP *)grp);
    BN_free(d);
    EC_KEY_free(key);
    throw std::runtime_error("sign");
  }

  // DER-encode signature (Bitcoin expects DER for legacy ECDSA)
  int derlen = i2d_ECDSA_SIG(sig, nullptr);
  Bytes der(derlen);
  unsigned char *p = der.data();
  i2d_ECDSA_SIG(sig, &p);

  ECDSA_SIG_free(sig);
  EC_POINT_free(Q);
  EC_GROUP_free((EC_GROUP *)grp);
  BN_free(d);
  EC_KEY_free(key);

  return der;
}

bool ecdsa_verify(const Hash256 &h, const Bytes &sig_der, const Bytes &pub) {
  EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
  if (!key)
    return false;

  const unsigned char *pp = pub.data();
  if (o2i_ECPublicKey(&key, &pp, pub.size()) == nullptr) {
    EC_KEY_free(key);
    return false;
  }

  const unsigned char *sp = sig_der.data();
  ECDSA_SIG *sig = d2i_ECDSA_SIG(nullptr, &sp, sig_der.size());
  if (!sig) {
    EC_KEY_free(key);
    return false;
  }

  int ok = ECDSA_do_verify(h.data(), h.size(), sig, key);
  ECDSA_SIG_free(sig);
  EC_KEY_free(key);
  return ok == 1;
}
