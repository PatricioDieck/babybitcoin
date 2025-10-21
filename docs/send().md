# Add send(from,to,amount) in wallet layer

### Decision

- **Put `send` in `wallet`**: a high-level orchestration that loads keys, selects UTXOs, builds and signs a tx.
- **Keep `tx` pure**: only stateless helpers (serialize, txid, sighash, build scripts), no IO or key storage.

### Design

- `wallet.h/.cpp`:
- Add `Transaction send(const std::string& fromName, const std::string& toAddressHex, int64_t amount, ChainState& cs, int64_t fee = 1000);`
- Steps: load wallet → derive `hash160(pub)` → ask chain for coins → build outputs (to, change) → build unsigned tx → sign each input with `sighash_all` + `build_p2pkh_scriptSig` → return signed `Transaction`.
- `chain.h/.cpp`:
- Add minimal helper: `std::vector<std::tuple<Hash256,uint32_t,int64_t,Bytes>> gather_coins(const Bytes& pub, int64_t target) const;` (txid, vout, value, scriptPubKey). Only scans current UTXO map.
- `tx.h/.cpp`:
- Reuse existing helpers: `sighash_all`, `build_p2pkh_scriptSig`. Optionally add a tiny utility to build an unsigned tx from inputs/outputs for readability.

### Behavior

- Returns a fully signed `Transaction` (caller can mine/broadcast later).
- Throws on errors (insufficient funds, bad address, no wallet).
- P2PKH-only; fixed fee for now.

### CLI (optional)

- Command: `send <from> <to_address_hex> <amount_sats>` → builds tx and prints txid/hex.

### Notes

- Keeps modules clean: wallet = keys + signing + orchestration; tx = data/crypto helpers; chain = state/UTXO queries.
- Future: replace `gather_coins` with better coin selection and fee estimation.