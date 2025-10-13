#include "p2p_minimal.h"
#include <iostream>

P2PConfig parse_command_line(int argc, char** argv) {
    P2PConfig cfg;
    // setup some config options, demo mode is on
    cfg.demo_mode = true;
    cfg.demo_tx_to_send = 5;
 
     // starting on arg 1 bc arg 0 is program name
     for (int i=1;i<argc;i++) {
         std::string a = argv[i];
         if (a == "--port" && i+1 < argc) {
             // --port `port` - set the listen port
             cfg.listen_port = static_cast<uint16_t>(std::stoi(argv[++i]));
         } else if (a == "--connect" && i+1 < argc) {
             // --connect `host:port` - add a peer to the list
 
             std::string arg = argv[++i];
             PeerAddress peer;
             
             auto pos = arg.find(':');
 
             peer.host = arg.substr(0, pos);
             peer.port = static_cast<uint16_t>(std::stoi(arg.substr(pos+1)));
             cfg.peers.push_back(peer);
             
         } else if (a.find("help" ) != std::string::npos) {
             system("cat README.md"); 
             std::exit(0);
         } else {
             std::cerr << "Unknown arg: " << a << "\n";
         }
     }
     return cfg;
 }
 