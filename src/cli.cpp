#include "p2p_minimal.h"
#include "miner.h"
#include "common.h"
#include <iostream>
#include <vector>
#include "wallet.h"

enum class Command {
    WALLET_CREATE,
    WALLET_LIST,
    WALLET_LOAD,
    WALLET_DELETE,
    BALANCE,
    SEND,
    MINE_BLOCK,
    STATUS,
    HELP,
    QUIT,
    UNKNOWN
};

/*
 client capabilities - things that the client should be able to do 


- execute transaction
    - 



- check balance 


^^^ we need to disconnect these functionalities from CLI 
    - make transaction -> assemble inputs, outputs, prompt signing, add to blockchain (local or global )    
*/

Command parse_command(const std::string& input) {
    if (input == "wallet" || input == "w") return Command::WALLET_CREATE;
    if (input == "list" || input == "l") return Command::WALLET_LIST;
    if (input == "delete" || input == "d") return Command::WALLET_DELETE;
    if (input == "balance" || input == "b") return Command::BALANCE;
    if (input == "send" || input == "s") return Command::SEND;
    if (input == "mine") return Command::MINE_BLOCK;
    if (input == "status") return Command::STATUS;
    if (input == "help" || input == "h") return Command::HELP;
    if (input == "quit" || input == "q") return Command::QUIT;
    return Command::UNKNOWN;
}



void execute_command(Command cmd, const std::vector<std::string>& args, ChainState& cs, P2PNode& p2p) {
    WalletStore ws;
    try {
        switch (cmd) {
            case Command::WALLET_CREATE: {
                if (args.size() < 2) {
                    std::cout << "Usage: wallet <name>\n";
                    break;
                }
                try {
                Wallet new_wallet = ws.create_wallet(args[1]);
                    std::cout << "Wallet '" + new_wallet.name + "' created successfully\n";
                    std::cout << "Address: " << WalletStore::address_hex(new_wallet.keys.pub) << "\n";
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << "\n";
                }
                break;
            }

            case Command::WALLET_DELETE: {             
                if (args.size() < 2) { std::cout << "Usage: delete <wallet_name>\n"; break; }   
                std::cout << "Are you sure you want to delete wallet '" + args[1] + "'? (y/n)\n";
                std::string confirm;
                std::cin >> confirm;
                if (confirm != "y") {
                    std::cout << "Wallet delete command '" + args[1] + "' cancelled\n";
                    break;
                }
                bool success = ws.remove_wallet(args[1]);
                if (success) {
                std::cout << "Wallet deleted '" + args[1] + "' successfully\n";
            } else {
                std::cout << "Failed to delete wallet '" + args[1] + "'\n";
            }
            break;
        }
            
            case Command::BALANCE: {
                if (args.size() < 2) {
                    std::cout << "Usage: balance <wallet_name>\n";
                    break;
                }
                try {
                Wallet wallet = ws.load_wallet(args[1]);
                    int64_t balance = cs.balance_for_pubkey(wallet.keys.pub);
                    std::cout << "Balance: " << balance << " BBTC\n";
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << "\n";
                }
                break;
            }
            case Command::WALLET_LIST:{
                std::vector<std::string> wallets = ws.list_wallets();

                std::cout << "Wallets: " << "\n";
                for (std::string& wallet : wallets) {
                    std::cout << "  " + wallet + "\n";
                }
                break;
            }
            
            case Command::SEND: {
                // the command is send <from> <to<address_hex>> <amount>
                if (args.size() < 4) {
                    std::cout << "Usage: send <from> <to_address_hex> <amount>\n";
                    break;
                }
                Transaction tx = ws.send_and_broadcast(args[1], args[2], std::stoll(args[3]), cs, p2p);
                std::cout << "Transaction sent: " << be_hex_key_(tx.txid()) << "\n";
                std::cout << "You can mine a block to see it in the blockchain\n";
                std::cout << "Usage: mine\n";
                break;  
            }
            
            case Command::MINE_BLOCK: {
                std::cout << "Mining block...\n";
                MinerConfig mcfg;
                mcfg.coinbase_pub = ws.load_wallet("miner").keys.pub;
                bool mined = mine_once(cs, mcfg, p2p);

                if (mined) {
                    std::cout << "Block mined successfully\n";
                } else {
                    std::cout << "Failed to mine block\n";
                }
                break;
            }
            
            case Command::STATUS: {
                std::cout << "Chain height: " << cs.height() << "\n";
                std::cout << "Tip hash: " << be_hex_key_(cs.tip_hash()) << "\n";
                break;
            }
            
            case Command::HELP: {
                std::cout << "Commands:\n";
                std::cout << "  wallet <name>     - Create new wallet\n";
                std::cout << "  list              - List all wallets\n";
                std::cout << "  delete <name>     - Delete wallet\n";
                std::cout << "  balance <name>    - Check wallet balance\n";
                std::cout << "  send <from> <to> <amount> - Send BTC\n";
                std::cout << "  mine             - Mine new block\n";
                std::cout << "  status           - Show chain status\n";
                std::cout << "  help             - Show this help\n";
                std::cout << "  quit             - Exit\n";
                break;
            }
            
            default:
                std::cout << "Unknown command. Type 'help' for commands.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

P2PConfig parse_command_line(int argc, char** argv) {
    P2PConfig cfg;
    // setup some config options, demo mode is on
    // cfg.demo_mode = true;
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
         } 
     }
     return cfg;
 }
 

 // Interactive CLI loop
void run_interactive_cli(ChainState& cs, P2PNode& p2p) {
    std::cout << "BABYBITCOIN INTERACTIVE WALLET\n";
    std::cout << "=============================\n";
    std::cout << "Type 'help' for commands, 'quit' to exit\n\n";
    
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        std::istringstream iss(input);
        std::vector<std::string> args;
        std::string token;
        while (iss >> token) args.push_back(token);
        
        Command cmd = parse_command(args[0]);
        if (cmd == Command::QUIT) break;
        
        execute_command(cmd, args, cs, p2p);
    }
}