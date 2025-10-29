#pragma once

class ChainState;
class P2PNode;

struct P2PConfig;

P2PConfig parse_command_line(int argc, char** argv);
void run_interactive_cli(ChainState& cs, P2PNode& p2p);