#pragma once
//
// BabyBitcoin P2P (header) — minimal, whitepaper-faithful gossip network.
// Messages: VERSION, VERACK, TX, BLOCK.
//  - TX: broadcast without validation (per our decision).
//  - BLOCK: validated via ChainState::accept_block(b) before re-gossip.
//
// Also supports an optional "demo mode" that self-broadcasts a few dummy TXs
// after handshake so you can see traffic in terminals.
//
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include <chrono>

#include "common.h"   // Bytes, write_u32_le, read_varint, hex
#include "crypto.h"   // sha256, dbl_sha256
#include "tx.h"       // Transaction, OutPoint, TxIn, TxOut
#include "block.h"    // Block, BlockHeader
#include "chain.h"    // ChainState


// For CLI convenience: host/port tuple to dial.
struct PeerAddress {
  std::string host;
  uint16_t    port;
};

// Node startup knobs.
struct P2PConfig {
  uint16_t listen_port = 18444;
  std::vector<PeerAddress> peers;    // optional outbound peers
  int select_timeout_ms = 250;

  // Demo mode (optional): auto-send a couple TXs so you can see logs flowing.
  bool demo_mode = false;
  int  demo_tx_to_send = 3;          // total demo TXs to originate
  int  demo_ms_between = 1000;       // delay between demo TXs
};

class P2PNode {
public:
  // we are constructing a P2PNode with a private ***reference*** to ChainState (ChainState is a class that holds the chain data)
  explicit P2PNode(ChainState& chain);
  ~P2PNode();

  // listen + (optional) dial + start IO thread
  bool start(const P2PConfig& cfg);

  // clean shutdown (idempotent)
  void stop();

  // App-level broadcasts when *you* originate local data:
  void broadcast_local_tx(const Transaction& tx);   // no validation
  void broadcast_local_block(const Block& b); // after you accept_block()

  ChainState& chain() { return chain_; }

  // snapshot of the mempool to mine all Txs into a block
  std::vector<Transaction> mempool_snapshot();
  
  // Optional: clear entire mempool (tests/reset)
  void clear_mempool();

  // Remove included txs after a block is accepted
  void prune_mined_txs(const Block& b);

private:
  // --- Peer bookkeeping ---
  struct Peer {
    int         fd{-1};
    std::string address;           // "ip:port" for logs
    bool        handshaked{false};
    bool        outbound{false};
    uint32_t    height{0}; // cached from VERSION handshake
    bool        has_height{false};
  };

  // --- Protocol constants & header ---
  static constexpr uint32_t NET_MAGIC = 0xBABECAFE;

  enum class MsgType : uint8_t {
    VERSION = 0x01, // payload: u32 proto_ver, u32 height
    VERACK  = 0x02, // payload: empty
    TX      = 0x10, // payload: Transaction::serialize()
    BLOCK   = 0x11, // payload: Block::serialize()
  };

/* 
#pragma pack(1) tells the compiler: "pack this struct tightly with no padding"
This ensures the struct is exactly 13 bytes (4+1+4+4) with no gaps 
*/
#pragma pack(push,1)
  struct MsgHeader {
    uint32_t magic;
    uint8_t  type;
    uint32_t length_le;
    uint32_t csum_le;
  };
#pragma pack(pop)

  // --- Lifecycle & loop ---
  bool listen_on_(uint16_t port);
  bool connect_outbound_(const std::string& host, uint16_t port);
  void io_loop_();
  void accept_inbound_();
  bool handle_peer_io_(size_t idx);

  // --- Dispatch & handshake ---
  void dispatch_(Peer& p, const MsgHeader& h, const Bytes& payload);
  void send_version_(Peer& p);
  void send_verack_(Peer& p);

  // --- Frame send/recv ---
  static bool send_frame_(int fd, MsgType t, const Bytes& payload);
  static bool recv_frame_(int fd, MsgHeader& h, Bytes& payload);

  // --- POSIX sockets ---
  static int  tcp_listen_(uint16_t port);
  static int  tcp_accept_(int listen_fd, std::string& out_addr);
  static int  tcp_connect_(const std::string& host, uint16_t port);
  static bool write_all_(int fd, const uint8_t* p, size_t n);
  static bool read_n_(int fd, uint8_t* p, size_t n);
  static void close_fd_(int fd);

  // --- Helpers ---
  static uint32_t    checksum4_le_(const Bytes& payload);

  // --- Demo mode helpers ---
  Transaction make_demo_tx_(uint32_t n);
  void maybe_send_demo_tx_();

  // --- Peer chain sync ---
  void maybe_catch_up_(Peer& p);

  // --- State ---
  ChainState& chain_;
  int listen_fd_{-1};
  std::vector<Peer> peers_;

  std::unordered_set<std::string> seen_tx_;
  std::unordered_set<std::string> seen_block_;
  std::vector<Transaction> mempool_; // optional: local cache

  std::thread io_thread_;
  std::atomic<bool> running_{false};
  int select_timeout_ms_{250};
  std::mutex mtx_;

  // demo
  bool demo_mode_{false};
  int  demo_remaining_{0};
  int  demo_interval_ms_{1000};
  uint32_t demo_nonce_{0};
  std::chrono::steady_clock::time_point demo_next_;
};

