- Keep canonical chain in memory (append-only vector in ChainState).
- Record each peer’s reported height during VERSION.
- After VERACK, if our height > their height, send them missing blocks [peer_height .. our_height-1].

Edits to add:

1) Chain: store blocks and expose accessor
- In src/chain.h (class ChainState, private):
```cpp
std::vector<Block> chain_;  // canonical chain, height_ == chain_.size()
```
- In src/chain.h (public):
```cpp
const Block* block_by_index(uint32_t i) const {
  return (i < chain_.size()) ? &chain_[i] : nullptr;
}
```
- In src/chain.cpp, at end of connect_block(...) after advancing state:
```cpp
chain_.push_back(b);
```

2) P2P: track peer height and catch up after handshake
- In src/p2p_minimal.h (inside Peer):
```cpp
uint32_t    peer_height{0};
bool        has_height{false};
```
- In src/p2p_minimal.h (private methods):
```cpp
void maybe_catch_up_(Peer& p);
```
- In src/p2p_minimal.cpp, implement:
```cpp
void P2PNode::maybe_catch_up_(Peer& p) {
  if (!p.handshaked || !p.has_height) return;
  uint32_t our = chain_.height();
  if (our <= p.peer_height) return;
  for (uint32_t h = p.peer_height; h < our; ++h) {
    const Block* b = chain_.block_by_index(h);
    if (!b) break;
    Bytes pl = b->serialize();
    send_frame_(p.fd, MsgType::BLOCK, pl);
  }
}
```
- In dispatch_ VERSION case (after reading peer_height and before/after send_verack_):
```cpp
p.peer_height = peer_height;
p.has_height = true;
send_verack_(p);
maybe_catch_up_(p);  // harmless if not yet handshaked
```
- In dispatch_ VERACK case (after setting handshaked):
```cpp
p.handshaked = true;
maybe_catch_up_(p);
```

Notes
- Indexing: peer_height matches number of connected blocks the peer claims; first block is index 0, so send blocks in [peer_height, height-1].
- No reorg support; this is fine for your happy-path single chain.
- This uses only your existing BLOCK message; no new messages needed.