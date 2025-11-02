### L1 listing: what you must ship (plain and prioritized)

- Consensus (must)
  - Pick tip by total work (chainwork), not height.
  - Handle reorgs (keep orphans; reorg up to N blocks).
  - Headers-first sync.

- Difficulty/Timing (must)
  - Use compact target `nBits`; verify targets.
  - Retarget every X blocks (simple, predictable rule).
  - Median-time-past; cap clock drift.

- Blocks and rewards (must)
  - Max block size/weight.
  - Coinbase maturity (e.g., 100 blocks).
  - Emission schedule (fixed or halving), fixed fee rules.

- Mempool and fees (must)
  - Full validation before relay; reject non-standard.
  - Min relay fee; feerate-based eviction; (optional) RBF.
  - Double-spend protection within mempool.

- Persistence and indexes (must)
  - Append-only block store + tx index.
  - Archival and pruned modes.
  - Atomic writes; snapshots for fast boot.

- P2P hardening (must)
  - Message size caps; rate limits.
  - Peer scoring/bans; DoS protections.
  - DNS seeds/bootnodes; keepalives.

- RPC and tooling (must)
  - JSON-RPC parity: getblock/getblockhash/getblockcount/getrawtransaction/sendrawtransaction/estimatesmartfee/getmempoolinfo/getchaintips.
  - Stable address format (Base58Check or Bech32).
  - Block explorer; Docker images; reproducible builds.

- Wallet standards (should)
  - BIP32/39 HD wallets; hardware wallet support.
  - Clear address checksum and network prefixes.

- Security (must)
  - External audits (consensus, P2P, storage).
  - Fuzzing + sanitizers in CI; bug bounty.

- Ops (must)
  - Metrics (Prometheus), structured logs, health endpoints.
  - Snapshots, bootstrap nodes, upgrade/runbooks.

- Compliance/exchange (must)
  - Written reorg/confirmation policy.
  - Custody partner readiness (deposit/withdraw guide).
  - Legal review of token/economics.


### 90-day, no-fluff plan

- Weeks 1–3: chainwork + reorgs + headers-first; `nBits` + retarget; block store + tx index.
- Weeks 4–6: mempool policy + fees + block limits + coinbase maturity; P2P hardening + seeds.
- Weeks 7–9: full RPC set; explorer; HD wallets + Base58/Bech32; snapshots/pruning.
- Weeks 10–12: audits + fuzzing; ops/monitoring; exchange integration docs (confirmations, reorg policy), release testnet and mainnet.

Want me to turn this into a tight GitHub issue checklist mapped to your current files? I’ll keep it short and actionable [[memory:8295392]] [[memory:8295387]].