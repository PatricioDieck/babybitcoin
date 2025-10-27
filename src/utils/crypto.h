#pragma once 
#include "common.h"

// Simple keypair container.
struct KeyPair {
  Bytes priv; // 32 bytes
  Bytes pub;  // 33 bytes (compressed) or 65 bytes (uncompressed). We'll use 33B // compressed.
};

// --- Hashing ---
// SHA-256 and double SHA-256 for txids & block headers (§2, §§3–4)
Hash256 sha256(const Bytes& m);
Hash256 dbl_sha256(const Bytes& m);

// --- Hashing ---
// SHA-256 and double SHA-256 for txids & block headers (§2, §§3–4)
Bytes ripemd160(const Bytes& m);
Bytes hash160(const Bytes& m);

// --- Keys & ECDSA ---
// Generate secp256k1 keypair.
KeyPair generate_keypair();

// Sign/verify using ECDSA over secp256k1.
// Input is a 32-byte hash (e.g., from dbl_sha256 or a SIGHASH digest).
// Signature is DER-encoded (Bitcoin-style).
Bytes ecdsa_sign(const Hash256 &h, const Bytes &priv);
bool ecdsa_verify(const Hash256 &h, const Bytes &sig_der, const Bytes &pub);
