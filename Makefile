all:
	g++ -std=gnu++17 -O2 -pthread \
  -Isrc -Ithird_party -I/opt/homebrew/opt/openssl@3/include \
  src/utils/common.cpp src/utils/crypto.cpp src/script_min.cpp src/tx.cpp \
  src/block.cpp src/chain.cpp src/p2p_minimal.cpp src/miner.cpp \
  src/wallet.cpp src/utils/storage.cpp src/utils/serialize.cpp\
  src/main.cpp \
  -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto \
  -o babycoin

clean:
	rm -f babycoin

run:
	./babycoin

install-deps:
	brew install openssl