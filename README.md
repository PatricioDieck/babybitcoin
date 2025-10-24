run:

make run

peer1:
./babycoin --port 18445 --connect 127.0.0.1:18444

peer2:
./babycoin --port 18446 --connect 127.0.0.1:18444

add `--interactive` or `-i` for interactive mode
- this sends transactions and blocks to the peer

random transaction tesst 
- send miner 41862e25d7a3b9fd31e7ba7fc7b8df93209ddbac 1000

NOTE- you need to mine the transactions before you can spend them
- run mine command to mine the transactions

^^^ this is because intra-block spending/validation is not yet been implemented (only implemented in BTC by Pieter Wuille (sipa) and others added the UTXO database and layered cache in 2012–2013.)

https://bitcoin.stackexchange.com/questions/117913/what-is-ccoinscacheentry-in-the-bitcoin-core

# Purpose & Specification

The goal of this project is to **develop a clear, working mental model of Bitcoin’s design and codebase** by re-implementing the system described in the 2008 whitepaper, step by step, in **minimal C++**.

Rather than immediately wrestling with the full complexity of Bitcoin Core, this project follows a **happy-path implementation** that emphasizes **clarity, modularity, and simplicity**. By the end, we want a toy blockchain that can be held entirely in memory—something small enough to “fit in the brain” but faithful enough to reflect the real Bitcoin design.

### Why we are doing this

* **Educational clarity**: The Bitcoin codebase (even early 0.1) is large and intimidating. It’s easy to get lost in utilities, constants, or C++ idioms and lose sight of the big picture.
* **Bridging theory and practice**: Reading the whitepaper provides the thousand-foot conceptual view; implementing each section grounds that theory in working code.
* **Mental modeling**: We want to move fluidly between the **macro view** (system design, data flow), the **meso view** (modules, interfaces), and the **micro view** (specific functions, bytes on the wire) without losing coherence.
* **Reusable insight**: A minimal working clone lets us reason about design tradeoffs, improvements, and constraints—and lays the foundation for reading, modifying, and extending Bitcoin Core itself.

### Approach

1. **Follow the whitepaper line-by-line**: Intro → §2 Transactions → §§3–4 Timestamp & PoW → §5 Network → §6 Incentive → etc.
2. **Implement the “happy path” only**:

   * P2PKH transactions, SIGHASH_ALL, fixed block subsidy, leading-zero-bits PoW.
   * No forks, reorgs, compact difficulty encoding, mempool policies, or full Script VM.
3. **Keep it modular**: each whitepaper concept → its own header/implementation file.
4. **Favor simplicity over completeness**: if a detail isn’t essential to understanding, abstract it away; if it is, implement it cleanly.
5. **Iterative sanity checks**: after each module, write a `main()` that exercises it end-to-end, so understanding stays anchored.

### Outcome

By the end of the project we will have:

* A **working mini-Bitcoin** with keys, transactions, blocks, PoW, and a chainstate.
* A **clean modular architecture** with clear interfaces (`crypto`, `script_min`, `tx`, `block`, `chain`, `node` later).
* A **reference spec** (this document) that maps code modules to whitepaper sections, explains why each exists, and documents invariants and workflows.
* The ability to **spin up our own blockchain network**, reason about Bitcoin’s design decisions, and extend the system for experimentation.

---

# Babycoin — Reference Spec (v0, Happy Path)

## 1) Original purpose & constraints (from your brief)

* **Goal:** understand the Bitcoin system by **implementing the whitepaper happy path in C++**—from transactions to blocks, PoW, and validation—**keeping it minimal** so the whole design stays in your head.
* **Method:** follow the **paper section-by-section** (Intro → §2 Transactions → §§3–4 Timestamp & PoW → §6 Incentive → networking), using **familiar names** and **clean interfaces/contracts**.
* **Constraints / style:**

  * Minimal surface area; **one obvious way** to do it.
  * **No full Script VM**; just enough (P2PKH).
  * **Simple PoW target:** “leading-zero bits” instead of compact targets/retarget.
  * **Pure C++** with tiny deps (OpenSSL, header-only HTTP later).
  * **Headers `.h` + `.cpp`** in the same `src/` folder (Satoshi-style).
* **Outcome:** a working, networkable mini-impl that locks in a clear mental model and prepares you to read Bitcoin Core with confidence.

---

## 2) Scope (what’s in vs out)

**In (now):**

* Transactions (§2) with **P2PKH** and **SIGHASH_ALL** (legacy-style).
* Blocks (§§3–4) with Merkle root and PoW (**leading-zero bits**).
* Chainstate: **UTXO set**, block **validation & connection**, **subsidy+fee** budget.
* A **sanity-check main** that mines Block 1 (coinbase), mines Block 2 (spend+coinbase), and prints balances.

**Deferred (later / out-of-scope for happy-path):**

* Full Script engine, witness/segwit, compact `nBits`, difficulty retarget, mempool/relay policies, forks/reorgs, coinbase maturity, persistence, wallet encryption, SPV, DoS defenses, bloom filters, etc.
* The **P2P/gossip** node (next milestone).

---

## 3) Design principles

* **Small nouns, clear verbs:** minimal types that map to the paper (Tx, Block, UTXO, PoW).
* **Contracts you can recite:** each module exposes a few pure, testable functions.
* **Deterministic serialization:** exact bytes → exact hashes; LE integers + CompactSize varints.
* **Local first:** prove behavior locally before adding network complicators.

---

## 4) Core entities & invariants

* **Bytes/Hash256**: `Bytes = std::vector<uint8_t>`, `Hash256 = std::array<uint8_t,32>`.
* **Transaction**: `TxIn(prevout, scriptSig, seq)`, `TxOut(value, scriptPubKey)`, `txid = dbl_sha256(serialize())`.
* **P2PKH**:

  * `scriptPubKey = OP_DUP OP_HASH160 <20B> OP_EQUALVERIFY OP_CHECKSIG`.
  * `scriptSig = <sig DER + 0x01 SIGHASH_ALL> <pubkey>`.
  * Spend is valid iff `HASH160(pub) = h160_in_spk` and `ECDSA_verify(sighash_all(tx,i,spk),sig,pub)`.
* **BlockHeader**: `(version, prev, merkle_root, timestamp, bits, nonce)`, `hash = dbl_sha256(header_bytes)`.
* **PoW (simplified)**: header hash must have **N leading zero bits** (`bits`).
* **Merkle root**: pairwise dbl-sha over txids, duplicate last on odd layers.
* **ChainState** (happy path):

  * UTXO map: key = `txid(LE) || index(LE)`, value = `(amount, scriptPubKey)`.
  * `accept_block` ⇒ PoW ok, merkle ok, coinbase first, txs valid, **coinbase ≤ subsidy+fees**, then update UTXO and tip.

---

## 5) Modules (files), responsibilities, and interfaces

### `common.h/.cpp`

**Why:** canonical bytes & serialization. Everything else depends on it.

* Types: `Bytes`, `Hash256`.
* Hex utils: `hex`, `from_hex`.
* Writers: `write_u32_le`, `write_u64_le`, `write_i64_le`, `write_varint`.
* Reader: `read_varint`.

**Contract:** exact LE + varint encoding for deterministic tx/block hashes.

---

### `crypto.h/.cpp`

**Why:** §2 “chain of digital signatures”.

* Hashing: `sha256`, `dbl_sha256`, `ripemd160`, `hash160`.
* Keys: `generate_keypair()` → **compressed** secp256k1 pubkey (33B).
* Sign/verify: `ecdsa_sign(Hash256, priv) -> DER`, `ecdsa_verify(Hash256, der, pub) -> bool`.

**Contract:** pure functions; deterministic; stable pubkey format (compressed) so P2PKH h160 matches.

---

### `script_min.h/.cpp`

**Why:** lock & check for **P2PKH only** (no VM).

* Build: `p2pkh_script(pubkey) -> Bytes(25)`.
* Parse: `p2pkh_hash_from_script(spk) -> 20B h160` (throws if not P2PKH).

**Contract:** makes/reads the exact P2PKH template we verify against.

---

### `tx.h/.cpp`

**Why:** §2 Transactions (serialize, sign, verify).

* Data: `OutPoint{txid,index}`, `TxIn{prevout,scriptSig,sequence}`, `TxOut{value,scriptPubKey}`, `Transaction`.
* Serialize: `Transaction::serialize()`.
* ID: `Transaction::txid() = dbl_sha256(serialize())`.
* Signing helpers:

  * `sighash_all(tx, i, prevout_spk)` (legacy-style SIGHASH_ALL).
  * `build_p2pkh_scriptSig(sig_der, pub)`.
  * `verify_p2pkh_input(tx, i, prevout_spk)`.

**Contract:** given a referenced P2PKH output, verify input i’s signature + pubkey hash.

---

### `block.h/.cpp`

**Why:** §§3–4 Timestamp server + PoW.

* Data: `BlockHeader`, `Block`.
* Hash/serialize: `BlockHeader::hash()`, `Block::serialize()`.
* Merkle: `merkle_root(vtx)`.
* Mining: `mine_block(Block&)` (sets `nonce` if found).
* PoW check: `header_meets_pow(const BlockHeader&)`.

**Contract:** header binds prev chain & tx set; PoW meets `bits`.

---

### `chain.h/.cpp`

**Why:** validate and advance the ledger (UTXO set, fees, subsidy).

* UTXO API (private): `make_key`, `add_utxo`, `spend_utxo`, `get_utxo`.
* TX validation: `is_coinbase(tx)`, `validate_tx(tx, fee)`.
* Block validation: `validate_block(b, total_fees)`:

  1. `b.vtx[0]` is coinbase.
  2. `header_meets_pow(b.header)`.
  3. `merkle_root(vtx) == header.merkle_root`.
  4. All tx valid; accumulate fees from non-coinbase.
  5. `sum(coinbase outputs) ≤ SUBSIDY + total_fees`.
* Connect: `connect_block(b, total_fees)`:

  * Spend inputs of non-coinbase txs, add all outputs (including coinbase), advance tip/height.
* Public: `accept_block(b)`, `height()`, `tip_hash()`, `balance_for_pubkey(pub)`.
* Constants: `COIN = 100_000_000`, `SUBSIDY = 50*COIN` (fixed for happy path).

**Contract:** if `accept_block` returns true, UTXO set reflects the new tip and balances are updated accordingly.

---

### `main.cpp` (sanity)

**Why:** end-to-end checks up to §6 (without networking).

* Generate keys (miner, Alice).
* Build **coinbase** paying miner; compute txid.
* Build **spend** tx (mock prevout), sign with miner’s priv; verify P2PKH input.
* **Mine Block 1** with coinbase; accept into chain.
* **Mine Block 2** with coinbase + spend; accept into chain.
* Show balances: miner & Alice.

**Contract:** prints `Spend tx input verify: OK`, both blocks mined & accepted, balances as expected (miner 50 + (50 − 10 − change) etc.).

---

## 6) Happy-path workflows (what happens end-to-end)

### A) Transaction (local create → local verify)

1. **Lock**: recipient address = P2PKH script from their pubkey.
2. **Build tx**: inputs reference UTXOs, outputs define P2PKH locks.
3. **Sign**: `sighash_all(tx,i,prev_spk)` → `ecdsa_sign(h, prev_owner_priv)`.
4. **Assemble scriptSig**: `<sig DER + 0x01> <pubkey>`.
5. **Verify**: `verify_p2pkh_input(tx,i,prev_spk)` checks `HASH160(pub)` matches and signature is valid.

### B) Block (timestamp server + PoW)

1. **Assemble**: header fields + `vtx` (coinbase first).
2. **Merkle**: compute `merkle_root(vtx)`.
3. **PoW**: iterate nonce until `leading_zero_bits(header.hash()) >= bits`.
4. **Result**: a block whose header is a valid costly timestamp for its tx set.

### C) Chain (validate & connect)

1. **Validate header**: `header_meets_pow`, prev link (happy path: no forks yet).
2. **Validate body**: coinbase first, merkle matches, each tx valid.
3. **Budget**: coinbase ≤ subsidy + total fees.
4. **Connect**: spend inputs for non-coinbase txs, add all outputs, move tip.

---

## 7) Build & run

```bash
clang++ -std=c++20 -Ithird_party -Isrc -O2 \
  src/common.cpp src/crypto.cpp src/script_min.cpp src/tx.cpp \
  src/block.cpp src/chain.cpp src/main.cpp \
  -lssl -lcrypto -lpthread -o babycoind

./babycoind
```

**Expected logs (indicative):**

* `Spend tx input verify: OK`
* `Block 1: Mined YES … accepted YES`
* `Block 2: Mined YES … accepted YES`
* `Miner balance (sats): 5000000000` (plus Block 2 effects)
* `Alice balance (sats): 1000000000` (if she received 10 BTC in Block 2)

---

## 8) Design choices & rationale

* **Compressed pubkeys** (33B) to keep P2PKH hashes stable and modern.
* **Leading-zero-bits difficulty** for readability (teaches the PoW concept without compact target math).
* **In-memory UTXO** with binary key (`txid||index`) for speed and simplicity.
* **No full Script VM** because §2’s lesson is the signature chain; P2PKH is enough.
* **No reorgs/forks** yet—learn the **happy path** first, then add complexity incrementally.

---

## 9) Known simplifications / limitations

* No coinbase maturity, no fee rules/policies, no mempool eviction, no orphan handling, no chainwork comparison (we assume append-only), no persistence (restart loses state), no DoS protections, no compact `nBits`/retargeting, no time drift checks, no standardness/policy rules, no full Script/opcodes.

These are **intentional** to keep the brain-sized model while you internalize the flow.

---

## 10) Next milestone: §5 Networking (gossip)

**Plan:** add a tiny P2P layer (`node.h/.cpp`) using a header-only HTTP server/client (e.g., `httplib.h`) with three message types:

* `inv` (announce `txid`/`blockhash`),
* `getdata` (request raw tx/block by id),
* `tx` / `block` (send raw object).

**Happy-path behavior:** on `tx`: validate (against current UTXO), add to mempool (or hold), rebroadcast `inv`. On `block`: validate & connect; request missing txs if announced first.

We’ll keep it **stateless & explicit**, mirroring the whitepaper’s network sketch and modern Core’s `inv/getdata` rhythm—just simplified.

---

If you want, I can convert this spec into a `README.md` and a small ASCII diagram or two for your repo so every future change stays anchored to this doc.

---

Perfect — here’s an **ASCII diagram** that shows how all of the modules fit together and flow into each other. You can paste this right under the “Modules” section of your README/spec sheet.

---

```
                 +-------------------+
                 |   common.{h,cpp}  |
                 |-------------------|
                 |  Bytes, Hash256   |
                 |  LE encoding      |
                 |  varints, hex     |
                 +---------+---------+
                           |
                           v
                 +-------------------+
                 |   crypto.{h,cpp}  |
                 |-------------------|
                 |  sha256, dbl_sha  |
                 |  ripemd160,hash160|
                 |  ECDSA key/sign   |
                 +---------+---------+
                           |
                           v
                 +-------------------+
                 | script_min.{h,cpp}|
                 |-------------------|
                 |  build/parse P2PKH|
                 +---------+---------+
                           |
                           v
                 +-------------------+
                 |     tx.{h,cpp}    |
                 |-------------------|
                 |  OutPoint, TxIn   |
                 |  TxOut, Tx        |
                 |  serialize, txid  |
                 |  sighash, verify  |
                 +---------+---------+
                           |
                           v
                 +-------------------+
                 |   block.{h,cpp}   |
                 |-------------------|
                 | BlockHeader, Block|
                 | merkle_root, hash |
                 | mine_block, PoW   |
                 +---------+---------+
                           |
                           v
                 +-------------------+
                 |   chain.{h,cpp}   |
                 |-------------------|
                 |   UTXO set        |
                 |   validate_tx     |
                 |   validate_block  |
                 |   accept_block    |
                 |   balances        |
                 +---------+---------+
                           |
                           v
                 +-------------------+
                 |    main.cpp       |
                 |-------------------|
                 |  Sanity checks    |
                 |  Keypairs, Tx,    |
                 |  Block mining     |
                 |  Chain accept     |
                 +-------------------+
```

---

### How to read this

* **Vertical flow** = dependencies: each box builds on the one above it.
* **common → crypto → script_min → tx → block → chain → main** mirrors the paper’s order.
* **main.cpp** sits at the bottom and ties everything together for sanity checks.

---

Do you want me to also create a **workflow diagram** (showing how transactions become blocks, blocks extend the chain, and balances update), or should we keep it strictly module-oriented for now?

# Modularity/Layering in the code


### **Current Architecture is Properly Layered:**

```
┌─────────────────┐
│   p2p_minimal   │  ← Highest level (uses everything)
├─────────────────┤
│     chain       │  ← Uses tx, block, common
├─────────────────┤  
│  block + tx     │  ← Use crypto, script, common
├─────────────────┤
│ script_minimal  │  ← Uses crypto + common
├─────────────────┤
│    crypto       │  ← Uses only common
├─────────────────┤
│    common       │  ← Foundation (no dependencies)
└─────────────────┘
```

### **Why This **Enhances** Modularity, Not Destroys It:**

1. **Single Responsibility**: Each layer has one clear purpose
   - `common`: Basic types and serialization
   - `crypto`: Cryptographic operations  
   - `script`: Script building/verification
   - `tx/block`: Domain objects
   - `chain`: Chain state management
   - `p2p`: Network communication

2. **Dependency Direction**: Dependencies flow **downward only**
   - Higher layers depend on lower layers
   - Lower layers know nothing about higher layers
   - **No circular dependencies**

3. **Loose Coupling**: Each module only depends on what it actually needs
   - `crypto` doesn't know about transactions
   - `tx` doesn't know about networking
   - `chain` doesn't know about P2P details

4. **High Cohesion**: Related functionality is grouped together
   - All serialization in `common`
   - All crypto in `crypto`  
   - All script logic in `script_min`

### **This is Actually **Better** Than "Pure" Modularity:**

Your approach follows **"Pragmatic Modularity"** rather than dogmatic separation:

✅ **Benefits of Your Approach:**
- **Code reuse**: `Hash256`, `Bytes` used everywhere consistently
- **DRY principle**: Serialization logic centralized
- **Type safety**: Consistent types across modules
- **Maintainability**: Changes to serialization only need to happen in one place

❌ **Problems with Overly Strict Modularity:**
- **Code duplication**: Every module reimplements `hex()`, `write_u32_le()`, etc.
- **Inconsistency**: Different modules might serialize integers differently
- **Bugs**: Each module has its own serialization bugs to fix
- **Maintenance nightmare**: Update serialization in 6 different places

### **The Key Insight:**

**Shared utilities don't destroy modularity - they enable it!** The key is:
- **Direction matters**: Bottom-up dependencies = good
- **Purpose matters**: Utilities should be pure functions with no business logic
- **Stability matters**: Common utilities should be the most stable part of your codebase

---

Modules inside of the project:

# Object Structure Quick Reference

## Transaction
- version: int32_t
- vin: vector<TxIn>
- vout: vector<TxOut>  
- locktime: uint32_t

## TxIn (Transaction Input)
- prevout: OutPoint
  - txid: Hash256 (32 bytes)
  - index: uint32_t
- scriptSig: Bytes (unlocking script)
- sequence: uint32_t

## TxOut (Transaction Output)
- value: int64_t (satoshis)
- scriptPubKey: Bytes (locking script)

## Block
- header: BlockHeader
  - version, prev_block, merkle_root, timestamp, bits, nonce
- vtx: vector<Transaction>

## P2PConfig
- listen_port: uint16_t
- peers: vector<PeerAddress>
  - host: string
  - port: uint16_t
- demo_mode: bool
- demo_tx_to_send: int
- demo_ms_between: int

## ChainState
- height(): uint32_t
- tip_hash(): Hash256
- balance_for_pubkey(): int64_t
- accept_block(): bool


---

# What to do now 

## This is really tough 

- i am currently in a lul on what to do next. what follow are my speculation on the next steps 
  - implement **UI client** that executes the TX
  - create **constant mining state**
    - be **transaction aware ** 
  - enforce the new-block, **miner subsidy every ten minutes **
    - save chain **checkpoints** in local dir + fetch on startup
    - get this shit to run on my computer and make a little RN client wallet

maybe make tests 
- **printing all the data** as it gets passed around the chain 
- print the **chain** that gets saved on checkpoint 

maybe control the program **from the CLI** 
- **terminal** that enables transactions 
  - check balances 
  - create wallets w names  
  - sign txs

maybe **read the whitepaper again**