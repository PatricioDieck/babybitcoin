Short answer: keep the wallet signing locally, make the chain/node do state, and add just enough P2P + mining + persistence to transact across machines. That’s faithful to Satoshi (full node validates/relays/ mines; wallet holds keys and signs) and gets you a working demo on Chris’s computer.

### Minimal milestones to ship a working remote demo

- Wallet + TX (local)
  - Keep `WalletStore::send(fromName, toAddressHex, amount, ChainState&, fee)` for building/signing P2PKH txs.
  - Add an overload that accepts `to_pub` and hashes internally if you want: `send_to_pub(...)`.

- Addresses (cleaner UX)
  - Add Base58Check P2PKH address encode/decode:
    - `address = base58check(0x00 || hash160(pub))`
    - Where to put: `wallet.h/.cpp` (encode) and `script_min.{h,cpp}` (decode to h160).
  - CLI prints Base58 address; send command accepts Base58 or hex h160.

- Node state and mempool (on Chris’s machine)
  - Add a tiny mempool to `chain.{h,cpp}` or a `node.{h,cpp}` wrapper:
    - `bool accept_tx(const Transaction& tx)` → validate with current UTXO (reuse `validate_tx`), store if valid.
    - `std::vector<Transaction> drain_mempool_for_block()` → pick all for now.
  - Mining loop (same process):
    - Every N seconds: build block {coinbase + mempool}, `mine_block`, `accept_block`, clear included txs.

- P2P: relay txs and blocks
  - In `p2p_minimal.{h,cpp}`:
    - Handle inbound `tx`: validate → mempool → rebroadcast.
    - Handle inbound `block`: validate/connect → rebroadcast.
    - Optional `inv/getdata` light flow (or directly push `tx`/`block`).
  - CLI/node flags (already similar to README): `--port`, `--connect <host:port>`.

- Persistence (basic, faithful)
  - Append blocks to `data/blocks.dat`; on startup, replay to rebuild UTXO.
  - Keep wallets in `wallets/` (done).
  - This mirrors Satoshi’s “store blocks, derive UTXO” model.

- CLI commands (both machines)
  - `wallet create <name>`
  - `wallet address <name>`
  - `balance <name>` (local chain query)
  - `send <fromName> <toAddress> <amount_sats>` → builds, signs, and broadcasts `tx`.
  - `mine [seconds]` (optional manual mining trigger on Chris’s node)

### How the demo flows
- Chris runs the node: `./babycoin --port 18444` (listens, mines).
- You run a peer: `./babycoin --port 18445 --connect chris.ip:18444`.
- You do:
  - `wallet create alice`
  - `balance alice`
  - `send alice <chris_base58_addr> 100000`
- Your peer signs locally, broadcasts. Chris’s node validates, adds to mempool, mines it into a block, and relays back. Balance updates on both sides.

### Why this stays faithful and simple
- Full validation/mining/persistence live in the node (Satoshi-style).
- Wallet holds private keys and only signs (no chain mutation).
- Network relays tx/block and converges on the tip.
- Blocks are the source of truth; UTXO is derived.

If you want, I can outline exact function signatures and minimal code edits per file next, but this is the smallest path to a real, remote, Satoshi-faithful demo you can run today.