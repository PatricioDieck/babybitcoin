#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// --- Why these aliases? ---
// We will be constructing binary payloads exactly like Bitcoin's reference
// serialization. 'Bytes' gives us a clear, explicit "bag of bytes" instead of
// mixing with std::string. 'Hash256' is a fixed-size 32-byte array used for
// txids and block header hashes.
using Bytes = std::vector<uint8_t>;
using Hash256 = std::array<uint8_t, 32>;

// --- Debugging visibility ---
// Seeing what's inside buffers (in hex) is essential as we test each whitepaper
// step. We'll constantly print and compare against expected values.
std::string hex(const Bytes &b);
Bytes from_hex(const std::string &s);

// --- Canonical serialization primitives ---
// Bitcoin's wire/disk format uses little-endian integers and CompactSize
// ("varint"). We'll use these for tx serialization (§2), block headers (§§3–4),
// and more.
//
// We keep these functions tiny and explicit so it's easy to audit the exact
// bytes.
void write_u32_le(Bytes &out, uint32_t v);
void write_u64_le(Bytes &out, uint64_t v);
void write_i64_le(Bytes &out, int64_t v);

// CompactSize variable-length integer encoding (counts and lengths).
void write_varint(Bytes &out, uint64_t v);

// Reader used later when we parse inbound data (e.g., from peers or disk).
uint64_t read_varint(const uint8_t *&p, const uint8_t *end);