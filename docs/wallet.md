## **🎯 First Principles: Wallet Design in Your Bitcoin System**

A wallet in your Baby Bitcoin system is fundamentally different from a traditional wallet. Let's break down the **core concepts**:

## **🔑 1. What is a Wallet in Your System?**

**A wallet is essentially:**
- **A keypair** (`KeyPair` from your crypto module)
- **An address** (derived from the public key)
- **A way to create and sign transactions**

**Key insight:** In Bitcoin-like systems, wallets don't "store" BTC - they store the **private keys** that prove ownership of UTXOs in the blockchain.

## **💾 2. Wallet Storage (First Implementation)**

**Simplest approach:**
```cpp
struct Wallet {
    std::string name;
    KeyPair keys;  // ← Your existing KeyPair struct
    std::string address;  // ← hash160(public_key)
};
```

**File format (start simple):**
```json
{
  "name": "alice",
  "private_key": "hex_encoded_32_bytes",
  "public_key": "hex_encoded_33_bytes", 
  "address": "hex_encoded_20_bytes"
}
```

## **⚙️ 3. Core Wallet Operations**

**What should a wallet be able to do?**

1. **Create** - Generate new keypair, save to file
2. **Load** - Read keypair from file, verify it works
3. **Get Balance** - Query ChainState for UTXOs owned by this wallet
4. **Create Transaction** - Build a transaction spending wallet's UTXOs
5. **Sign Transaction** - Sign the transaction with wallet's private key

## **🔗 4. Integration with Your Existing System**

**Your wallet needs to work with:**
- **ChainState** (to check balances and spend UTXOs)
- **P2PNode** (to broadcast transactions)
- **Transaction creation** (build tx from wallet's UTXOs)

## **🎯 5. Implementation Plan (Start Small)**

**Week 1: Basic Wallet Storage**
```cpp
// 1. Create wallet function
Wallet create_wallet(const std::string& name) {
    KeyPair keys = generate_keypair();
    std::string address = hex(hash160(keys.pub));
    
    // Save to wallets/name.json
    // Return wallet object
}

// 2. Load wallet function  
Wallet load_wallet(const std::string& name) {
    // Read from wallets/name.json
    // Parse JSON, reconstruct KeyPair
    // Return wallet object
}
```

**Week 2: Balance & Transaction Creation**
```cpp
// 3. Get balance function
int64_t get_wallet_balance(const Wallet& wallet, ChainState& cs) {
    return cs.balance_for_pubkey(wallet.keys.pub);
}

// 4. Create transaction function
Transaction create_transaction(const Wallet& from_wallet, 
                              const std::string& to_address,
                              int64_t amount, ChainState& cs) {
    // Find UTXOs owned by from_wallet
    // Build transaction spending those UTXOs
    // Return unsigned transaction
}
```

## **🔒 6. Security Considerations**

**For now (MVP):**
- Store private keys as hex in JSON (not encrypted)
- Only for development/testing

**For production:**
- Encrypt private keys in wallet files
- Use secure key derivation
- Add wallet backup/restore

## **📁 7. File Structure**

**Recommended structure:**
```
src/
├── wallet.h      // Wallet class, save/load functions
├── wallet.cpp    // Implementation
└── wallets/      // Directory for wallet files
    ├── alice.json
    ├── bob.json
    └── ...
```

## **🚀 8. Integration with Your CLI**

**Your CLI commands should:**
```cpp
// In main2.cpp CLI handler:
case Command::WALLET_CREATE: {
    if (args.size() < 2) {
        std::cout << "Usage: wallet <name>\n";
        break;
    }
    Wallet wallet = create_wallet(args[1]);
    std::cout << "Created wallet: " << wallet.name << "\n";
    std::cout << "Address: " << wallet.address << "\n";
    break;
}

case Command::BALANCE: {
    Wallet wallet = load_wallet(args[1]);
    int64_t balance = get_wallet_balance(wallet, cs);
    std::cout << "Balance: " << balance / 100000000.0 << " BTC\n";
    break;
}
```

## **💭 Key Insight**

**Your wallet is just a thin layer over your existing blockchain!** It:
- Uses your `KeyPair` struct
- Queries your `ChainState` for balances
- Creates transactions using your existing transaction logic
- Broadcasts via your `P2PNode`

**The "magic" happens in your existing blockchain code** - the wallet just provides a convenient interface to interact with it!

## **🎯 Next Step**

**Start with the simplest possible wallet:**

1. **Create a wallet** - Generate keypair, save to file
2. **Load a wallet** - Read keypair from file  
3. **Check balance** - Use existing `cs.balance_for_pubkey()`

**This gives you a working wallet in 1-2 days** that integrates perfectly with your existing blockchain!

**Which part should we implement first?**