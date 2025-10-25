#pragma once
#include "chain.h"
#include <string>


bool save_chain(const ChainState& cs, const std::string& datadir);
bool load_chain(ChainState& cs, const std::string& datadir);

// Later...
// bool save_peers(const std::vector<PeerAddress>& peers, const std::string& datadir);
// bool load_peers(std::vector<PeerAddress>& peers, const std::string& datadir);
