all:
	g++ -std=gnu++17 -O2 -pthread \
  -Isrc -I/opt/homebrew/opt/openssl@3/include \
  src/common.cpp src/crypto.cpp src/script_min.cpp src/tx.cpp \
  src/block.cpp src/chain.cpp src/p2p_minimal.cpp src/main2.cpp \
  -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto \
  -o babycoin

clean:
	rm -f babycoin

run:
	./babycoin

install-deps:
	brew install openssl