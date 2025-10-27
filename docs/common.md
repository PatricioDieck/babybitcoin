Perfect—let’s start writing the first building block and explain every line so you always know *why* it’s here and how it serves the whitepaper goals. We’ll keep **headers as `.h`** (like Satoshi), and **put `.h` and `.cpp` in the same `src/` folder**, as you requested.

We’ll begin with the tiniest, universal layer: **`common`**. It’s intentionally boring—by design. Everything else (transactions, blocks, PoW) will lean on these functions. After this, we’ll implement `crypto.{h,cpp}`, then `tx.{h,cpp}` (whitepaper §2), etc.

---

# Project map (so you have the full picture)

```
babycoin/
  third_party/
    httplib.h           // tiny HTTP server/client for P2P later
  src/
    common.h            // shared types + serialization helpers (today)
    common.cpp
    crypto.h            // sha256, double-sha, ripemd160, hash160, keys, sign, verify
    crypto.cpp
    script_min.h        // minimal P2PKH helpers (no full Script VM)
    script_min.cpp
    tx.h                // §2: OutPoint, TxIn, TxOut, Transaction (serialize/sign/verify)
    tx.cpp
    block.h             // §§3–4: BlockHeader/Block, merkle root, PoW
    block.cpp
    chain.h             // UTXO set, accept_block/connect_block (happy path)
    chain.cpp
    node.h              // tiny HTTP gossip (announce/fetch tx/block)
    node.cpp
    main.cpp            // wires it all together, demo run
```

We’ll implement one file at a time. Today: `common.h/.cpp`.

---

# `src/common.h` (header)

### Why this file exists

* The whitepaper’s core artifacts—**transactions** and **blocks**—have a precise, byte-level layout on the wire and on disk. To produce the **same hashes** (for txids and block headers), we must serialize exactly like Bitcoin (little-endian integers, CompactSize “varints”).
* We also need a **neutral “bytes” type** and **hex tools** for debugging as we build and test each section.

### What’s inside

* **`Bytes`** and **`Hash256`** types to avoid ambiguity.
* **Hex encode/decode** for visibility while we debug.
* **Little-endian writers** (Bitcoin uses LE for most integers).
* **CompactSize varint** helpers (Bitcoin’s count/length encoding).

```cpp
// src/common.h
#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>

// --- Why these aliases? ---
// We will be constructing binary payloads exactly like Bitcoin's reference serialization.
// 'Bytes' gives us a clear, explicit "bag of bytes" instead of mixing with std::string.
// 'Hash256' is a fixed-size 32-byte array used for txids and block header hashes.
using Bytes   = std::vector<uint8_t>;
using Hash256 = std::array<uint8_t, 32>;

// --- Debugging visibility ---
// Seeing what's inside buffers (in hex) is essential as we test each whitepaper step.
// We'll constantly print and compare against expected values.
std::string hex(const Bytes& b);
Bytes from_hex(const std::string& s);

// --- Canonical serialization primitives ---
// Bitcoin's wire/disk format uses little-endian integers and CompactSize ("varint").
// We'll use these for tx serialization (§2), block headers (§§3–4), and more.
//
// We keep these functions tiny and explicit so it's easy to audit the exact bytes.
void write_u32_le(Bytes& out, uint32_t v);
void write_u64_le(Bytes& out, uint64_t v);
void write_i64_le(Bytes& out, int64_t v);

// CompactSize variable-length integer encoding (counts and lengths).
void write_varint(Bytes& out, uint64_t v);

// Reader used later when we parse inbound data (e.g., from peers or disk).
uint64_t read_varint(const uint8_t*& p, const uint8_t* end);
```

**How this supports the whitepaper:**

* §2 Transactions: we’ll serialize `vin/vout` counts with **varint**, values as **LE integers**, and then hash the bytes to form the **txid**—exactness here is critical.
* §§3–4 Timestamp/PoW: block header fields are serialized **LE**, then double-SHA256 to get the header hash—we need these helpers to ensure our hash matches what other nodes would compute.

---

# `src/common.cpp` (implementation)

### Why each function matters

* `hex` / `from_hex`: sanity checks and tests. We’ll print tx bytes, block headers, merkle layers, etc.
* LE writers: guarantee cross-platform correctness (endianness differences can silently break hashes).
* `write_varint` / `read_varint`: the only correct way to encode/decode vector lengths and counts in Bitcoin-style serialization.

```cpp
// src/common.cpp
#include "utils/common.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

// Turn raw bytes into readable hex for debugging.
// We'll use this constantly to verify serialized txs/blocks against expectations.
std::string hex(const Bytes& b) {
  std::ostringstream oss;
  for (auto v : b) oss << std::hex << std::setw(2) << std::setfill('0') << (int)v;
  return oss.str();
}

// Helper to convert a single hex char to its numeric value.
static uint8_t hexval(char c) {
  if ('0'<=c && c<='9') return c-'0';
  if ('a'<=c && c<='f') return c-'a'+10;
  if ('A'<=c && c<='F') return c-'A'+10;
  throw std::runtime_error("bad hex");
}

// Parse a hex string into bytes. Useful for tests and fixtures.
// Example: from_hex("0a0b") -> {0x0a, 0x0b}
Bytes from_hex(const std::string& s) {
  if (s.size()%2) throw std::runtime_error("hex length");
  Bytes out; out.reserve(s.size()/2);
  for (size_t i=0;i<s.size();i+=2) out.push_back((hexval(s[i])<<4)|hexval(s[i+1]));
  return out;
}

// Little-endian writers (Bitcoin's canonical integer encoding on wire/disk).
// These must be exact; changing byte order changes hashes.
void write_u32_le(Bytes& out, uint32_t v) {
  out.push_back((v>>0)&0xff); out.push_back((v>>8)&0xff);
  out.push_back((v>>16)&0xff); out.push_back((v>>24)&0xff);
}

void write_u64_le(Bytes& out, uint64_t v) {
  for (int i=0;i<8;i++) out.push_back((v>>(8*i))&0xff);
}

void write_i64_le(Bytes& out, int64_t v) {
  write_u64_le(out, static_cast<uint64_t>(v));
}

// CompactSize (varint) used for counts/lengths in tx/block serialization.
// Spec: 1 byte if < 0xfd; 0xfd + 2 bytes if <= 0xffff; 0xfe + 4 bytes if <= 0xffffffff;
// else 0xff + 8 bytes.
void write_varint(Bytes& out, uint64_t v) {
  if (v < 0xfd) { out.push_back((uint8_t)v); return; }
  if (v <= 0xffff) {
    out.push_back(0xfd); out.push_back(v&0xff); out.push_back((v>>8)&0xff); return;
  }
  if (v <= 0xffffffffULL) {
    out.push_back(0xfe);
    for (int i=0;i<4;i++) out.push_back((v>>(8*i))&0xff);
    return;
  }
  out.push_back(0xff);
  for (int i=0;i<8;i++) out.push_back((v>>(8*i))&0xff);
}

// Reader counterpart for when we parse payloads (e.g., from peers or disk).
uint64_t read_varint(const uint8_t*& p, const uint8_t* end) {
  if (p>=end) throw std::runtime_error("varint eof");
  uint8_t b = *p++;
  if (b < 0xfd) return b;
  if (b == 0xfd) {
    if (end - p < 2) throw std::runtime_error("eof");
    uint64_t v = p[0] | (uint64_t)p[1]<<8; p+=2; return v;
  }
  if (b == 0xfe) {
    if (end - p < 4) throw std::runtime_error("eof");
    uint64_t v=0; for (int i=0;i<4;i++) v |= (uint64_t)p[i]<<(8*i); p+=4; return v;
  }
  if (end - p < 8) throw std::runtime_error("eof");
  uint64_t v=0; for (int i=0;i<8;i++) v |= (uint64_t)p[i]<<(8*i); p+=8; return v;
}
```

---

## How this ties back to the whitepaper (sanity check)

* **Introduction** demands we can *reliably* share and verify the same data across peers. That requires a **canonical byte format**.
* **§2 Transactions** and **§§3–4 Blocks/PoW** will **hash** serialized bytes; any mismatch in endianness or length encoding = different hash = network disagreement.
* This `common` module is the **foundation** that makes your txids, merkle roots, and block header hashes **stable and reproducible**—which is the essence of Nakamoto consensus.

---

## Next step (when you’re ready): `crypto.h/.cpp`

Why: §2 needs **SHA256** and **secp256k1 signatures** to implement the “chain of digital signatures.” We’ll keep the interface minimal: `sha256`, `dbl_sha256`, `ripemd160`, `hash160`, `generate_keypair`, `ecdsa_sign`, `ecdsa_verify`.

If this `common` layer looks good, say the word and we’ll write `crypto.h/.cpp` next (again with line-by-line “why”).
