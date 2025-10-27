#include "serialize.h"

// -------------- Parsers (inverse of your serialize) ------------

OutPoint parse_outpoint(const uint8_t*& p, const uint8_t* end) {
    if (end - p < 36) throw std::runtime_error("outpoint eof");
    OutPoint op{};
    std::memcpy(op.txid.data(), p, 32); p += 32;
    op.index = read_u32_le_(p, end);
    return op;
}

TxIn parse_txin(const uint8_t*& p, const uint8_t* end) {
    TxIn in{};
    in.prevout = parse_outpoint(p, end);
    uint64_t slen = read_varint(p, end);
    if ((uint64_t)(end - p) < slen) throw std::runtime_error("scriptsig eof");
    in.scriptSig.assign(p, p + slen); p += slen;
    in.sequence = read_u32_le_(p, end);
    return in;
}

TxOut parse_txout(const uint8_t*& p, const uint8_t* end) {
    TxOut o{};
    o.value = read_i64_le_(p, end);
    uint64_t plen = read_varint(p, end);
    if ((uint64_t)(end - p) < plen) throw std::runtime_error("scriptpubkey eof");
    o.scriptPubKey.assign(p, p + plen); p += plen;
    return o;
}

Transaction parse_tx(const uint8_t*& p, const uint8_t* end) {
    Transaction tx{};
    tx.version = (int32_t)read_u32_le_(p, end);
    uint64_t nin = read_varint(p, end);  tx.vin.reserve((size_t)nin);
    for (uint64_t i=0;i<nin;i++) tx.vin.push_back(parse_txin(p, end));
    uint64_t nout = read_varint(p, end); tx.vout.reserve((size_t)nout);
    for (uint64_t i=0;i<nout;i++) tx.vout.push_back(parse_txout(p, end));
    tx.locktime = read_u32_le_(p, end);
    return tx;
}

BlockHeader parse_header(const uint8_t*& p, const uint8_t* end) {
    BlockHeader h{};
    h.version = (int32_t)read_u32_le_(p, end);
    if (end - p < 32) throw std::runtime_error("prev_block eof");
    std::memcpy(h.prev_block.data(), p, 32); p += 32;
    if (end - p < 32) throw std::runtime_error("merkle_root eof");
    std::memcpy(h.merkle_root.data(), p, 32); p += 32;
    h.timestamp = read_u32_le_(p, end);
    h.bits      = read_u32_le_(p, end);
    h.nonce     = read_u32_le_(p, end);
    return h;
}

Block parse_block(const uint8_t*& p, const uint8_t* end) {
    Block b{};
    b.header = parse_header(p, end);
    uint64_t ntx = read_varint(p, end);
    b.vtx.reserve((size_t)ntx);
    for (uint64_t i=0;i<ntx;i++) b.vtx.push_back(parse_tx(p, end));
    return b;
}

// Block parse_block(const Bytes& raw) {
//     const uint8_t* p = raw.data();
//     const uint8_t* end = raw.data() + raw.size();
//     Block b{}; b.header = parse_header(p, end);
//     uint64_t ntx = read_varint(p, end);
//     b.vtx.reserve((size_t)ntx);
//     for (uint64_t i=0;i<ntx;i++) b.vtx.push_back(parse_tx(p, end));
//     if (p != end) throw std::runtime_error("trailing bytes in block");
//     return b;
//   }
  