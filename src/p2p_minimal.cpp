#include "p2p_minimal.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <iomanip>


#define LOG(msg) do { std::cout << "[P2P] " << msg << std::endl; } while(0)

// ---------------- Helpers ----------------
uint32_t P2PNode::checksum4_le_(const Bytes& payload) {
  Hash256 h = dbl_sha256(payload);
  return (uint32_t)h[0] | ((uint32_t)h[1]<<8) | ((uint32_t)h[2]<<16) | ((uint32_t)h[3]<<24);
}

// -------------- POSIX sockets ------------

bool P2PNode::write_all_(int fd, const uint8_t* p, size_t n) {
  while (n) { ssize_t w = ::send(fd, p, n, 0); if (w<=0) return false; p+=w; n-= (size_t)w; }
  return true;
}
bool P2PNode::read_n_(int fd, uint8_t* p, size_t n) {
  while (n) { ssize_t r = ::recv(fd, p, n, 0); if (r<=0) return false; p+=r; n-= (size_t)r; }
  return true;
}
void P2PNode::close_fd_(int fd) { if (fd>=0) ::close(fd); }

int P2PNode::tcp_listen_(uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd<0) return -1;
  int yes=1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(port); a.sin_addr.s_addr=INADDR_ANY;
  if (bind(fd,(sockaddr*)&a,sizeof(a))<0) { close_fd_(fd); return -1; }
  if (listen(fd, 16)<0) { close_fd_(fd); return -1; }
  return fd;
}
int P2PNode::tcp_accept_(int listen_fd, std::string& out_addr) {
  sockaddr_in client_address{}; 
  socklen_t len=sizeof(client_address); 

  // accept the connection- file descriptor of the new connection
  int fd = ::accept(listen_fd, (sockaddr*)&client_address, &len);
  if (fd<0) return -1; // if the connection fails, return -1
  char ip[64]; // ip address
  ::inet_ntop(AF_INET, &client_address.sin_addr, ip, sizeof(ip)); // convert the address to a string

  // HIDDEN SIDE EFFECT: convert the address to a string
  out_addr = std::string(ip) + ":" + std::to_string(ntohs(client_address.sin_port));
  return fd;
}
int P2PNode::tcp_connect_(const std::string& host, uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd<0) return -1;
  hostent* he = gethostbyname(host.c_str());
  if (!he) { close_fd_(fd); return -1; }
  sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(port);
  std::memcpy(&a.sin_addr, he->h_addr_list[0], he->h_length);
  if (connect(fd,(sockaddr*)&a,sizeof(a))<0) { close_fd_(fd); return -1; }
  return fd;
}

// -------------- Frames (header + payload) ------------

bool P2PNode::send_frame_(int fd, MsgType t, const Bytes& payload) {
  MsgHeader h{ NET_MAGIC, (uint8_t)t, (uint32_t)payload.size(), checksum4_le_(payload) };
  return write_all_(fd, (const uint8_t*)&h, sizeof(h)) &&
         (payload.empty() || write_all_(fd, payload.data(), payload.size()));
}
bool P2PNode::recv_frame_(int fd, MsgHeader& h, Bytes& payload) {
  if (!read_n_(fd, (uint8_t*)&h, sizeof(h))) return false;
  if (h.magic != NET_MAGIC) return false;

  // optional guardrails: cap payload size (non-consensus)
  // const uint32_t MAX_TX = 1'000'000;
  // const uint32_t MAX_BLOCK = 4'000'000;
  // if (h.type == (uint8_t)MsgType::TX    && h.length_le > MAX_TX)    return false;
  // if (h.type == (uint8_t)MsgType::BLOCK && h.length_le > MAX_BLOCK) return false;

  payload.resize(h.length_le);
  if (h.length_le && !read_n_(fd, payload.data(), payload.size())) return false;
  return checksum4_le_(payload) == h.csum_le;
}

// -------------- Parsers (inverse of your serialize) ------------

OutPoint P2PNode::parse_outpoint_(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 36) throw std::runtime_error("outpoint eof");
  OutPoint op{};
  std::memcpy(op.txid.data(), p, 32); p += 32;
  op.index = read_u32_le_(p, end);
  return op;
}
TxIn P2PNode::parse_txin_(const uint8_t*& p, const uint8_t* end) {
  TxIn in{};
  in.prevout = parse_outpoint_(p, end);
  uint64_t slen = read_varint(p, end);
  if ((uint64_t)(end - p) < slen) throw std::runtime_error("scriptsig eof");
  in.scriptSig.assign(p, p + slen); p += slen;
  in.sequence = read_u32_le_(p, end);
  return in;
}
TxOut P2PNode::parse_txout_(const uint8_t*& p, const uint8_t* end) {
  TxOut o{};
  o.value = read_i64_le_(p, end);
  uint64_t plen = read_varint(p, end);
  if ((uint64_t)(end - p) < plen) throw std::runtime_error("scriptpubkey eof");
  o.scriptPubKey.assign(p, p + plen); p += plen;
  return o;
}
Transaction P2PNode::parse_tx_(const uint8_t*& p, const uint8_t* end) {
  Transaction tx{};
  tx.version = (int32_t)read_u32_le_(p, end);
  uint64_t nin = read_varint(p, end);  tx.vin.reserve((size_t)nin);
  for (uint64_t i=0;i<nin;i++) tx.vin.push_back(parse_txin_(p, end));
  uint64_t nout = read_varint(p, end); tx.vout.reserve((size_t)nout);
  for (uint64_t i=0;i<nout;i++) tx.vout.push_back(parse_txout_(p, end));
  tx.locktime = read_u32_le_(p, end);
  return tx;
}
BlockHeader P2PNode::parse_header_(const uint8_t*& p, const uint8_t* end) {
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
Block P2PNode::parse_block_(const uint8_t*& p, const uint8_t* end) {
  Block b{};
  b.header = parse_header_(p, end);
  uint64_t ntx = read_varint(p, end);
  b.vtx.reserve((size_t)ntx);
  for (uint64_t i=0;i<ntx;i++) b.vtx.push_back(parse_tx_(p, end));
  return b;
}

// -------------- Demo mode helpers --------------

Transaction P2PNode::make_demo_tx_(uint32_t n) {
  // Construct a tiny-but-validly-serialized TX. Not spendable/valid at consensus,
  // but perfect to exercise gossip. Unique by 'n'.
  Transaction tx;

  // Fake prevout: txid = sha256("demo"||n), index = n
  Bytes seed; seed.insert(seed.end(), {'d','e','m','o'}); write_u32_le(seed, n);
  Hash256 h = sha256(seed);
  TxIn in;
  std::memcpy(in.prevout.txid.data(), h.data(), 32);
  in.prevout.index = n;
  in.scriptSig = Bytes{'d','e','m','o', (uint8_t)(n & 0xff)}; // arbitrary
  tx.vin.push_back(in);

  // One zero-value output (scriptPubKey empty). Still serializes fine.
  TxOut out;
  out.value = 0;
  out.scriptPubKey = {};
  tx.vout.push_back(out);

  return tx;
}

void P2PNode::maybe_send_demo_tx_() {
  if (!demo_mode_ || demo_remaining_ <= 0) return;

  // Only send if we have at least one handshaked peer.
  bool any_ok=false;
  for (auto& p : peers_) if (p.handshaked) { any_ok=true; break; }
  if (!any_ok) return;

  auto now = std::chrono::steady_clock::now();
  if (now < demo_next_) return;

  Transaction tx = make_demo_tx_(++demo_nonce_);
  LOG("demo: originating TX " << be_hex_key_(tx.txid()).substr(0,12) << "…");
  broadcast_local_tx(tx);
  demo_remaining_--;
  demo_next_ = now + std::chrono::milliseconds(demo_interval_ms_);
}

// -------------- Lifecycle --------------

P2PNode::P2PNode(ChainState& chain) : chain_(chain) {}
P2PNode::~P2PNode() { stop(); }

bool P2PNode::listen_on_(uint16_t port) {
  listen_fd_ = tcp_listen_(port);
  if (listen_fd_ >= 0) LOG("listening on " << port);
  return listen_fd_ >= 0;
}
bool P2PNode::connect_outbound_(const std::string& host, uint16_t port) {
  int fd = tcp_connect_(host, port);
  if (fd < 0) { LOG("outbound connect failed to " << host << ":" << port); return false; }
  peers_.push_back(Peer{fd, host+":"+std::to_string(port), false, true});
  LOG("connected outbound to " << peers_.back().address);
  send_version_(peers_.back()); // kick off handshake
  return true;
}

bool P2PNode::start(const P2PConfig& cfg) {
  select_timeout_ms_ = cfg.select_timeout_ms;
  demo_mode_         = cfg.demo_mode;
  demo_remaining_    = cfg.demo_tx_to_send;
  demo_interval_ms_  = cfg.demo_ms_between;
  demo_nonce_        = 0;
  demo_next_         = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);

  if (!listen_on_(cfg.listen_port)) return false;
  for (const auto& pa : cfg.peers) (void)connect_outbound_(pa.host, pa.port);

  running_.store(true);
  io_thread_ = std::thread([this]{ this->io_loop_(); });
  return true;
}

void P2PNode::stop() {
  bool expected = true;
  if (running_.compare_exchange_strong(expected, false)) {
    if (io_thread_.joinable()) io_thread_.join();
    if (listen_fd_ >= 0) { close_fd_(listen_fd_); listen_fd_ = -1; }
    for (auto& p : peers_) { LOG("closing " << p.address); close_fd_(p.fd); }
    peers_.clear();
    seen_tx_.clear();
    seen_block_.clear();
    mempool_.clear();
  }
}

// -------------- Public broadcasts --------------

// local bc we originated (no validation)- relay is in dispatch_
void P2PNode::broadcast_local_tx(const Transaction& tx) {
  const auto key = be_hex_key_(tx.txid());
  std::lock_guard<std::mutex> lock(mtx_);
  if (!seen_tx_.insert(key).second) { LOG("→ TX local " << key.substr(0,12) << "… (dup, skip)"); return; }
  Bytes pl = tx.serialize();
  size_t fan=0;
  for (auto& p : peers_) if (p.handshaked) { send_frame_(p.fd, MsgType::TX, pl); fan++; }
  LOG("→ TX local " << key.substr(0,12) << "… to " << fan << " peer(s)");
}

void P2PNode::broadcast_local_block(const Block& b) {
  const auto key = be_hex_key_(b.header.hash());
  std::lock_guard<std::mutex> lock(mtx_);
  if (!seen_block_.insert(key).second) { LOG("→ BLOCK local " << key.substr(0,12) << "… (dup, skip)"); return; }
  Bytes pl = b.serialize();
  size_t fan=0;
  for (auto& p : peers_) if (p.handshaked) { send_frame_(p.fd, MsgType::BLOCK, pl); fan++; }
  LOG("→ BLOCK local " << key.substr(0,12) << "… to " << fan << " peer(s)");
}

// -------------- IO thread --------------

void P2PNode::io_loop_() {
  while (running_.load()) {
    fd_set rfds; 
    FD_ZERO(&rfds);
    int maxfd = -1;
  
    // this is the list of indexes of the peers
    std::vector<int> idxs; 
    idxs.reserve(peers_.size());
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (listen_fd_ >= 0) { 
        FD_SET(listen_fd_, &rfds); 
        maxfd = std::max(maxfd, listen_fd_); 
      }
      for (size_t i=0;i<peers_.size();++i) {
        FD_SET(peers_[i].fd, &rfds);
        maxfd = std::max(maxfd, peers_[i].fd);
        idxs.push_back((int)i);
      }
    }

    timeval tv{ select_timeout_ms_/1000, (select_timeout_ms_%1000)*1000 };
    // rv stands for return value
    int rv = select(maxfd+1, &rfds, nullptr, nullptr, &tv);
    if (rv <= 0) { maybe_send_demo_tx_(); continue; }

    // if the listen fd is set, accept an inbound connection
    if (listen_fd_ >= 0 && FD_ISSET(listen_fd_, &rfds)) accept_inbound_();

    for (int idx : idxs) {
      std::lock_guard<std::mutex> lock(mtx_);
      if ((size_t)idx >= peers_.size()) continue;
      if (FD_ISSET(peers_[idx].fd, &rfds)) {
        if (!handle_peer_io_((size_t)idx)) { /* peer removed; indexes shift */ }
      }
    }

    // After handling IO, try demo again
    maybe_send_demo_tx_();
  }
}

void P2PNode::accept_inbound_() {
  std::string addr;
  int fd = tcp_accept_(listen_fd_, addr);
  if (fd < 0) return;
  peers_.push_back(Peer{fd, addr, false, false});
  LOG("accepted inbound from " << addr << " (awaiting VERSION)");
  // Inbound peer should send VERSION first; we'll reply with VERACK.
}

bool P2PNode::handle_peer_io_(size_t idx) {
  if (idx >= peers_.size()) return false;  // ← Peer doesn't exist
  Peer& peer = peers_[idx]; // ← Get peer
  MsgHeader header; 
  Bytes payload; 
  if (!recv_frame_(peer.fd, header, payload)) {
    LOG("recieve failed, peer closed " << peer.address);
    close_fd_(peer.fd);
    peers_.erase(peers_.begin()+idx);
    return false;
  }
  // dispatch the message to the peer
  dispatch_(peer, header, payload);
  return true;
}

// -------------- Dispatch & handshake --------------

void P2PNode::send_version_(Peer& p) {
  Bytes payload;
  write_u32_le(payload, 1);   // protocol version
  write_u32_le(payload, chain_.height()); // info only
  LOG("→ VERSION to " << p.address << " (height=" << chain_.height() << ")");
  send_frame_(p.fd, MsgType::VERSION, payload);
}
void P2PNode::send_verack_(Peer& p) {
  LOG("→ VERACK to " << p.address);
  send_frame_(p.fd, MsgType::VERACK, Bytes{});
  p.handshaked = true;
}

void P2PNode::dispatch_(Peer& p, const MsgHeader& h, const Bytes& payload) {
  const uint8_t* cur = payload.data();
  const uint8_t* end = payload.data() + payload.size();

  switch ((MsgType)h.type) {
    case MsgType::VERSION: {
      uint32_t peer_proto  = read_u32_le_(cur, end);
      uint32_t peer_height = read_u32_le_(cur, end);
      LOG("← VERSION from " << p.address << " (proto=" << peer_proto << ", height=" << peer_height << ")");
      send_verack_(p);
      break;
    }
    case MsgType::VERACK: {
      LOG("← VERACK from " << p.address);
      p.handshaked = true;
      break;
    }
    case MsgType::TX: {
      Transaction tx = parse_tx_(cur, end);
      auto key = be_hex_key_(tx.txid());
      LOG("← TX " << key.substr(0,12) << "… from " << p.address);
      if (!seen_tx_.insert(key).second) { LOG("  (dup)"); break; }
      mempool_.push_back(tx); // optional
      Bytes pl = tx.serialize();
      size_t fan=0;

      for (auto& q : peers_) if (q.handshaked && q.fd != p.fd) { 
        // send the tx to all other peers
        send_frame_(q.fd, MsgType::TX, pl); 
        fan++; 
      }

      if (fan) LOG("  relayed TX to " << fan << " peer(s)");
      break;
    }
    case MsgType::BLOCK: {
      Block b = parse_block_(cur, end);
      auto key = be_hex_key_(b.header.hash());
      LOG("← BLOCK " << key.substr(0,12) << "… from " << p.address);
      if (!seen_block_.insert(key).second) { LOG("  (dup)"); break; }

      if (chain_.accept_block(b)) {
        LOG("  block accepted (height=" << chain_.height() << ")");
        // prune mempool entries included in block (optional)
        if (b.vtx.size() > 1) {
          std::unordered_set<std::string> in_block;
          for (size_t i=1;i<b.vtx.size();++i) in_block.insert(be_hex_key_(b.vtx[i].txid()));
          std::vector<Transaction> keep; keep.reserve(mempool_.size());
          for (auto& t : mempool_) if (!in_block.count(be_hex_key_(t.txid()))) keep.push_back(std::move(t));
          mempool_.swap(keep);
        }
        Bytes pl = b.serialize();
        size_t fan=0;

        for (auto& q : peers_) if (q.handshaked && q.fd != p.fd) { 
          // send the block to all other peers
          send_frame_(q.fd, MsgType::BLOCK, pl); 
          fan++; 
        }

        if (fan) LOG("  relayed BLOCK to " << fan << " peer(s)");
      } else {
        LOG("  block rejected by chainstate");
      }
      break;
    }
    default: break; // ignore unknown
  }
}

