#include "common.h"

#include <sstream>
#include <iomanip>
#include <stdexcept>

// Turn raw bytes into readable hex for debugging.
// We'll use this constantly to verify serialized txs/blocks against
// expectations.
std::string hex(const Bytes &b) {
  std::ostringstream oss;
  for (auto v : b)
    oss << std::hex << std::setw(2) << std::setfill('0') << (int)v;
  return oss.str();
}

// Helper to convert a single hex char to its numeric value.
static uint8_t hexval(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  throw std::runtime_error("bad hex");
}

// Parse a hex string into bytes. Useful for tests and fixtures.
// Example: from_hex("0a0b") -> {0x0a, 0x0b}
Bytes from_hex(const std::string &s) {
  if (s.size() % 2)
    throw std::runtime_error("hex length");
  Bytes out;
  out.reserve(s.size() / 2);
  for (size_t i = 0; i < s.size(); i += 2)
    out.push_back((hexval(s[i]) << 4) | hexval(s[i + 1]));
  return out;
}

// Little-endian writers (Bitcoin's canonical integer encoding on wire/disk).
// These must be exact; changing byte order changes hashes.
void write_u32_le(Bytes &out, uint32_t v) {
  out.push_back((v >> 0) & 0xff);
  out.push_back((v >> 8) & 0xff);
  out.push_back((v >> 16) & 0xff);
  out.push_back((v >> 24) & 0xff);
}

void write_u64_le(Bytes &out, uint64_t v) {
  for (int i = 0; i < 8; i++)
    out.push_back((v >> (8 * i)) & 0xff);
}

void write_i64_le(Bytes &out, int64_t v) {
  write_u64_le(out, static_cast<uint64_t>(v));
}

// CompactSize (varint) used for counts/lengths in tx/block serialization.
// Spec: 1 byte if < 0xfd; 0xfd + 2 bytes if <= 0xffff; 0xfe + 4 bytes if <=
// 0xffffffff; else 0xff + 8 bytes.
void write_varint(Bytes &out, uint64_t v) {
  if (v < 0xfd) {
    out.push_back((uint8_t)v);
    return;
  }
  if (v <= 0xffff) {
    out.push_back(0xfd);
    out.push_back(v & 0xff);
    out.push_back((v >> 8) & 0xff);
    return;
  }
  if (v <= 0xffffffffULL) {
    out.push_back(0xfe);
    for (int i = 0; i < 4; i++)
      out.push_back((v >> (8 * i)) & 0xff);
    return;
  }
  out.push_back(0xff);
  for (int i = 0; i < 8; i++)
    out.push_back((v >> (8 * i)) & 0xff);
}

// Reader counterpart for when we parse payloads (e.g., from peers or disk).
uint64_t read_varint(const uint8_t *&p, const uint8_t *end) {
  if (p >= end)
    throw std::runtime_error("varint eof");
  uint8_t b = *p++;
  if (b < 0xfd)
    return b;
  if (b == 0xfd) {
    if (end - p < 2)
      throw std::runtime_error("eof");
    uint64_t v = p[0] | (uint64_t)p[1] << 8;
    p += 2;
    return v;
  }
  if (b == 0xfe) {
    if (end - p < 4)
      throw std::runtime_error("eof");
    uint64_t v = 0;
    for (int i = 0; i < 4; i++)
      v |= (uint64_t)p[i] << (8 * i);
    p += 4;
    return v;
  }
  if (end - p < 8)
    throw std::runtime_error("eof");
  uint64_t v = 0;
  for (int i = 0; i < 8; i++)
    v |= (uint64_t)p[i] << (8 * i);
  p += 8;
  return v;
}

std::string be_hex_key_(const Hash256& le) {
  Bytes be(le.rbegin(), le.rend());
  return hex(be);
}
uint32_t read_u32_le_(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 4) throw std::runtime_error("u32 eof");
  uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
  p += 4; return v;
}
int64_t read_i64_le_(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 8) throw std::runtime_error("i64 eof");
  uint64_t v=0; for (int i=0;i<8;i++) v |= (uint64_t)p[i] << (8*i);
  p += 8; return (int64_t)v;
}

// Pretty-print a 32-byte hash in the common BE (human) form.
// Internally we store Hash256 in little-endian; reversing makes it readable.
std::string hex_hash(const Hash256& h) {
  Bytes be(h.rbegin(), h.rend());
  return hex(be);
}