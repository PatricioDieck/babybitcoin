# How P2PNode References the Chain

- The node holds a **reference** to the existing `ChainState` and** calls a few methods** on it (e.g., `accept_block(b)`, `height()`), but it doesn’t own or mirror chain data. Ownership of the ledger (UTXO, tip, height) stays inside `ChainState`. We aren’t “pumping the entire chain” into the P2P node. 

- Why a reference instead of statics/globals:
  - **Avoids globals/singletons and static methods.**
  - Lets you inject any `ChainState` (or a mock) for tests.
  - Keeps lifetime and ownership explicit (the caller owns `ChainState`; P2P just uses it).

- How loose coupling is maintained:
  - Directionality is one-way: `P2PNode` depends on `ChainState`’s public interface; `ChainState` does not depend on P2P (no include of `p2p_minimal.h`, no callbacks).
  - Minimal surface area: P2P only uses a small set of `ChainState` methods (validate/connect + a few getters). No internals are exposed.
  - Clear responsibilities: P2P handles sockets, framing, gossip, dedup, and relay; `ChainState` handles validation, UTXO, and advancing the tip.
  - No shared mutable structures: P2P has its own `mempool_` and `seen_*` sets; `ChainState` owns chain data.

- What the constructor does: `P2PNode::P2PNode(ChainState& chain) : chain_(chain) {}` injects a reference so the node can validate (and then relay) using the same, authoritative chain instance—without copying, without static/global state.

- If you wanted even looser coupling later: introduce an interface (e.g., `IChain` with `accept_block`, `height`, etc.) and have `P2PNode` depend on that; `ChainState` would implement it. That lets you swap implementations without changing P2P.

Net: We share a single chain by reference, keep dependency direction one-way, expose only a small interface, and avoid static/global coupling.